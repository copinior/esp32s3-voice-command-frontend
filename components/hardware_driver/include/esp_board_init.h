#ifndef __ESP_BOARD_INIT_H
#define __ESP_BOARD_INIT_H

#include "esp_custom_board.h"
#include "bsp_board.h"

esp_err_t esp_board_init(uint32_t sample_rate, int channel_format, int bits_per_chan);
esp_err_t esp_get_feed_data(bool is_get_raw_channel, int16_t *buffer, int buffer_len);
int esp_get_feed_channel(void);
char *esp_get_input_format(void);
esp_err_t esp_audio_play(const int16_t *data, int length, TickType_t ticks_to_wait);
esp_err_t esp_audio_set_play_vol(int volume);
esp_err_t esp_audio_get_play_vol(int *volume);
esp_err_t esp_lcd_cs_set(bool level);

esp_err_t esp_display_init(void);
esp_err_t esp_display_deinit(void);
esp_err_t esp_display_backlight_init(void);
esp_err_t esp_display_backlight_set(int brightness_percent);
esp_err_t esp_display_backlight_on(void);
esp_err_t esp_display_backlight_off(void);
esp_err_t esp_display_fill_color(uint16_t color);
esp_err_t esp_display_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data);
int esp_display_get_h_res(void);
int esp_display_get_v_res(void);

#endif
