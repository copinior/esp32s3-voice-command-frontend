#include "espnow_bridge.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "ESPNOW_BRIDGE";
static bool s_espnow_initialized = false;
static espnow_comm_config_t s_config= {0};
static espnow_comm_rx_cb_t s_rx_callback = NULL;
static espnow_comm_send_status_cb_t s_send_status_callback = NULL;
static uint16_t s_sequence_count = 0;

static esp_err_t wifi_init(void) {
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

static esp_err_t add_peer_if_not_exist(const uint8_t *mac_addr) {
    if (esp_now_is_peer_exist(mac_addr)) return ESP_OK;

    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, mac_addr, 6);
    peer_info.channel = 0;
    peer_info.ifidx = WIFI_IF_STA;
    peer_info.encrypt = false;

    return esp_now_add_peer(&peer_info);
}

static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    ESP_LOGI(TAG, "Send to " MACSTR " status: %s", MAC2STR(mac_addr), status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");
    if (s_send_status_callback) {
        s_send_status_callback(mac_addr, status == ESP_NOW_SEND_SUCCESS);
    }
}

static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (s_rx_callback && info && data) {
        s_rx_callback(info->src_addr, data, len, info->rx_ctrl->rssi, info->rx_ctrl->channel);
    }
}

esp_err_t espnow_comm_init(const espnow_comm_config_t *cfg) {
    if (cfg == NULL) {
        ESP_LOGE(TAG, "Comfig is NULL");
        return ESP_ERR_INVALID_ARG;
    }   

    if (s_espnow_initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    memcpy(&s_config, cfg, sizeof(espnow_comm_config_t));

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = wifi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW init failed; %s", esp_err_to_name(ret));
    }

    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    if (!s_config.broadcast_default) {
        ret = add_peer_if_not_exist(s_config.default_dest_mac);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Add peer failed: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "Added peer: " MACSTR, MAC2STR(s_config.default_dest_mac));
    }

    s_espnow_initialized = true;
    ESP_LOGI(TAG, "ESp-NOW initialized successfully");
    return ESP_OK;
}

esp_err_t espnow_comm_set_rx_cb(espnow_comm_rx_cb_t cb) {
    if (!s_espnow_initialized) {
        ESP_LOGE(TAG, "ESP-NOW not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    s_rx_callback = cb;
    ESP_LOGI(TAG, "RX callback registered");
    return ESP_OK;
}

esp_err_t espnow_comm_set_send_status_cb(espnow_comm_send_status_cb_t cb) {
    if (!s_espnow_initialized) {
        ESP_LOGE(TAG, "ESP-NOW not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    s_send_status_callback = cb;
    ESP_LOGI(TAG, "Send status callback registered");
    return ESP_OK;
}

esp_err_t espnow_comm_send_voice_cmd(const espnow_voice_cmd_t *cmd, const uint8_t dest_mac[6]) {
    if(!s_espnow_initialized) {
        ESP_LOGE(TAG, "ESP-NOW not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (cmd == NULL) {
        ESP_LOGE(TAG, "Command is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t *target_mac = dest_mac ? dest_mac : s_config.default_dest_mac;

    esp_err_t ret = add_peer_if_not_exist(target_mac);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add peer");
        return ret;
    }

    espnow_voice_cmd_t packet = *cmd;
    packet.seq = s_sequence_count++;

    ret = esp_now_send(target_mac, (const uint8_t *)&packet, sizeof(espnow_voice_cmd_t));

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Send failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Send cmd_id=%d, seq=%d, prob=%d", packet.command_id, packet.seq, packet.prob_q15);
    }

    return ret;
}

esp_err_t espnow_comm_send_raw(const void *data, size_t len, const uint8_t dest_mac[6], TickType_t wait_ticks) {
    if (!s_espnow_initialized) {
        ESP_LOGE(TAG, "ESP-NOW not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid data or length");
        return ESP_ERR_INVALID_SIZE;
    }

    const uint8_t *target_mac = dest_mac ? dest_mac : s_config.default_dest_mac;

    esp_err_t ret = add_peer_if_not_exist(target_mac);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_now_send(target_mac, (const uint8_t *)data, len);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Raw send failed: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t espnow_comm_deinit(void) {
    if (!s_espnow_initialized) {
        return ESP_OK;
    }

    esp_now_unregister_send_cb();
    esp_now_unregister_recv_cb();

    esp_err_t ret = esp_now_deinit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW deinit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_wifi_stop();
    esp_wifi_deinit();

    s_espnow_initialized = false;
    s_rx_callback = NULL;
    s_send_status_callback = NULL;
    s_sequence_count = 0;
    memset(&s_config, 0, sizeof(espnow_comm_config_t));

    ESP_LOGI(TAG, "ESP-NOW deinitialized");
    return ESP_OK;
}
