#include "lvgl_display.h"

#include "lvgl_port.h"

esp_err_t lvgl_display_init(void)
{
    return lvgl_port_init();
}

void lvgl_display_deinit(void)
{
    lvgl_port_deinit();
}

bool lvgl_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void lvgl_display_unlock(void)
{
    lvgl_port_unlock();
}

uint32_t lvgl_display_timer_handler(void)
{
    return lvgl_port_timer_handler();
}
