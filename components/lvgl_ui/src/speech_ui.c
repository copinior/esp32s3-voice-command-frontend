#include "speech_ui.h"

#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "lvgl_display.h"
#include "ui_config.h"

static const char *TAG = "SPEECH_UI";

typedef enum {
    UI_QUEUE_EVT_COMMAND = 0,
    UI_QUEUE_EVT_LISTENING,
    UI_QUEUE_EVT_ESPNOW_STATUS,
} ui_queue_evt_type_t;

typedef struct {
    ui_queue_evt_type_t type;
    union {
        speech_ui_command_event_t command;
        bool listening;
        bool espnow_success;
    } data;
} ui_queue_evt_t;

static QueueHandle_t s_ui_queue = NULL;
static TaskHandle_t s_ui_task = NULL;

static lv_obj_t *s_state_label = NULL;
static lv_obj_t *s_cmd_label = NULL;
static lv_obj_t *s_phrase_label = NULL;
static lv_obj_t *s_meta_label = NULL;
static lv_obj_t *s_status_label = NULL;
static lv_obj_t *s_counter_label = NULL;
static lv_obj_t *s_espnow_status_label = NULL;

static uint32_t s_local_cmd_count = 0;
static uint32_t s_remote_cmd_count = 0;
static uint8_t s_espnow_consec_fail = 0;
static const uint8_t ESPNOW_FAIL_THRESHOLD = 3;

static const char *source_to_text(speech_ui_source_t source)
{
    return (source == SPEECH_UI_SOURCE_REMOTE) ? "REMOTE" : "LOCAL";
}

static const char *status_to_text(speech_ui_cmd_status_t status)
{
    switch (status) {
    case SPEECH_UI_CMD_STATUS_RECEIVED:
        return "RECEIVED";
    case SPEECH_UI_CMD_STATUS_EXECUTED:
        return "EXECUTED";
    case SPEECH_UI_CMD_STATUS_REJECTED:
        return "REJECTED";
    default:
        return "ERROR";
    }
}

static void speech_ui_create_screen(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0B132B), 0);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x1C2541), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_pad_all(screen, 12, 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xEAF1FF), 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Voice Command Monitor");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_state_label = lv_label_create(screen);
    lv_label_set_text(s_state_label, "State: Waiting wake word");
    lv_obj_align_to(s_state_label, title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

    s_cmd_label = lv_label_create(screen);
    lv_label_set_text(s_cmd_label, "Command: --");
    lv_obj_align_to(s_cmd_label, s_state_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);

    s_phrase_label = lv_label_create(screen);
    lv_label_set_text(s_phrase_label, "Phrase: --");
    lv_obj_set_width(s_phrase_label, 300);
    lv_label_set_long_mode(s_phrase_label, LV_LABEL_LONG_WRAP);
    lv_obj_align_to(s_phrase_label, s_cmd_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);

    s_meta_label = lv_label_create(screen);
    lv_label_set_text(s_meta_label, "Source: -- | Prob: -- | RSSI: --");
    lv_obj_align_to(s_meta_label, s_phrase_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);

    s_status_label = lv_label_create(screen);
    lv_label_set_text(s_status_label, "Status: IDLE");
    lv_obj_align_to(s_status_label, s_meta_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);

    s_counter_label = lv_label_create(screen);
    lv_label_set_text(s_counter_label, "Local: 0 | Remote: 0");
    lv_obj_align_to(s_counter_label, s_status_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);

    s_espnow_status_label = lv_label_create(screen);
    lv_label_set_text(s_espnow_status_label, "ESP-NOW: --");
    lv_obj_set_style_text_color(s_espnow_status_label, lv_color_hex(0xA0A0A0), 0);
    lv_obj_align_to(s_espnow_status_label, s_counter_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);
}

static void speech_ui_update_counters(const speech_ui_command_event_t *event)
{
    if (event->source == SPEECH_UI_SOURCE_REMOTE) {
        s_remote_cmd_count++;
    } else {
        s_local_cmd_count++;
    }

    lv_label_set_text_fmt(s_counter_label, "Local: %lu | Remote: %lu", (unsigned long)s_local_cmd_count,
                          (unsigned long)s_remote_cmd_count);
}

static void speech_ui_apply_command_event(const speech_ui_command_event_t *event)
{
    uint32_t prob_percent = ((uint32_t)event->prob_q15 * 100U) / 32767U;

    lv_label_set_text_fmt(s_cmd_label, "Command: #%d", event->command_id);

    if (event->phrase[0] != '\0') {
        lv_label_set_text_fmt(s_phrase_label, "Phrase: %s", event->phrase);
    } else {
        lv_label_set_text(s_phrase_label, "Phrase: --");
    }

    lv_label_set_text_fmt(s_meta_label, "Source: %s | Prob: %lu%% | RSSI: %d CH:%u", source_to_text(event->source),
                          (unsigned long)prob_percent, event->rssi, event->channel);

    lv_label_set_text_fmt(s_status_label, "Status: %s", status_to_text(event->status));

    if (event->status != SPEECH_UI_CMD_STATUS_RECEIVED) {
        speech_ui_update_counters(event);
    }
}

static void speech_ui_apply_listening_state(bool listening)
{
    lv_label_set_text(s_state_label, listening ? "State: Listening command" : "State: Waiting wake word");
}

