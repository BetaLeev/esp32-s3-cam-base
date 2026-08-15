/**
 * @file main.c
 * @brief ESP32-S3智能水泵控制系统 - 主程序入口（异步音频集成版）
 */
#include "esp_clk_tree.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/Task.h"
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "dns_server.h"
#include "http_server.h"
#include "lwip/ip_addr.h"
#include "nvs_flash.h"
#include "sdcard/sdcard.h"
#include "spiffs_web.h"
#include "wifi/wifi.h"
#include "wifi/wifi_config.h"

#include "device/servo/servo.h"
#include "device/motor/motor.h"
#include "device/led/led.h"
#include "device/pulse/pulse.h"
#include "device/pulse/pulse_web.h"
#include "audio/audio.h"
#include "audio/audio_web.h"
#include "audio/audio_simple.h" // 引入异步音频模块头文件
#include "sensors/sensors.h"
#include "system/system.h"
#include "video/video.h"
#include "ai/ai.h"

static const char *TAG = "MAIN";
#define LOG_TAG TAG

/* 固件编译时间戳 - 用于确认是否烧录了新代码 */
static const char *FIRMWARE_BUILD_TIME = __DATE__ " " __TIME__;

/* 系统启动时间戳 */
static int64_t g_system_start_time = 0;

/**
 * @brief 更新硬件资源监控数据（仅保留基础 DRAM/PSRAM/Flash/CPU/Uptime 监控）
 */
static void update_hardware_resources(void) {
    /* DRAM内存信息 */
    g_system_status.dram_total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    g_system_status.dram_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);

    /* PSRAM内存信息 */
    g_system_status.psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    g_system_status.psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    /* Flash信息 */
    uint32_t flash_size = 0;
    if (esp_flash_get_size(esp_flash_default_chip, &flash_size) == ESP_OK) {
        g_system_status.flash_total = flash_size;
        g_system_status.flash_free = flash_size;
    }

    /* SPIFFS信息 */
    size_t spiffs_total = 0;
    size_t spiffs_free = 0;
    if (spiffs_web_get_info(&spiffs_total, &spiffs_free) == ESP_OK) {
        g_system_status.spiffs_total = (uint32_t)spiffs_total;
        g_system_status.spiffs_free = (uint32_t)spiffs_free;
    }

    /* [禁用] TF卡信息 - 强制赋值为 0 */
    g_system_status.sdcard_mounted = 0;
    g_system_status.sdcard_total = 0;
    g_system_status.sdcard_free = 0;

    /* CPU频率 */
    uint32_t cpu_freq_hz = 0;
    if (esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED,
                                     &cpu_freq_hz) == ESP_OK) {
        g_system_status.cpu_freq_mhz = cpu_freq_hz / 1000000;
    } else {
        g_system_status.cpu_freq_mhz = 240;
    }

    /* 系统运行时间 */
    if (g_system_start_time > 0) {
        g_system_status.uptime_seconds =
            (unsigned int)((esp_timer_get_time() - g_system_start_time) / 1000000);
    }
}

/* 全局系统状态 - 仅供内存与运行时间展示 */
system_status_t g_system_status = {
    .thermistor_raw = 0,
    .thermistor_temp = 0.0f,
    .photosensor_raw = 0,
    .light = 0.0f,
    .dht11_temp = 0.0f,
    .dht11_humidity = 0.0f,
    .dht11_valid = 0,
    .pump_state = 0,
    .pump_speed = 0,
    .servo_angle = 90,
    .dram_total = 0,
    .dram_free = 0,
    .psram_total = 0,
    .psram_free = 0,
    .flash_total = 0,
    .flash_free = 0,
    .spiffs_total = 0,
    .spiffs_free = 0,
    .sdcard_total = 0,
    .sdcard_free = 0,
    .sdcard_mounted = 0,
    .cpu_freq_mhz = 0,
    .uptime_seconds = 0,
    .version = 1,
};

