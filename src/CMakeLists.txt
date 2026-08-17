# ESP-IDF Component CMakeLists.txt

# 1. 自动递归获取 src 目录下所有的 .c 和 .cpp 源文件
FILE(GLOB_RECURSE app_sources
    "*.c"
    "*.cpp"
)

# 2. 自动递归获取 src 目录及所有子目录作为头文件包含路径
FILE(GLOB_RECURSE include_files "*.h" "*.hpp")
set(include_dirs "")
foreach(_file ${include_files})
    get_filename_component(_dir ${_file} PATH)
    list(APPEND include_dirs ${_dir})
endforeach()
list(REMOVE_DUPLICATES include_dirs)

# 3. 注册组件并关联依赖
idf_component_register(
    SRCS 
        ${app_sources}
    INCLUDE_DIRS 
        "."
        ${include_dirs}
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
        u8g2              # 👈 核心修改：链接 U8g2 单色屏图形库
)