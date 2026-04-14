#include "lcd_display.h"

#include "esp_board_init.h"
#include "esp_log.h"

static const char *TAG = "LCD_DISPLAY";

esp_err_t lcd_display_init(void)
{
    return esp_display_init();
}

esp_err_t lcd_display_deinit(void)
{
    return esp_display_deinit();
}

esp_err_t lcd_display_backlight_init(void)
{
    return esp_display_backlight_init();
}

esp_err_t lcd_display_backlight_set(int brightness_percent)
{
    return esp_display_backlight_set(brightness_percent);
}

esp_err_t lcd_display_backlight_on(void)
{
    return esp_display_backlight_on();
}

esp_err_t lcd_display_backlight_off(void)
{
    return esp_display_backlight_off();
}

esp_err_t lcd_display_fill_color(uint16_t color)
{
    return esp_display_fill_color(color);
}

esp_err_t lcd_display_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    return esp_display_draw_bitmap(x_start, y_start, x_end, y_end, color_data);
}

void bsp_lvgl_start(void)
{
    esp_err_t ret = lcd_display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "lcd display init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = lcd_display_backlight_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "lcd backlight init failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = lcd_display_backlight_on();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "lcd backlight on failed: %s", esp_err_to_name(ret));
    }
}
