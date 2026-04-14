#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t lvgl_port_init(void);
void lvgl_port_deinit(void);

bool lvgl_port_lock(uint32_t timeout_ms);
void lvgl_port_unlock(void);

uint32_t lvgl_port_timer_handler(void);
