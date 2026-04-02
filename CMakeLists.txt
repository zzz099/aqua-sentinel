cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME aqua_sentinel)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Fetch all LVGL source files
file(GLOB_RECURSE LVGL_SOURCES "LVGL/src/*.c")
file(GLOB_RECURSE LVGL_DEMO_SOURCES "LVGL/demos/*.c")

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user sources here
        Core/Src/touch_iic.c
        Core/Src/touch_800x480.c
        Core/Src/re_printf.c
        Core/Src/AT_MQTT_OS.c
        Core/Src/cJSON.c
        Core/Src/AT24C256.c
        Core/Src/ds18b20.c
        Core/Src/tds.c
        Core/Src/turbidity.c
        Core/Src/bmp280function.c
        Core/Src/aquarium_light.c
        Core/Src/waterlevel.c
        Core/Src/ph_sensor.c
        Core/Src/qspi_w25q64.c
        Core/Src/lcd_fonts.c
        Core/Src/lcd_image.c
        Core/Src/lcd_pwm.c
        Core/Src/lcd_rgb.c
        Core/Src/lcd_test.c
        Core/Src/lv_ui.c
        Core/Src/sdram.c
        Core/Src/device_ctrl.c
        Core/Src/auto_fish_ctrl.c
        Core/Src/task_mqtt.c
        Core/Src/task_sensor.c
        # Add LVGL port files
        LVGL/lv_port_disp.c
        LVGL/lv_port_indev.c
        LVGL/lv_port_fs.c
        # Add LVGL sources
        ${LVGL_SOURCES}
        # Add LVGL demo sources (benchmark, etc.)
        ${LVGL_DEMO_SOURCES}
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined include paths
        LVGL
        LVGL/src
        LVGL/demos
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
        # Add user defined symbols
        LV_CONF_INCLUDE_SIMPLE
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
        stm32cubemx

        # Add user defined libraries
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -u _printf_float)