/* __attribute__((unused)) 避免禁用该函数时编译器报 -Wunused-function 警告 */
__attribute__((unused)) static void format_bytes(uint64_t bytes, char *buf, size_t buf_size) {
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        snprintf(buf, buf_size, "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024ULL * 1024ULL) {
        snprintf(buf, buf_size, "%.2f MB", bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024ULL) {
        snprintf(buf, buf_size, "%.2f KB", bytes / 1024.0);
    } else {
        snprintf(buf, buf_size, "%llu B", (unsigned long long)bytes);
    }
}

/**
 * @brief 应用主任务 - 仅监控基础系统状态
 */
static void app_main_task(void *pvParameters) {
    MAIN_LOGI(TAG, "应用主任务启动（仅 Wi-Fi 调试模式）");

    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        update_hardware_resources();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/**
 * @brief 应用主入口
 */
void app_main(void) {
    printf(">>> printf: app_main entered\n");
    esp_log_level_set("MAIN", ESP_LOG_INFO);

    MAIN_LOGI(TAG, ">>> app_main 第一行");
    g_system_start_time = esp_timer_get_time();

    /* 固件编译时间戳 */
    MAIN_LOGI(TAG, "========== app_main 开始 ==========");
    MAIN_LOGI(TAG, "固件编译时间: %s", FIRMWARE_BUILD_TIME);
    ESP_EARLY_LOGI("TEST", "app_main entered immediately");
    ESP_EARLY_LOGI("MAIN", ">>> FORCE OUTPUT - app_main entered");
    
    /* 1. 初始化 NVS Flash */
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "NVS Flash初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    /* 初始化执行器模块（电机 + 舵机） */
    ret = motor_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "电机模块初始化失败: %s", esp_err_to_name(ret));
    }

    ret = servo_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "执行器模块初始化失败: %s", esp_err_to_name(ret));
    }

    /* 初始化LED模块 */
    ret = led_init();
    if (ret != ESP_OK) {
        MAIN_LOGW(TAG, "LED模块初始化失败: %s", esp_err_to_name(ret));
    }

    /* 初始化脉冲控制模块 */
    ret = pulse_init();
    if (ret != ESP_OK) {
        MAIN_LOGW(TAG, "脉冲控制模块初始化失败: %s", esp_err_to_name(ret));
    }

    /* 【核心更改】初始化异步音频模块（替代原先容易崩溃的同步 audio_init） */
    ret = audio_simple_init();
    if (ret != ESP_OK) {
        MAIN_LOGW(TAG, "异步音频模块初始化失败: %s", esp_err_to_name(ret));
    } else {
        MAIN_LOGI(TAG, "异步音频模块初始化成功 (后台任务运行在 Core 1)");
        MAIN_LOGI(TAG, "提示: 使用 POST /api/audio/play 异步播放音乐");
    }

    /* 传感器模块初始化 */
    ret = sensors_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "传感器模块初始化失败: %s", esp_err_to_name(ret));
    }

    /* 初始化摄像头视频模块（异步启动，不阻塞主流程） */
    ret = video_init_async();
    if (ret != ESP_OK) {
        MAIN_LOGW(TAG, "摄像头异步任务创建失败 (0x%x)", ret);
    }

    /* 2. 初始化 Wi-Fi 热点/STA 驱动 */
    ret = wifi_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "Wi-Fi初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    /* 3. 初始化 Wi-Fi 配置管理 */
    ret = wifi_config_init();
    if (ret != ESP_OK) {
        MAIN_LOGW(TAG, "Wi-Fi配置管理初始化失败: %s", esp_err_to_name(ret));
    }

    /* 4. 启动 DNS 服务器 */
    ret = dns_server_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "DNS服务器启动失败: %s", esp_err_to_name(ret));
        return;
    }

    /* 5. 初始化 SPIFFS 文件系统 */
    ret = spiffs_web_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "SPIFFS初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    /* 6. 启动 HTTP 服务器 */
    ret = http_server_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "HTTP服务器启动失败: %s", esp_err_to_name(ret));
        return;
    }

    /* 7. 注册所有模块的 Web API 路由 */
    ret = audio_web_init();
    if (ret != ESP_OK) {
        MAIN_LOGW(TAG, "音频Web接口注册失败: %s", esp_err_to_name(ret));
    }

    /* 初始化系统管理模块 */
    ret = system_init();
    if (ret != ESP_OK) {
        MAIN_LOGW(TAG, "系统管理模块初始化失败: %s", esp_err_to_name(ret));
    }

    ret = sdcard_init();
    if (ret != ESP_OK) {
        MAIN_LOGW(TAG, "TF卡初始化失败");
    }

    /* 8. 初始化 AI 语音模块 */
    ret = ai_init();
    if (ret != ESP_OK) {
        MAIN_LOGW(TAG, "AI模块初始化失败: %s", esp_err_to_name(ret));
    }

    /* 传感器后台轮询任务 */
    ret = sensors_create_task();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "传感器任务创建失败: %s", esp_err_to_name(ret));
    }

    /* 创建系统监控任务 */
    BaseType_t task_ret =
        xTaskCreate(app_main_task, "app_main_task", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
    if (task_ret != pdPASS) {
        MAIN_LOGE(TAG, "创建应用主任务失败");
    }
}