static void speech_ui_apply_espnow_status(bool success)
{
    if (success) {
        s_espnow_consec_fail = 0;
        lv_label_set_text(s_espnow_status_label, "ESP-NOW: OK");
        lv_obj_set_style_text_color(s_espnow_status_label, lv_color_hex(0x00E676), 0);
    } else {
        if (s_espnow_consec_fail < 255) {
            s_espnow_consec_fail++;
        }
        if (s_espnow_consec_fail >= ESPNOW_FAIL_THRESHOLD) {
            lv_label_set_text_fmt(s_espnow_status_label, "ESP-NOW: FAIL(%u)", s_espnow_consec_fail);
            lv_obj_set_style_text_color(s_espnow_status_label, lv_color_hex(0xFF1744), 0);
        } else {
            lv_label_set_text_fmt(s_espnow_status_label, "ESP-NOW: RETRY(%u/%u)", s_espnow_consec_fail, ESPNOW_FAIL_THRESHOLD);
            lv_obj_set_style_text_color(s_espnow_status_label, lv_color_hex(0xFFAB00), 0);
        }
    }
}

static void speech_ui_task(void *arg)
{
    (void)arg;

    while (true) {
        ui_queue_evt_t evt = {0};
        BaseType_t queue_ret = xQueueReceive(s_ui_queue, &evt, pdMS_TO_TICKS(UI_TASK_LOOP_DELAY_MS));

        if (queue_ret == pdPASS && lvgl_display_lock(20)) {
            if (evt.type == UI_QUEUE_EVT_COMMAND) {
                speech_ui_apply_command_event(&evt.data.command);
            } else if (evt.type == UI_QUEUE_EVT_LISTENING) {
                speech_ui_apply_listening_state(evt.data.listening);
            } else if (evt.type == UI_QUEUE_EVT_ESPNOW_STATUS) {
                speech_ui_apply_espnow_status(evt.data.espnow_success);
            }
            lvgl_display_unlock();
        }

        if (lvgl_display_lock(20)) {
            lvgl_display_timer_handler();
            lvgl_display_unlock();
        }
    }
}

esp_err_t speech_ui_start(void)
{
    if (s_ui_task != NULL) {
        return ESP_OK;
    }

    esp_err_t ret = lvgl_display_init();
    ESP_RETURN_ON_ERROR(ret, TAG, "lvgl display init failed");

    s_ui_queue = xQueueCreate(UI_EVENT_QUEUE_LEN, sizeof(ui_queue_evt_t));
    if (s_ui_queue == NULL) {
        lvgl_display_deinit();
        return ESP_ERR_NO_MEM;
    }

    if (!lvgl_display_lock(100)) {
        vQueueDelete(s_ui_queue);
        s_ui_queue = NULL;
        lvgl_display_deinit();
        return ESP_ERR_TIMEOUT;
    }
    speech_ui_create_screen();
    lvgl_display_unlock();

    BaseType_t task_ret = xTaskCreatePinnedToCore(speech_ui_task, "speech_ui", UI_TASK_STACK_SIZE, NULL,
                                                   UI_TASK_PRIORITY, &s_ui_task, UI_TASK_CORE_ID);
    if (task_ret != pdPASS) {
        vQueueDelete(s_ui_queue);
        s_ui_queue = NULL;
        lvgl_display_deinit();
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Speech UI started");
    return ESP_OK;
}

esp_err_t speech_ui_stop(void)
{
    if (s_ui_task != NULL) {
        vTaskDelete(s_ui_task);
        s_ui_task = NULL;
    }

    if (s_ui_queue != NULL) {
        vQueueDelete(s_ui_queue);
        s_ui_queue = NULL;
    }

    lvgl_display_deinit();
    return ESP_OK;
}

esp_err_t speech_ui_post_command_event(const speech_ui_command_event_t *event)
{
    ESP_RETURN_ON_FALSE(event != NULL, ESP_ERR_INVALID_ARG, TAG, "event is null");
    ESP_RETURN_ON_FALSE(s_ui_queue != NULL, ESP_ERR_INVALID_STATE, TAG, "ui queue not ready");

    ui_queue_evt_t ui_evt = {
        .type = UI_QUEUE_EVT_COMMAND,
    };
    memcpy(&ui_evt.data.command, event, sizeof(speech_ui_command_event_t));

    if (xQueueSend(s_ui_queue, &ui_evt, 0) != pdPASS) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t speech_ui_post_listening_state(bool listening)
{
    ESP_RETURN_ON_FALSE(s_ui_queue != NULL, ESP_ERR_INVALID_STATE, TAG, "ui queue not ready");

    ui_queue_evt_t ui_evt = {
        .type = UI_QUEUE_EVT_LISTENING,
    };
    ui_evt.data.listening = listening;

    if (xQueueSend(s_ui_queue, &ui_evt, 0) != pdPASS) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t speech_ui_post_espnow_status(bool success)
{
    ESP_RETURN_ON_FALSE(s_ui_queue != NULL, ESP_ERR_INVALID_STATE, TAG, "ui queue not ready");

    ui_queue_evt_t ui_evt = {
        .type = UI_QUEUE_EVT_ESPNOW_STATUS,
    };
    ui_evt.data.espnow_success = success;

    if (xQueueSend(s_ui_queue, &ui_evt, 0) != pdPASS) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}
