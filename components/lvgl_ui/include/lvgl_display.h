#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t lvgl_display_init(void);
void lvgl_display_deinit(void);

bool lvgl_display_lock(uint32_t timeout_ms);
void lvgl_display_unlock(void);

uint32_t lvgl_display_timer_handler(void);
