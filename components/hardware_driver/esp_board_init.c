#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "bsp_board.h"
#include "esp_board_init.h"

esp_err_t esp_board_init(uint32_t sample_rate, int channel_format, int bits_per_chan) {
    return bsp_board_init(sample_rate, channel_format, bits_per_chan);
}

esp_err_t esp_get_feed_data(bool is_get_raw_channel, int16_t *buffer, int buffer_len) {
    return bsp_get_feed_data(is_get_raw_channel, buffer, buffer_len);
}

int esp_get_feed_channel(void) {
    return bsp_get_feed_channel();
}

char *esp_get_input_format(void) {
    return bsp_get_input_format();
}

esp_err_t esp_audio_play(const int16_t *data, int length, TickType_t ticks_to_wait) {
    return bsp_audio_play(data, length, ticks_to_wait);
}

esp_err_t esp_audio_set_play_vol(int volume) {
    return bsp_audio_set_play_vol(volume);
}

esp_err_t esp_audio_get_play_vol(int *volume) {
    return bsp_audio_get_play_vol(volume);
}

esp_err_t esp_lcd_cs_set(bool level) {
    return bsp_lcd_cs_set(level);
}

esp_err_t esp_display_init(void) {
    return bsp_display_init();
}

esp_err_t esp_display_deinit(void) {
    return bsp_display_deinit();
}

esp_err_t esp_display_backlight_init(void) {
    return bsp_display_backlight_init();
}

esp_err_t esp_display_backlight_set(int brightness_percent) {
    return bsp_display_backlight_set(brightness_percent);
}

esp_err_t esp_display_backlight_on(void) {
    return bsp_display_backlight_on();
}

esp_err_t esp_display_backlight_off(void) {
    return bsp_display_backlight_off();
}

esp_err_t esp_display_fill_color(uint16_t color) {
    return bsp_display_fill_color(color);
}

esp_err_t esp_display_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data) {
    return bsp_display_draw_bitmap(x_start, y_start, x_end, y_end, color_data);
}

int esp_display_get_h_res(void) {
    return bsp_display_get_h_res();
}

int esp_display_get_v_res(void) {
    return bsp_display_get_v_res();
}
