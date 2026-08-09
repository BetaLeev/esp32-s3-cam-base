# ESP-IDF Component CMakeLists.txt

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# 排除 web 目录下的源文件

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
        spiffs            # SPIFFS文件系统
        driver            # GPIO、PWM等硬件驱动
        mbedtls           # TLS/SSL加密
        cjson
)