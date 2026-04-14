#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_display_init(void);
esp_err_t bsp_display_deinit(void);

esp_err_t bsp_display_backlight_init(void);
esp_err_t bsp_display_backlight_set(int brightness_percent);
esp_err_t bsp_display_backlight_on(void);
esp_err_t bsp_display_backlight_off(void);

esp_err_t bsp_display_fill_color(uint16_t color);
esp_err_t bsp_display_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data);

int bsp_display_get_h_res(void);
int bsp_display_get_v_res(void);

#ifdef __cplusplus
}
#endif
