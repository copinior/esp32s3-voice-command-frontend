#ifndef DISPLAY_BASE_H
#define DISPLAY_BASE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t display_base_init();
void display_base_deinit();
bool display_base_lock(uint32_t timeout_ms);
void display_base_unlock();

#endif
