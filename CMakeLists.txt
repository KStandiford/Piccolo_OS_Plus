# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

# Pull in Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(
    piccolo_os_plus_demo
    VERSION 1.01
    DESCRIPTION "A demonstration of Piccolo OS Plus"
    LANGUAGES C CXX ASM)

list(APPEND PICO_CONFIG_HEADER_FILES ${PROJECT_SOURCE_DIR}/piccolo_os_lock_core.h)
# Initialise the Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name

add_executable(${PROJECT_NAME} piccolo_os_demo.c context_switch.s piccolo_os.c piccolo_os.h 
piccolo_os_lock_core.c piccolo_os_lock_core.h)

#pico_set_binary_type(${PROJECT_NAME} no_flash)

pico_add_extra_outputs(${PROJECT_NAME})

# Add the standard library to the build
target_link_libraries(${PROJECT_NAME} pico_stdlib pico_malloc hardware_exception hardware_sync
    pico_multicore )


pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_enable_stdio_usb(${PROJECT_NAME} 0)

add_subdirectory( "${PROJECT_SOURCE_DIR}/docs")
