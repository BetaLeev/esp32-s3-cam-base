/**
 * @file main.c
 * @brief ESP32-S3智能水泵控制系统 - 主程序入口
 * @note 整合所有模块，协调FreeRTOS任务
 */
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_flash.h"
#include "esp_clk_tree.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "wifi_app.h"
#include "wifi_config.h"
#include "lwip/ip_addr.h"
#include "http_server.h"
#include "spiffs_web.h"
#include "sdcard/sdcard.h"
#include "nvs_flash.h"
#include "dns_server.h"

#include "sensors/sensors.h"
#include "actuators/actuators.h"
#include "video/video.h"
#include "audio/audio.h"
#include "audio/audio_web.h"

static const char *TAG = "MAIN";
#define LOG_TAG TAG

/* 系统启动时间戳 */
static int64_t g_system_start_time = 0;

/**
 * @brief 更新硬件资源监控数据
 */
static void update_hardware_resources(void)
{
    /* DRAM内存信息 */
    g_system_status.dram_total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    g_system_status.dram_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);

    /* PSRAM内存信息 - 使用heap_caps检测 */
    g_system_status.psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    g_system_status.psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    /* Flash信息 - 使用esp_flash_get_size */
    uint32_t flash_size = 0;
    if (esp_flash_get_size(esp_flash_default_chip, &flash_size) == ESP_OK) {
        g_system_status.flash_total = flash_size;
        g_system_status.flash_free = flash_size;
    }

    /* SPIFFS信息 - 从spiffs_web获取 */
    size_t spiffs_total = 0;
    size_t spiffs_free = 0;
    if (spiffs_web_get_info(&spiffs_total, &spiffs_free) == ESP_OK) {
        g_system_status.spiffs_total = (uint32_t)spiffs_total;
        g_system_status.spiffs_free = (uint32_t)spiffs_free;
    }

    /* TF卡信息 */
    g_system_status.sdcard_mounted = sdcard_is_mounted() ? 1 : 0;
    uint64_t sd_total = 0, sd_free = 0;
    if (sdcard_get_info(&sd_total, &sd_free) == ESP_OK) {
        g_system_status.sdcard_total = sd_total;
        g_system_status.sdcard_free = sd_free;
    } else {
        g_system_status.sdcard_total = 0;
        g_system_status.sdcard_free = 0;
    }

    /* CPU频率 - 使用esp_clk_tree获取 */
    uint32_t cpu_freq_hz = 0;
    if (esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &cpu_freq_hz) == ESP_OK) {
        g_system_status.cpu_freq_mhz = cpu_freq_hz / 1000000;
    } else {
        g_system_status.cpu_freq_mhz = 240;
    }

    /* 系统运行时间 */
    if (g_system_start_time > 0) {
        g_system_status.uptime_seconds = (unsigned int)((esp_timer_get_time() - g_system_start_time) / 1000000);
    }
}

/* 全局系统状态 - 供各模块共享 */
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

/**
 * @brief 格式化字节数为可读字符串
 */
static void format_bytes(uint64_t bytes, char *buf, size_t buf_size)
{
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
 * @brief 应用主任务 - 监控系统状态
 */
static void app_main_task(void *pvParameters)
{
    MAIN_LOGI(TAG, "应用主任务启动");

    /* 等待系统稳定 */
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        /* 更新硬件资源监控 */
        update_hardware_resources();

        /* 延时5秒 */
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/**
 * @brief 应用主入口
 */
void app_main(void)
{
    /* 记录系统启动时间 */
    g_system_start_time = esp_timer_get_time();

    /* 初始化NVS Flash */
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "NVS Flash初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    /* 初始化执行器模块 */
    ret = actuators_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "执行器模块初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    /* 初始化传感器模块 */
    ret = sensors_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "传感器模块初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    /* 启动摄像头视频模块异步初始化（后台执行，不阻塞） */
    ret = video_init_async();
    if (ret != ESP_OK) {
        MAIN_LOGW(TAG, "摄像头异步任务创建失败 (0x%x)", ret);
    } else {
        MAIN_LOGI(TAG, "摄像头异步初始化已启动");
    }

    /* 初始化Wi-Fi热点 */
    ret = wifi_app_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "Wi-Fi初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    /* 初始化Wi-Fi配置管理 */
    ret = wifi_config_init();
    if (ret != ESP_OK) {
        MAIN_LOGW(TAG, "Wi-Fi配置管理初始化失败: %s", esp_err_to_name(ret));
    }

    /* 启动DNS服务器 */
    ret = dns_server_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "DNS服务器启动失败: %s", esp_err_to_name(ret));
        return;
    }

    /* 初始化SPIFFS文件系统 */
    ret = spiffs_web_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "SPIFFS初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    /* 启动HTTP服务器（必须在TF卡之前，以便处理TF卡错误） */
    ret = http_server_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "HTTP服务器启动失败: %s", esp_err_to_name(ret));
        return;
    }

    /* 初始化TF卡（失败不影响系统运行） */
    ret = sdcard_init();
    if (ret != ESP_OK) {
        MAIN_LOGW(TAG, "TF卡初始化失败，系统将继续运行（SD卡相关功能不可用）");
    } else {
        MAIN_LOGI(TAG, "TF卡初始化成功");
    }

    /* 创建传感器读取任务 */
    ret = sensors_create_task();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "传感器任务创建失败: %s", esp_err_to_name(ret));
    }

    /* 音频模块暂时禁用 - I2S 引脚 GPIO19/20 与 USB D-/D+ 冲突，需重新分配引脚 */
    MAIN_LOGI(TAG, "[阶段1] 音频模块暂禁用 (GPIO19/20 与 USB 冲突)");

    /* 创建应用主任务 */
    BaseType_t task_ret = xTaskCreate(
        app_main_task,
        "app_main_task",
        4096,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    if (task_ret != pdPASS) {
        MAIN_LOGE(TAG, "创建应用主任务失败");
    }
}
