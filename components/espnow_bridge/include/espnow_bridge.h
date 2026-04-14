#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_now.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool broadcast_default;
    uint8_t default_dest_mac[6];      // broadcast_default=false时使用
    uint8_t retransmit_count;         // 建议3~8
    bool ack_enable;                  // 单播建议true
    bool sec_enable;                  // 先false
    TickType_t send_wait_ticks;       // 如pdMS_TO_TICKS(200)
} espnow_comm_config_t;

typedef struct __attribute__((packed)) {
    uint8_t var;
    uint8_t msg_type;
    uint16_t seq;
    int16_t command_id;
    uint16_t prob_q15;          // 0~32767
    uint32_t ts_ms;
} espnow_voice_cmd_t;

#define ESPNOW_MSG_TYPE_VOICE_CMD   0x01
#define ESPNOW_MSG_TYPE_HEARTBEAT   0x02

typedef void (*espnow_comm_rx_cb_t)(
    const uint8_t src_mac[6],
    const void *data,
    size_t len,
    int8_t rssi,
    uint8_t channel);

typedef void (*espnow_comm_send_status_cb_t)(const uint8_t dest_mac[6], bool success);

esp_err_t espnow_comm_init(const espnow_comm_config_t *cfg);
esp_err_t espnow_comm_deinit(void);

esp_err_t espnow_comm_set_rx_cb(espnow_comm_rx_cb_t cb);
esp_err_t espnow_comm_set_send_status_cb(espnow_comm_send_status_cb_t cb);

esp_err_t espnow_comm_send_voice_cmd(const espnow_voice_cmd_t *cmd, const uint8_t dest_mac[6]);
esp_err_t espnow_comm_send_raw(const void *data, size_t len, const uint8_t dest_mac[6], TickType_t wait_ticks);

#ifdef __cplusplus
}
#endif
