# Project Structure Guide

This document describes the current module layout after refactoring display initialization into unified BSP-style layers.

## 1. Top-Level

- `main/`
  - Application entry and speech recognition pipeline.
  - Calls board-level APIs from `hardware_driver`.
- `components/`
  - Reusable platform and feature components.

## 2. Hardware BSP Layer

- `components/hardware_driver/`
  - Unified hardware abstraction entry.
  - Public wrapper header: `include/esp_board_init.h`
  - Public wrapper source: `esp_board_init.c`

- `components/hardware_driver/boards/include/esp_custom_board.h`
  - Single source of board pin/config macros (audio + LCD).

- `components/hardware_driver/boards/include/bsp_board.h`
  - Board-level audio and common BSP APIs.

- `components/hardware_driver/boards/include/bsp_display.h`
  - Board-level display BSP APIs.

- `components/hardware_driver/boards/bsp_board.c`
  - Audio codec, I2S, I2C, PCA9557 and board common initialization.

- `components/hardware_driver/boards/bsp_display.c`
  - LCD SPI panel init (ST7789), backlight PWM, fill/draw APIs.

## 3. LVGL/UI Layer

- `components/lvgl_ui/`
  - UI and LVGL adaptation logic.

- `components/lvgl_ui/include/lcd_display.h`
  - Display-facing UI helpers that call `esp_board_init` wrappers.

- `components/lvgl_ui/src/lcd_display.c`
  - Thin adapter from UI module to unified BSP display APIs.

- `components/lvgl_ui/src/lvgl_port.c`
  - LVGL porting layer (tick/task/flush, to be completed).

- `components/lvgl_ui/src/speech_ui.c`
  - Speech interaction UI state rendering (to be completed).

## 4. Dependency Direction (Recommended)

Keep the call chain one-way:

1. `main` -> `lvgl_ui` (UI logic)
2. `lvgl_ui` -> `hardware_driver` (hardware access only through wrappers)
3. `hardware_driver` -> HAL/IDF drivers

Avoid reverse dependencies (for example `hardware_driver` including `lvgl_ui`).

## 5. What Changed in This Refactor

1. Display pin/config macros moved to `esp_custom_board.h`.
2. Added display BSP APIs in `bsp_display.h/.c`.
3. Added `esp_display_*` wrapper APIs in `esp_board_init.h/.c`.
4. `lvgl_ui` now uses `lcd_display.c` as adapter to unified BSP APIs.
5. Removed duplicated display config header from `lvgl_ui`:
   - deleted `components/lvgl_ui/include/lvgl_board_config.h`

## 6. Quick Navigation

- Add or change GPIO/panel parameters:
  - `components/hardware_driver/boards/include/esp_custom_board.h`
- Modify LCD bring-up sequence:
  - `components/hardware_driver/boards/bsp_display.c`
- Modify audio bring-up sequence:
  - `components/hardware_driver/boards/bsp_board.c`
- Modify UI display call behavior:
  - `components/lvgl_ui/src/lcd_display.c`
