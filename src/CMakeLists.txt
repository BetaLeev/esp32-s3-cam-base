# ESP-IDF Component CMakeLists.txt
# 前端文件通过 data 目录由 PlatformIO 自动打包到 SPIFFS

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# 排除 web 目录下的源文件（不需要编译）
list(FILTER app_sources EXCLUDE REGEX ".*/web/.*\\.c$")

# ESP-IDF 组件依赖列表
# 所有使用到的组件都必须在此声明，否则链接时会找不到符号
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
        fatfs             # FAT文件系统(TF卡)
        spiffs            # SPIFFS文件系统
        esp_adc           # ADC模数转换
        driver            # GPIO、PWM等硬件驱动
        mbedtls           # TLS/SSL加密(网络通信)
)
