set(srcs
    "src/lvgl_port.c"
    "src/lcd_display.c"
    "src/lvgl_display.c"
    "src/speech_ui.c"
    "src/display_base.c"
)

set(include_dirs
    "include"
)

set(requires
    esp_timer
    freertos
    lvgl
    hardware_driver
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
)

component_compile_options(-w)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
