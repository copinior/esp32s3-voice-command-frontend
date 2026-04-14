#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t lcd_display_init(void);
esp_err_t lcd_display_deinit(void);

esp_err_t lcd_display_backlight_init(void);
esp_err_t lcd_display_backlight_set(int brightness_percent);
esp_err_t lcd_display_backlight_on(void);
esp_err_t lcd_display_backlight_off(void);

esp_err_t lcd_display_fill_color(uint16_t color);
esp_err_t lcd_display_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data);

void bsp_lvgl_start(void);
