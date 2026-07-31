# ESP-IDF Component CMakeLists.txt
# 前端文件通过 data 目录由 PlatformIO 自动打包到 SPIFFS

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# 排除 web 目录下的源文件（不需要编译）
list(FILTER app_sources EXCLUDE REGEX ".*/web/.*\\.c$")

idf_component_register(SRCS ${app_sources}
    INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    REQUIRES esp_netif lwip esp_http_server nvs_flash esp_timer freertos fatfs esp_adc driver spiffs
)
