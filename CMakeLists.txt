cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_demo)

littlefs_create_partition_image(webfs "${CMAKE_SOURCE_DIR}/data/web" FLASH_IN_PROJECT)