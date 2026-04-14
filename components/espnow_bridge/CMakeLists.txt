set(srcs
    "espnow_bridge.c"
)

set(include_dirs
    "include"
)

set(requires
    esp_wifi
    esp_event
    esp_netif
    nvs_flash
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
)

component_compile_options(-w)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
