# ESP-IDF Component CMakeLists.txt

# 源文件列表（排除临时文件）
FILE(GLOB_RECURSE app_sources
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/device/led/*.c
    ${CMAKE_SOURCE_DIR}/src/device/pulse/*.c
    ${CMAKE_SOURCE_DIR}/src/device/motor/*.c
    ${CMAKE_SOURCE_DIR}/src/device/servo/*.c
    ${CMAKE_SOURCE_DIR}/src/sdcard/*.c
    ${CMAKE_SOURCE_DIR}/src/sensors/*.c
    ${CMAKE_SOURCE_DIR}/src/sensors/**/*.c
    ${CMAKE_SOURCE_DIR}/src/system/*.c
    ${CMAKE_SOURCE_DIR}/src/wifi/*.c
    ${CMAKE_SOURCE_DIR}/src/video/*.c
    ${CMAKE_SOURCE_DIR}/src/audio/*.c
    ${CMAKE_SOURCE_DIR}/src/audio/*.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/*.c
    ${CMAKE_SOURCE_DIR}/src/config/*.c
    ${CMAKE_SOURCE_DIR}/src/web/*.c
    ${CMAKE_SOURCE_DIR}/src/web/**/*.c
)

idf_component_register(SRCS ${app_sources}
    INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    REQUIRES
        esp_wifi          # WiFi扫描、连接
        esp_netif         # 网络接口
        lwip              # TCP/IP协议栈
        nvs_flash         # NVS非易失性存储
        esp_http_server   # HTTP服务器
        esp_timer         # 定时器
        freertos          # FreeRTOS操作系统
        driver            # GPIO、PWM等硬件驱动
        mbedtls           # TLS/SSL加密
        cjson             # JSON 解析
)
