cmake_minimum_required(VERSION 3.20.0)

set(BOARD esp32)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(crownstone-router-firmware)

FILE(GLOB app_sources src/*.cpp)
FILE(GLOB app_sources_network src/drivers/network/*.cpp)
FILE(GLOB app_sources_uart src/drivers/uart/*.cpp)

target_sources(app PRIVATE ${app_sources} ${app_sources_network} ${app_sources_uart})
target_include_directories(app PRIVATE include/)
target_compile_options(app PRIVATE -fno-threadsafe-statics)