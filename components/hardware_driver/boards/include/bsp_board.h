#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "bsp_display.h"

#if CONFIG_ESP_CUSTOM_BOARD
#include "esp_custom_board.h"
#else
#error "No supported board selected"
#endif

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_board_init(uint32_t sample_rate, int channel_format, int bits_per_chan);
esp_err_t bsp_sdcard_init(char *mount_point, size_t max_files);
esp_err_t bsp_sdcard_deinit(char *mount_point);

esp_err_t bsp_audio_play(const int16_t *data, int length, TickType_t ticks_to_wait);
esp_err_t bsp_get_feed_data(bool is_get_raw_channel, int16_t *buffer, int buffer_len);

int bsp_get_feed_channel(void);
char *bsp_get_input_format(void);

esp_err_t bsp_audio_set_play_vol(int volume);
esp_err_t bsp_audio_get_play_vol(int *volume);

esp_err_t bsp_lcd_cs_set(bool level);


#ifdef __cplusplus
}
#endif
