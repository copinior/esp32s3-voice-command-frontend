#include "display_base.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_lvgl_mutex = NULL;

esp_err_t display_base_init(void)
{
    if (s_lvgl_mutex != NULL) {
        return ESP_OK;
    }

    s_lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_lvgl_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void display_base_deinit(void)
{
    if (s_lvgl_mutex != NULL) {
        vSemaphoreDelete(s_lvgl_mutex);
        s_lvgl_mutex = NULL;
    }
}

bool display_base_lock(uint32_t timeout_ms)
{
    if (s_lvgl_mutex == NULL) {
        return false;
    }

    TickType_t timeout_ticks = (timeout_ms == 0U) ? 0U : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mutex, timeout_ticks) == pdTRUE;
}

void display_base_unlock(void)
{
    if (s_lvgl_mutex != NULL) {
        xSemaphoreGiveRecursive(s_lvgl_mutex);
    }
}
