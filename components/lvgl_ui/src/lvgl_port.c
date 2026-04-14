
#include "lvgl_port.h"

#include <stdlib.h>

#include "display_base.h"
#include "esp_board_init.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lcd_display.h"
#include "lvgl.h"
#include "ui_config.h"

static const char *TAG = "LVGL_PORT";

static lv_display_t *s_display = NULL;
static void *s_buf_1 = NULL;
static esp_timer_handle_t s_tick_timer = NULL;
static bool s_initialized = false;

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(UI_LVGL_TICK_PERIOD_MS);
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *color_map)
{
    (void)disp;
    esp_err_t ret = lcd_display_draw_bitmap(area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LCD flush failed: %s", esp_err_to_name(ret));
    }
    lv_display_flush_ready(disp);
}

esp_err_t lvgl_port_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(display_base_init(), TAG, "display base init failed");
    ESP_RETURN_ON_ERROR(lcd_display_init(), TAG, "lcd init failed");
    ESP_RETURN_ON_ERROR(lcd_display_backlight_init(), TAG, "lcd backlight init failed");
    ESP_RETURN_ON_ERROR(lcd_display_backlight_on(), TAG, "lcd backlight on failed");

    lv_init();

    const int hor_res = esp_display_get_h_res();
    const int ver_res = esp_display_get_v_res();
    s_display = lv_display_create(hor_res, ver_res);
    ESP_RETURN_ON_FALSE(s_display != NULL, ESP_ERR_NO_MEM, TAG, "lv_display_create failed");

    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(s_display, lvgl_flush_cb);

    uint32_t draw_buf_pixels = (uint32_t)hor_res * UI_LVGL_DRAW_BUF_LINES;
    uint32_t draw_buf_bytes = draw_buf_pixels * sizeof(lv_color_t);

    s_buf_1 = heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (s_buf_1 == NULL) {
        free(s_buf_1);
        s_buf_1 = NULL;
        return ESP_ERR_NO_MEM;
    }

    lv_display_set_buffers(s_display, s_buf_1, NULL, draw_buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

    const esp_timer_create_args_t tick_timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
    };

    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_timer_args, &s_tick_timer), TAG, "tick timer create failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(s_tick_timer, UI_LVGL_TICK_PERIOD_MS * 1000U), TAG,
                        "tick timer start failed");

    s_initialized = true;
    ESP_LOGI(TAG, "LVGL port initialized (%dx%d)", hor_res, ver_res);
    return ESP_OK;
}

void lvgl_port_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    if (s_tick_timer != NULL) {
        esp_timer_stop(s_tick_timer);
        esp_timer_delete(s_tick_timer);
        s_tick_timer = NULL;
    }

    free(s_buf_1);
    s_buf_1 = NULL;

    s_display = NULL;
    s_initialized = false;

    display_base_deinit();
}

bool lvgl_port_lock(uint32_t timeout_ms)
{
    return display_base_lock(timeout_ms);
}

void lvgl_port_unlock(void)
{
    display_base_unlock();
}

uint32_t lvgl_port_timer_handler(void)
{
    if (!s_initialized) {
        return 0;
    }

    return lv_timer_handler();
}
