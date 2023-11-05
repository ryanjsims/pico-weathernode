cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

include(lib/pico-sdk/pico_sdk_init.cmake)

project(pico-weathernode)

pico_sdk_init()

add_subdirectory(lib/pico-web-client)
target_compile_definitions(pico_web_client PRIVATE "LOG_LEVEL=$ENV{LOG_LEVEL}")

add_executable(pico_weathernode
    src/main.cpp
    src/aht20.cpp
)

target_include_directories(pico_weathernode PUBLIC include)
target_link_libraries(pico_weathernode PRIVATE
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mbedtls
    pico_mbedtls
    pico_multicore
    pico_stdlib
    pico_web_client
    hardware_i2c
)
target_compile_options(pico_weathernode PRIVATE "-Wno-psabi")
target_compile_definitions(pico_weathernode PRIVATE 
    "WIFI_SSID=\"$ENV{WIFI_SSID}\""
    "WIFI_PASSWORD=\"$ENV{WIFI_PASSWORD}\""
    "LOG_LEVEL=$ENV{LOG_LEVEL}"
    "LAT=$ENV{LAT}"
    "LNG=$ENV{LNG}"
    "TIMEZONE=\"$ENV{TIMEZONE}\""
)
target_link_options(pico_weathernode PRIVATE "-Wl,--print-memory-usage")

pico_enable_stdio_usb(pico_weathernode 1)
pico_enable_stdio_uart(pico_weathernode 0)
pico_add_extra_outputs(pico_weathernode)