cmake_minimum_required(VERSION 3.20.0)

set(BOARD esp32)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(crownstone-router-firmware)

FILE(GLOB app_sources src/*.cpp)
FILE(GLOB app_sources_drivers src/drivers/*.cpp)
FILE(GLOB app_sources_socket src/socket/*.cpp)

target_sources(app PRIVATE ${app_sources} ${app_sources_drivers} ${app_sources_socket})
target_include_directories(app PRIVATE include/)
target_compile_options(app PRIVATE -fno-threadsafe-statics)