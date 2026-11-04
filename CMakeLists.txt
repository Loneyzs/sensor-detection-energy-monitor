cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/zephyr_utf8.cmake)
project(energy_monitor)
target_sources(app PRIVATE src/main.c src/monitor.c src/acs712.c src/button.c
  src/ui_model.c src/ui_render.c src/ssd1306.c src/oled.c
  boards/seekfree/energy_mspm0g3507/i2c_inputs.c)
target_compile_options(app PRIVATE -Wall -Wextra)
