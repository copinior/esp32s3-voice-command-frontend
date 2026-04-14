#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPEECH_UI_PHRASE_MAX_LEN        (48)

typedef enum {
    SPEECH_UI_SOURCE_LOCAL = 0,
    SPEECH_UI_SOURCE_REMOTE = 1,
} speech_ui_source_t;

typedef enum {
    SPEECH_UI_CMD_STATUS_RECEIVED = 0,
    SPEECH_UI_CMD_STATUS_EXECUTED,
    SPEECH_UI_CMD_STATUS_REJECTED,
    SPEECH_UI_CMD_STATUS_ERROR,
} speech_ui_cmd_status_t;

typedef struct {
    speech_ui_source_t source;
    speech_ui_cmd_status_t status;
    int command_id;
    uint16_t prob_q15;
    int8_t rssi;
    uint8_t channel;
    char phrase[SPEECH_UI_PHRASE_MAX_LEN];
} speech_ui_command_event_t;

esp_err_t speech_ui_start(void);
esp_err_t speech_ui_stop(void);

esp_err_t speech_ui_post_command_event(const speech_ui_command_event_t *event);
esp_err_t speech_ui_post_listening_state(bool listening);
esp_err_t speech_ui_post_espnow_status(bool success);

#ifdef __cplusplus
}
#endif
