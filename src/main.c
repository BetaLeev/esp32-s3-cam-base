/**
 * @file main.c
 * @brief ESP32-S3智能水泵控制系统 - 主程序入口
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
#include "lwip/ip_addr.h"
#include "nvs_flash.h"
#include "sdcard/sdcard.h"
#include "web/filesystem/filesystem.h"
#include "web/web.h"
#include "wifi/wifi.h"
#include "wifi/wifi_config.h"

#include "ai/ai.h"
#include "audio/audio.h"
#include "audio/audio_simple.h"
#include "audio/mic.h"
#include "device/display/display.h"
#include "device/led/led.h"
#include "device/motor/motor.h"
#include "device/pulse/pulse.h"
#include "device/servo/servo.h"
#include "sensors/sensors.h"
#include "system/system.h"
#include "video/video.h"

static const char *TAG = "MAIN";
#define LOG_TAG TAG

static const char *FIRMWARE_BUILD_TIME = __DATE__ " " __TIME__;
static int64_t g_system_start_time = 0;

static void update_hardware_resources(void) {
    g_system_status.dram_total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    g_system_status.dram_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    g_system_status.psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    g_system_status.psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    uint32_t flash_size = 0;
    if (esp_flash_get_size(esp_flash_default_chip, &flash_size) == ESP_OK) {
        g_system_status.flash_total = flash_size;
        g_system_status.flash_free = flash_size;
    }

    size_t spiffs_total = 0;
    size_t spiffs_free = 0;
    if (web_filesystem_get_info(&spiffs_total, &spiffs_free) == ESP_OK) {
        g_system_status.spiffs_total = (uint32_t)spiffs_total;
        g_system_status.spiffs_free = (uint32_t)spiffs_free;
    }

    g_system_status.sdcard_mounted = 0;
    g_system_status.sdcard_total = 0;
    g_system_status.sdcard_free = 0;

    uint32_t cpu_freq_hz = 0;
    if (esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED, &cpu_freq_hz) == ESP_OK) {
        g_system_status.cpu_freq_mhz = cpu_freq_hz / 1000000;
    } else {
        g_system_status.cpu_freq_mhz = 240;
    }

    if (g_system_start_time > 0) {
        g_system_status.uptime_seconds = (unsigned int)((esp_timer_get_time() - g_system_start_time) / 1000000);
    }
}

system_status_t g_system_status = {
    .version = 1,
};

static void app_main_task(void *pvParameters) {
    MAIN_LOGI(TAG, "应用主任务启动");
    vTaskDelay(pdMS_TO_TICKS(2000));
    while (1) {
        update_hardware_resources();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void) {
    printf(">>> printf: app_main entered\n");
    esp_log_level_set("MAIN", ESP_LOG_INFO);

    MAIN_LOGI(TAG, "========== app_main 开始 ==========");
    MAIN_LOGI(TAG, "固件编译时间: %s", FIRMWARE_BUILD_TIME);
    g_system_start_time = esp_timer_get_time();

    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "NVS Flash初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    display_init();
    display_show_chinese_demo("MAC问题解决专家");

    motor_init();
    servo_init();
    led_init();
    pulse_init();
    sensors_init();
    video_init_async();

    wifi_init();
    wifi_config_init();

    // Web 初始化（已在内部集中注册所有 API 路由）
    ret = web_init();
    if (ret != ESP_OK) {
        MAIN_LOGE(TAG, "Web模块初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    // 初始化底层异步音频与麦克风模块
    audio_simple_init();
    mic_init();

    system_init();
    sdcard_init();
    ai_init();
    sensors_create_task();

    xTaskCreate(app_main_task, "app_main_task", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
}