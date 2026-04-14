cmake_minimum_required(VERSION 3.5)

if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/components")
    message(FATAL_ERROR "Local components directory not found: ${CMAKE_CURRENT_LIST_DIR}/components")
endif()

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_voice_motor_control_frontend)
