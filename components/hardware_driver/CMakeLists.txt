set(srcs
    "esp_board_init.c"
    "boards/bsp_board.c"
    "boards/bsp_display.c"
)

set(include_dirs
    "include"
    "boards/include"
)

set(requires
    driver
    esp_lcd
    fatfs
    spiffs
    es7210
    es8311
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
)

component_compile_options(-w)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
