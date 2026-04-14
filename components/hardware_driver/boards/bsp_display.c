#include "bsp_display.h"

#include "bsp_board.h"
#include "esp_check.h"
#include "esp_custom_board.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "BSP_DISPLAY";
static const TickType_t LCD_FLUSH_WAIT_TICKS = pdMS_TO_TICKS(200);

static bool s_spi_inited = false;
static bool s_display_inited = false;
static bool s_backlight_inited = false;

static esp_lcd_panel_io_handle_t s_panel_io = NULL;
static esp_lcd_panel_handle_t s_panel = NULL;
static SemaphoreHandle_t s_lcd_flush_done = NULL;

static bool panel_io_color_trans_done_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata,
                                         void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    (void)user_ctx;
    BaseType_t high_task_wakeup = pdFALSE;
    if (s_lcd_flush_done != NULL) {
        xSemaphoreGiveFromISR(s_lcd_flush_done, &high_task_wakeup);
    }
    return high_task_wakeup == pdTRUE;
}

static esp_err_t bsp_display_spi_init(void)
{
    if (s_spi_inited) {
        return ESP_OK;
    }

    spi_bus_config_t buscfg = {
        .mosi_io_num = BSP_LCD_SPI_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .sclk_io_num = BSP_LCD_SPI_CLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_DRAM_BUF_HEIGHT * sizeof(uint16_t),
    };

    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO), TAG, "spi bus init failed");
    s_spi_inited = true;
    return ESP_OK;
}

static int clamp_brightness(int brightness_percent)
{
    if (brightness_percent < 0) {
        return 0;
    }
    if (brightness_percent > 100) {
        return 100;
    }
    return brightness_percent;
}

esp_err_t bsp_display_init(void)
{
    if (s_display_inited) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(bsp_display_spi_init(), TAG, "display spi init failed");
    if (s_lcd_flush_done == NULL) {
        s_lcd_flush_done = xSemaphoreCreateBinary();
        ESP_RETURN_ON_FALSE(s_lcd_flush_done != NULL, ESP_ERR_NO_MEM, TAG, "flush semaphore alloc failed");
    }

    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = BSP_LCD_SPI_CS,
        .dc_gpio_num = BSP_LCD_DC,
        .spi_mode = BSP_LCD_SPI_MODE,
        .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = BSP_LCD_TRANS_QUEUE_DEPTH,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .on_color_trans_done = panel_io_color_trans_done_cb,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(BSP_LCD_SPI_NUM, &io_config, &s_panel_io), TAG, "new panel io failed");
    ESP_LOGI(TAG, "panel io ok: host=%d mode=%d pclk=%d", BSP_LCD_SPI_NUM, BSP_LCD_SPI_MODE, BSP_LCD_PIXEL_CLOCK_HZ);
    
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(s_panel_io, &panel_config, &s_panel), TAG, "new st7789 panel failed");
    ESP_LOGI(TAG, "panel driver ok: st7789 bpp=%d", BSP_LCD_BITS_PER_PIXEL);

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset failed");

    if (BSP_LCD_SPI_CS == GPIO_NUM_NC) {
        esp_err_t ret = bsp_lcd_cs_set(false);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "external LCD CS set after reset failed: %s", esp_err_to_name(ret));
        }
    }

    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init failed");
    ESP_LOGI(TAG, "panel init sequence ok");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, BSP_LCD_COLOR_INVERT), TAG, "panel invert failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(s_panel, BSP_LCD_OFFSET_X, BSP_LCD_OFFSET_Y), TAG, "panel set gap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, BSP_LCD_SWAP_XY), TAG, "panel swap xy set failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, BSP_LCD_MIRROR_X, BSP_LCD_MIRROR_Y), TAG, "panel mirror set failed");

    esp_err_t disp_ret = esp_lcd_panel_disp_on_off(s_panel, true);
    if (disp_ret != ESP_OK && disp_ret != ESP_ERR_NOT_SUPPORTED) {
        ESP_RETURN_ON_ERROR(disp_ret, TAG, "panel display failed");
    }
    ESP_LOGI(TAG, "panel display on done");

    s_display_inited = true;
    return ESP_OK;
}

esp_err_t bsp_display_deinit(void)
{
    if (s_panel != NULL) {
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
    }
    if (s_panel_io != NULL) {
        esp_lcd_panel_io_del(s_panel_io);
        s_panel_io = NULL;
    }

    s_display_inited = false;
    if (s_lcd_flush_done != NULL) {
        vSemaphoreDelete(s_lcd_flush_done);
        s_lcd_flush_done = NULL;
    }
    return ESP_OK;
}

esp_err_t bsp_display_backlight_init(void)
{
    if (s_backlight_inited || BSP_LCD_BACKLIGHT == GPIO_NUM_NC) {
        return ESP_OK;
    }

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 25000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "backlight timer config failed");

    ledc_channel_config_t ch_cfg = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .flags = {
            .output_invert = BSP_LCD_BACKLIGHT_INVERT,
        },
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch_cfg), TAG, "backlight channel config failed");

    s_backlight_inited = true;
    return ESP_OK;
}

esp_err_t bsp_display_backlight_set(int brightness_percent)
{
    if (BSP_LCD_BACKLIGHT == GPIO_NUM_NC) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(bsp_display_backlight_init(), TAG, "backlight init failed");
    int clamped = clamp_brightness(brightness_percent);
    uint32_t duty = (uint32_t)((1023 * clamped) / 100);

    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty), TAG, "backlight set duty failed");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH), TAG, "backlight update duty failed");
    return ESP_OK;
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_backlight_set(100);
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_backlight_set(0);
}

esp_err_t bsp_display_fill_color(uint16_t color)
{
    ESP_RETURN_ON_FALSE(s_display_inited && s_panel != NULL, ESP_ERR_INVALID_STATE, TAG, "display not initialized");

    uint16_t line_buf[BSP_LCD_H_RES];
    for (int x = 0; x < BSP_LCD_H_RES; x++) {
        line_buf[x] = color;
    }

    for (int y = 0; y < BSP_LCD_V_RES; y++) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(s_panel, 0, y, BSP_LCD_H_RES, y + 1, line_buf), TAG, "fill line failed");
    }
    return ESP_OK;
}

esp_err_t bsp_display_draw_bitmap(int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    ESP_RETURN_ON_FALSE(s_display_inited && s_panel != NULL, ESP_ERR_INVALID_STATE, TAG, "display not initialized");
    ESP_RETURN_ON_FALSE(color_data != NULL, ESP_ERR_INVALID_ARG, TAG, "null color data");
    ESP_RETURN_ON_FALSE(x_start >= 0 && y_start >= 0 && x_end > x_start && y_end > y_start, ESP_ERR_INVALID_ARG, TAG,
                        "invalid draw area");

    if (s_lcd_flush_done != NULL) {
        xSemaphoreTake(s_lcd_flush_done, 0);
    }

    esp_err_t ret = esp_lcd_panel_draw_bitmap(s_panel, x_start, y_start, x_end, y_end, color_data);
    ESP_RETURN_ON_ERROR(ret, TAG, "draw bitmap failed");

    if (s_lcd_flush_done != NULL) {
        if (xSemaphoreTake(s_lcd_flush_done, LCD_FLUSH_WAIT_TICKS) != pdPASS) {
            ESP_LOGW(TAG, "draw wait timeout");
        }
    }
    return ESP_OK;
}

int bsp_display_get_h_res(void)
{
    return BSP_LCD_H_RES;
}

int bsp_display_get_v_res(void)
{
    return BSP_LCD_V_RES;
}
