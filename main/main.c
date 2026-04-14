/*
 * Application entry for the ESP32-S3 voice command frontend.
 *
 * This file contains the project-specific speech recognition pipeline,
 * UI event flow, and ESP-NOW forwarding logic used by this repository.
*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_afe_config.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_process_sdkconfig.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "espnow_bridge.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "model_path.h"
#include "speech_commands_action.h"
#include "speech_ui.h"
#include "esp_board_init.h"

static const char *TAG_MAIN = "MAIN";

#define APP_CMD_QUEUE_LEN                (24)
#define APP_AUDIO_QUEUE_LEN              (12)
#define APP_ESPNOW_CMD_MSG_TYPE          (0x01)
#define APP_ESPNOW_HEARTBEAT_MSG_TYPE    (0x02)
#define APP_ESPNOW_HEARTBEAT_INTERVAL_MS (5000)
#define APP_REMOTE_CONF_THRESHOLD_Q15    (16384U)
#define APP_COMMAND_ID_MIN               (0)
#define APP_COMMAND_ID_MAX               (15)
#define APP_COMMAND_COUNT                (16)
#define APP_AFE_RINGBUF_FRAMES           (80)
#define APP_WAKE_SESSION_MS              (30000)

#define APP_TASK_CORE_AUDIO              (0)
#define APP_TASK_CORE_DISPATCH           (0)
#define APP_TASK_CORE_FEED               (0)
#define APP_TASK_CORE_DETECT             (1)
#define APP_TASK_PRIO_AUDIO              (4)
#define APP_TASK_PRIO_DISPATCH           (5)
#define APP_TASK_PRIO_FEED               (7)
#define APP_TASK_PRIO_DETECT             (8)
#define APP_TASK_STACK_AUDIO             (3 * 1024)
#define APP_TASK_STACK_DISPATCH          (3 * 1024)
#define APP_TASK_STACK_FEED              (4 * 1024)
#define APP_TASK_STACK_DETECT            (6 * 1024)

typedef enum {
    APP_CMD_SOURCE_LOCAL_SR = 0,
    APP_CMD_SOURCE_REMOTE_ESPNOW = 1,
} app_cmd_source_t;

typedef struct {
    app_cmd_source_t source;
    espnow_voice_cmd_t cmd;
    int8_t rssi;
    uint8_t channel;
    char phrase[SPEECH_UI_PHRASE_MAX_LEN];
} app_cmd_event_t;

int wakeup_flag = 0;
static const esp_afe_sr_iface_t *afe_handle = NULL;
static volatile int task_flag = 0;
static volatile bool s_detect_ready = false;
static srmodel_list_t *models = NULL;
static QueueHandle_t s_cmd_queue = NULL;
static QueueHandle_t s_audio_queue = NULL;

static const uint16_t APP_CMD_THR_Q15[APP_COMMAND_COUNT] = {
    8000, 8000, 7000, 7500, 7000, 8000, 14000, 17000,
    5000, 4500, 10000, 11000, 12000, 9000, 5500, 4500
};

static inline uint16_t app_get_cmd_threshold_q15(int cmd_id) {
    if (cmd_id < 0 || cmd_id >= APP_COMMAND_COUNT) {
        return 32767U;  //非法命令默认拒绝
    }
    return APP_CMD_THR_Q15[cmd_id];
}
 
static void app_log_internal_heap(const char *stage)
{
    ESP_LOGI(TAG_MAIN, "heap(%s): internal_free=%lu largest=%lu min=%lu", stage,
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned long)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
}

static void app_create_task_or_abort(TaskFunction_t task_func, const char *name, uint32_t stack_bytes, void *arg,
                                     UBaseType_t prio, BaseType_t core_id)
{
    app_log_internal_heap(name);
    BaseType_t ok = xTaskCreatePinnedToCore(task_func, name, stack_bytes, arg, prio, NULL, core_id);
    if (ok != pdPASS) {
        ESP_LOGE(TAG_MAIN, "Create task failed: %s stack=%lu prio=%u core=%ld", name, (unsigned long)stack_bytes,
                 (unsigned int)prio, (long)core_id);
        abort();
    }
}

static inline bool app_is_valid_command_id(int cmd_id)
{
    return cmd_id >= APP_COMMAND_ID_MIN && cmd_id <= APP_COMMAND_ID_MAX;
}

static bool app_parse_mac_string(const char *mac_str, uint8_t out_mac[6])
{
    if (mac_str == NULL || out_mac == NULL) {
        return false;
    }

    unsigned int parsed[6] = {0};
    int matched = sscanf(mac_str, "%2x:%2x:%2x:%2x:%2x:%2x",
                         &parsed[0], &parsed[1], &parsed[2],
                         &parsed[3], &parsed[4], &parsed[5]);
    if (matched != 6) {
        return false;
    }

    for (size_t i = 0; i < 6; ++i) {
        out_mac[i] = (uint8_t)parsed[i];
    }
    return true;
}

static bool app_is_broadcast_mac(const uint8_t mac[6])
{
    static const uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    return mac != NULL && memcmp(mac, broadcast_mac, sizeof(broadcast_mac)) == 0;
}

static uint16_t app_prob_to_q15(float prob)
{
    if (prob <= 0.0f) {
        return 0;
    }
    if (prob >= 1.0f) {
        return 32767U;
    }
    return (uint16_t)(prob * 32767.0f);
}

static void app_copy_phrase(char *dst, size_t dst_size, const char *src)
{
    if (dst_size == 0) {
        return;
    }

    if (src == NULL || src[0] == '\0') {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s", src);
}

static void app_post_ui_command_event(const app_cmd_event_t *evt, speech_ui_cmd_status_t status)
{
    speech_ui_command_event_t ui_evt = {
        .source = (evt->source == APP_CMD_SOURCE_REMOTE_ESPNOW) ? SPEECH_UI_SOURCE_REMOTE : SPEECH_UI_SOURCE_LOCAL,
        .status = status,
        .command_id = evt->cmd.command_id,
        .prob_q15 = evt->cmd.prob_q15,
        .rssi = evt->rssi,
        .channel = evt->channel,
    };
    app_copy_phrase(ui_evt.phrase, sizeof(ui_evt.phrase), evt->phrase);

    esp_err_t ret = speech_ui_post_command_event(&ui_evt);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG_MAIN, "UI event dropped: %s", esp_err_to_name(ret));
    }
}

static void app_enqueue_command_event(const app_cmd_event_t *evt)
{
    if (s_cmd_queue == NULL) {
        return;
    }

    if (xQueueSend(s_cmd_queue, evt, 0) != pdPASS) {
        ESP_LOGW(TAG_MAIN, "Command queue full, dropping source=%d cmd=%d", evt->source, evt->cmd.command_id);
    }
}

static void app_enqueue_audio_event(const app_cmd_event_t *evt)
{
    if (s_audio_queue == NULL) {
        return;
    }

    if (xQueueSend(s_audio_queue, evt, 0) != pdPASS) {
        ESP_LOGW(TAG_MAIN, "Audio queue full, drop cmd=%d src=%d", evt->cmd.command_id, evt->source);
        app_post_ui_command_event(evt, SPEECH_UI_CMD_STATUS_ERROR);
    }
}

void espnow_rx_handler(const uint8_t src_mac[6], const void *data, size_t len, int8_t rssi, uint8_t channel)
{
    ESP_LOGI(TAG_MAIN, "RX from " MACSTR ", RSSI=%d CH=%d len=%d", MAC2STR(src_mac), rssi, channel, (int)len);

    if (len != sizeof(espnow_voice_cmd_t) || data == NULL) {
        ESP_LOGW(TAG_MAIN, "Unknown packet format, len=%d", (int)len);
        return;
    }

    const espnow_voice_cmd_t *cmd = (const espnow_voice_cmd_t *)data;
    ESP_LOGI(TAG_MAIN, "RX cmd: msg_type=0x%02X id=%d prob_q15=%u seq=%u ts_ms=%lu",
             cmd->msg_type, cmd->command_id, cmd->prob_q15, cmd->seq, (unsigned long)cmd->ts_ms);
    app_cmd_event_t evt = {
        .source = APP_CMD_SOURCE_REMOTE_ESPNOW,
        .cmd = *cmd,
        .rssi = rssi,
        .channel = channel,
    };

    app_enqueue_command_event(&evt);
}

void espnow_send_status_handler(const uint8_t dest_mac[6], bool success)
{
    ESP_LOGI(TAG_MAIN, "ESPNOW send to " MACSTR " %s", MAC2STR(dest_mac), success ? "OK" : "FAIL");
    speech_ui_post_espnow_status(success);
}

static void app_heartbeat_timer_cb(void *arg)
{
    (void)arg;
    espnow_voice_cmd_t hb = {
        .var = 1,
        .msg_type = APP_ESPNOW_HEARTBEAT_MSG_TYPE,
        .seq = 0,
        .command_id = -1,
        .prob_q15 = 0,
        .ts_ms = (uint32_t)(esp_timer_get_time() / 1000),
    };
    esp_err_t ret = espnow_comm_send_voice_cmd(&hb, NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG_MAIN, "Heartbeat send failed: %s", esp_err_to_name(ret));
    }
}

static void command_dispatch_task(void *arg)
{
    (void)arg;

    while (task_flag) {
        app_cmd_event_t evt = {0};
        if (xQueueReceive(s_cmd_queue, &evt, pdMS_TO_TICKS(100)) != pdPASS) {
            continue;
        }

        bool is_remote = (evt.source == APP_CMD_SOURCE_REMOTE_ESPNOW);
        ESP_LOGI(TAG_MAIN, "Dispatch cmd: src=%s id=%d prob_q15=%u seq=%u ts_ms=%lu",
                 is_remote ? "remote" : "local", evt.cmd.command_id, evt.cmd.prob_q15,
                 evt.cmd.seq, (unsigned long)evt.cmd.ts_ms);

        if (is_remote && evt.cmd.msg_type != APP_ESPNOW_CMD_MSG_TYPE) {
            ESP_LOGW(TAG_MAIN, "Drop remote cmd: unsupported msg_type=0x%02X", evt.cmd.msg_type);
            app_post_ui_command_event(&evt, SPEECH_UI_CMD_STATUS_ERROR);
            continue;
        }

        if (!app_is_valid_command_id(evt.cmd.command_id)) {
            ESP_LOGW(TAG_MAIN, "Drop cmd: invalid command_id=%d", evt.cmd.command_id);
            app_post_ui_command_event(&evt, SPEECH_UI_CMD_STATUS_REJECTED);
            continue;
        }

        if (!is_remote) {
            uint16_t thr = app_get_cmd_threshold_q15(evt.cmd.command_id);
            if (evt.cmd.prob_q15 < thr) {
                ESP_LOGW(TAG_MAIN, "Reject local cmd=%d low confidence=%u", evt.cmd.command_id, evt.cmd.prob_q15);
                app_post_ui_command_event(&evt, SPEECH_UI_CMD_STATUS_REJECTED);
                continue;
            } 
        } else if (evt.cmd.prob_q15 < APP_REMOTE_CONF_THRESHOLD_Q15) {
            ESP_LOGW(TAG_MAIN, "Reject remote cmd=%d low confidence=%u", evt.cmd.command_id, evt.cmd.prob_q15);
            app_post_ui_command_event(&evt, SPEECH_UI_CMD_STATUS_REJECTED);
            continue;
        }
        

        app_post_ui_command_event(&evt, SPEECH_UI_CMD_STATUS_RECEIVED);
        if (evt.source == APP_CMD_SOURCE_LOCAL_SR) {
            ESP_LOGI(TAG_MAIN, "Forward local cmd to ESPNOW: id=%d prob_q15=%u", evt.cmd.command_id, evt.cmd.prob_q15);
            esp_err_t send_ret = espnow_comm_send_voice_cmd(&evt.cmd, NULL);
            if (send_ret != ESP_OK) {
                ESP_LOGE(TAG_MAIN, "ESP-NOW send failed: %s", esp_err_to_name(send_ret));
            }
        }
        app_enqueue_audio_event(&evt);
    }

    vTaskDelete(NULL);
}

static void audio_exec_task(void *arg)
{
    (void)arg;

    while (task_flag) {
        app_cmd_event_t evt = {0};
        if (xQueueReceive(s_audio_queue, &evt, pdMS_TO_TICKS(100)) != pdPASS) {
            continue;
        }

        ESP_LOGI(TAG_MAIN, "Execute cmd: src=%s id=%d", evt.source == APP_CMD_SOURCE_REMOTE_ESPNOW ? "remote" : "local",
                 evt.cmd.command_id);
        speech_commands_action(evt.cmd.command_id);
        app_post_ui_command_event(&evt, SPEECH_UI_CMD_STATUS_EXECUTED);
    }

    vTaskDelete(NULL);
}

void feed_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *)arg;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int nch = afe_handle->get_feed_channel_num(afe_data);
    int feed_channel = esp_get_feed_channel();
    assert(nch == feed_channel);
    ESP_LOGI(TAG_MAIN, "feed task started, chunk=%d ch=%d", audio_chunksize, feed_channel);

    int16_t *i2s_buff = malloc(audio_chunksize * sizeof(int16_t) * feed_channel);
    assert(i2s_buff);

    while (task_flag && !s_detect_ready) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGI(TAG_MAIN, "feed task enters running state");

    while (task_flag) {
        esp_err_t ret =
            esp_get_feed_data(true, i2s_buff, audio_chunksize * (int)sizeof(int16_t) * feed_channel);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG_MAIN, "feed read failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }
        afe_handle->feed(afe_data, i2s_buff);
    }

    free(i2s_buff);
    vTaskDelete(NULL);
}

void detect_Task(void *arg)
{
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *)arg;
    int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);
    int64_t session_deadline_us = 0;
    bool wakenet_disabled = false;

    char *mn_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_CHINESE);
    ESP_LOGI(TAG_MAIN, "multinet: %s", mn_name);

    esp_mn_iface_t *multinet = esp_mn_handle_from_name(mn_name);
    model_iface_data_t *model_data = multinet->create(mn_name, 6000);
    esp_mn_commands_update_from_sdkconfig(multinet, model_data);

    int mu_chunksize = multinet->get_samp_chunksize(model_data);
    assert(mu_chunksize == afe_chunksize);

    multinet->print_active_speech_commands(model_data);
    s_detect_ready = true;
    ESP_LOGI(TAG_MAIN, "detect task started");

    while (task_flag) {
        afe_fetch_result_t *res = NULL;
        if (afe_handle->fetch_with_delay != NULL) {
            res = afe_handle->fetch_with_delay(afe_data, pdMS_TO_TICKS(50));
        } else {
            res = afe_handle->fetch(afe_data);
        }
        if (res == NULL) {
            ESP_LOGW(TAG_MAIN, "AFE fetch returned NULL");
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }

        if (res->ret_value == ESP_FAIL) {
            ESP_LOGW(TAG_MAIN, "AFE fetch timeout/fail, keep waiting feed data");
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        if (wakeup_flag == 1 && session_deadline_us > 0 && esp_timer_get_time() >= session_deadline_us) {
            if (wakenet_disabled && afe_handle->enable_wakenet != NULL) {
                afe_handle->enable_wakenet(afe_data);
                wakenet_disabled = false;
            }
            wakeup_flag = 0;
            session_deadline_us = 0;
            multinet->clean(model_data);
            speech_ui_post_listening_state(false);
            ESP_LOGI(TAG_MAIN, "Wake session timeout, waiting wake word");
            continue;
        }

        if (res->ringbuff_free_pct < 0.5f) {
            static int64_t s_last_rb_warn_us = 0;
            int64_t now_us = esp_timer_get_time();
            if ((now_us - s_last_rb_warn_us) > 1000000) {
                ESP_LOGW(TAG_MAIN, "AFE ringbuffer low free space, free_pct=%.2f", res->ringbuff_free_pct);
                s_last_rb_warn_us = now_us;
            }
        }

        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG_MAIN, "Wake word detected");
            multinet->clean(model_data);
            wakeup_flag = 1;
            session_deadline_us = esp_timer_get_time() + (int64_t)APP_WAKE_SESSION_MS * 1000;
            if (!wakenet_disabled && afe_handle->disable_wakenet != NULL) {
                afe_handle->disable_wakenet(afe_data);
                wakenet_disabled = true;
            }
            speech_ui_post_listening_state(true);
        } else if (res->raw_data_channels > 1 && res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
            ESP_LOGI(TAG_MAIN, "Wake channel verified: %d", res->trigger_channel_id);
            wakeup_flag = 1;
            session_deadline_us = esp_timer_get_time() + (int64_t)APP_WAKE_SESSION_MS * 1000;
            if (!wakenet_disabled && afe_handle->disable_wakenet != NULL) {
                afe_handle->disable_wakenet(afe_data);
                wakenet_disabled = true;
            }
            speech_ui_post_listening_state(true);
        }

        if (wakeup_flag != 1) {
            continue;
        }

        esp_mn_state_t mn_state = multinet->detect(model_data, res->data);
        if (mn_state == ESP_MN_STATE_DETECTING) {
            continue;
        }

        if (mn_state == ESP_MN_STATE_DETECTED) {
            esp_mn_results_t *mn_result = multinet->get_results(model_data);
            if (mn_result != NULL && mn_result->num > 0) {
                int cmd_id = mn_result->command_id[0];
                float prob = mn_result->prob[0];

                app_cmd_event_t evt = {
                    .source = APP_CMD_SOURCE_LOCAL_SR,
                    .cmd = {
                        .var = 1,
                        .msg_type = APP_ESPNOW_CMD_MSG_TYPE,
                        .seq = 0,
                        .command_id = cmd_id,
                        .prob_q15 = app_prob_to_q15(prob),
                        .ts_ms = (uint32_t)(esp_timer_get_time() / 1000),
                    },
                    .rssi = 0,
                    .channel = 0,
                };

                app_copy_phrase(evt.phrase, sizeof(evt.phrase), mn_result->string);
                ESP_LOGI(TAG_MAIN, "Local SR detected: id=%d prob=%.3f phrase=%s",
                         cmd_id, (double)prob, evt.phrase[0] ? evt.phrase : "--");
                app_enqueue_command_event(&evt);
            }
            multinet->clean(model_data);
            session_deadline_us = esp_timer_get_time() + (int64_t)APP_WAKE_SESSION_MS * 1000;
        } else if (mn_state == ESP_MN_STATE_TIMEOUT) {
            multinet->clean(model_data);
            if (session_deadline_us > 0 && esp_timer_get_time() < session_deadline_us) {
                continue;
            }
            if (wakenet_disabled && afe_handle->enable_wakenet != NULL) {
                afe_handle->enable_wakenet(afe_data);
                wakenet_disabled = false;
            }
            wakeup_flag = 0;
            session_deadline_us = 0;
            speech_ui_post_listening_state(false);
            ESP_LOGI(TAG_MAIN, "Speech command timeout, waiting wake word");
        }
    }

    if (model_data != NULL) {
        multinet->destroy(model_data);
    }
    if (wakenet_disabled && afe_handle->enable_wakenet != NULL) {
        afe_handle->enable_wakenet(afe_data);
    }

    s_detect_ready = false;
    ESP_LOGI(TAG_MAIN, "detect task exit");
    vTaskDelete(NULL);
}

void app_main(void)
{
    uint8_t default_dest_mac[6] = {0};
    if (!app_parse_mac_string(CONFIG_APP_ESPNOW_DEFAULT_DEST_MAC, default_dest_mac)) {
        ESP_LOGE(TAG_MAIN, "Invalid CONFIG_APP_ESPNOW_DEFAULT_DEST_MAC: %s", CONFIG_APP_ESPNOW_DEFAULT_DEST_MAC);
        return;
    }

    if (app_is_broadcast_mac(default_dest_mac)) {
        ESP_LOGW(TAG_MAIN, "Using broadcast ESP-NOW destination MAC. Set CONFIG_APP_ESPNOW_DEFAULT_DEST_MAC for deployment.");
    }

    models = esp_srmodel_init("model");
    ESP_ERROR_CHECK(esp_board_init(16000, 1, 16));

    s_cmd_queue = xQueueCreate(APP_CMD_QUEUE_LEN, sizeof(app_cmd_event_t));
    if (s_cmd_queue == NULL) {
        ESP_LOGE(TAG_MAIN, "Failed to create command queue");
        return;
    }

    s_audio_queue = xQueueCreate(APP_AUDIO_QUEUE_LEN, sizeof(app_cmd_event_t));
    if (s_audio_queue == NULL) {
        ESP_LOGE(TAG_MAIN, "Failed to create audio queue");
        return;
    }

    ESP_ERROR_CHECK(speech_ui_start());
    ESP_ERROR_CHECK(speech_ui_post_listening_state(false));

    afe_config_t *afe_config = afe_config_init(esp_get_input_format(), models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    afe_config->afe_ringbuf_size = APP_AFE_RINGBUF_FRAMES;
    afe_handle = esp_afe_handle_from_config(afe_config);
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(afe_config);
    afe_config_free(afe_config);

    espnow_comm_config_t espnow_config = {
        .retransmit_count = 5,
        .broadcast_default = false,
        .default_dest_mac = {0},
        .ack_enable = true,
        .sec_enable = false,
        .send_wait_ticks = pdMS_TO_TICKS(1000),
    };
    memcpy(espnow_config.default_dest_mac, default_dest_mac, sizeof(default_dest_mac));

    ESP_ERROR_CHECK(espnow_comm_init(&espnow_config));
    ESP_ERROR_CHECK(espnow_comm_set_rx_cb(espnow_rx_handler));
    ESP_ERROR_CHECK(espnow_comm_set_send_status_cb(espnow_send_status_handler));

    const esp_timer_create_args_t hb_timer_args = {
        .callback = app_heartbeat_timer_cb,
        .name = "espnow_hb",
    };
    esp_timer_handle_t hb_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&hb_timer_args, &hb_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(hb_timer, (uint64_t)APP_ESPNOW_HEARTBEAT_INTERVAL_MS * 1000));

    task_flag = 1;
    s_detect_ready = false;
    app_log_internal_heap("before_tasks");
    app_create_task_or_abort(audio_exec_task, "audio_exec", APP_TASK_STACK_AUDIO, NULL, APP_TASK_PRIO_AUDIO, APP_TASK_CORE_AUDIO);
    app_create_task_or_abort(command_dispatch_task, "cmd_dispatch", APP_TASK_STACK_DISPATCH, NULL, APP_TASK_PRIO_DISPATCH,
                             APP_TASK_CORE_DISPATCH);
    app_create_task_or_abort(&feed_Task, "feed", APP_TASK_STACK_FEED, (void *)afe_data, APP_TASK_PRIO_FEED,
                             APP_TASK_CORE_FEED);
    app_create_task_or_abort(&detect_Task, "detect", APP_TASK_STACK_DETECT, (void *)afe_data, APP_TASK_PRIO_DETECT,
                             APP_TASK_CORE_DETECT);
}
