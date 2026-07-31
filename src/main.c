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
#include "lwip/ip_addr.h"
#include "http_server.h"
#include "spiffs_web.h"
#include "sdcard/sdcard.h"
#include "nvs_flash.h"
#include "dns_server.h"

#include "sensors/sensors.h"
#include "actuators/actuators.h"

static const char *TAG = "MAIN";

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
        g_system_status.flash_free = flash_size;  // Flash整体大小，不做减法
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
        g_system_status.cpu_freq_mhz = 240;  // 默认240MHz
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
 * @brief 打印系统启动信息
 */
static void print_system_info(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32-S3 智能水泵控制系统");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "硬件配置:");
    ESP_LOGI(TAG, "  - 主控芯片: ESP32-S3");
    ESP_LOGI(TAG, "  - 电机驱动: TB6612");
    ESP_LOGI(TAG, "  - 传感器: 热敏电阻 + 光敏电阻 + DHT11");
    ESP_LOGI(TAG, "  - 执行器: SG90舵机");
    ESP_LOGI(TAG, "GPIO配置:");
    ESP_LOGI(TAG, "  - 水泵 PWMA: GPIO%d", MOTOR_PWMA_PIN);
    ESP_LOGI(TAG, "  - 水泵 AIN1: GPIO%d", MOTOR_AIN1_PIN);
    ESP_LOGI(TAG, "  - 水泵 AIN2: GPIO%d", MOTOR_AIN2_PIN);
    ESP_LOGI(TAG, "  - 舵机: GPIO%d", SERVO_GPIO);
    ESP_LOGI(TAG, "  - 热敏电阻: GPIO%d", THERMISTOR_GPIO);
    ESP_LOGI(TAG, "  - 光敏电阻: GPIO%d", PHOTOSENSOR_GPIO);
    ESP_LOGI(TAG, "  - DHT11: GPIO%d", DHT11_PIN);
    ESP_LOGI(TAG, "Wi-Fi配置:");
    ESP_LOGI(TAG, "  - 模式: AP+STA共存模式");
    ESP_LOGI(TAG, "  - AP SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "  - AP IP: %d.%d.%d.%d", WIFI_AP_IP_1, WIFI_AP_IP_2, WIFI_AP_IP_3, WIFI_AP_IP_4);
    ESP_LOGI(TAG, "  - AP密码: %s", WIFI_PASSWORD);
    ESP_LOGI(TAG, "  - STA目标Wi-Fi: %s", WIFI_STA_SSID);
    ESP_LOGI(TAG, "========================================");
}

/**
 * @brief 系统初始化
 */
static esp_err_t system_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, ">>> [1/10] 初始化NVS Flash...");
    ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "    [失败] NVS Flash: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "    [成功] NVS Flash");

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, ">>> [2/9] 初始化执行器模块(舵机+电机)...");
    ret = actuators_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "    [失败] 执行器模块: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "    [成功] 执行器模块");

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, ">>> [3/9] 初始化传感器模块...");
    ret = sensors_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "    [失败] 传感器模块: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "    [成功] 传感器模块");

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, ">>> [4/9] 初始化TF卡...");
    ret = sdcard_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "    [警告] TF卡初始化失败，将继续运行");
    } else {
        ESP_LOGI(TAG, "    [成功] TF卡");
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, ">>> [5/9] 初始化Wi-Fi热点...");
    ret = wifi_app_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "    [失败] Wi-Fi: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "    [成功] Wi-Fi热点");

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, ">>> [6/9] 启动DNS服务器(Captive Portal)...");
    ret = dns_server_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "    [失败] DNS服务器: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "    [成功] DNS服务器");

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, ">>> [7/9] 初始化SPIFFS文件系统...");
    ret = spiffs_web_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "    [失败] SPIFFS: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "    [成功] SPIFFS文件系统");

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, ">>> [8/9] 启动HTTP服务器...");
    ret = http_server_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "    [失败] HTTP服务器: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "    [成功] HTTP服务器");

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, ">>> [9/9] 创建传感器读取任务...");
    ret = sensors_create_task();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "    [失败] 传感器任务: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "    [成功] 传感器读取任务");

    return ESP_OK;
}

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
    uint32_t loop_count = 0;

    ESP_LOGI(TAG, "应用主任务启动");

    /* 等待系统稳定 */
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        loop_count++;

        /* 更新硬件资源监控 */
        update_hardware_resources();

        /* 每10秒打印一次状态 */
        if (loop_count % 2 == 0) {
            char ip_str[32];
            char dram_free_str[32], dram_total_str[32];
            char psram_free_str[32], psram_total_str[32];
            char flash_total_str[32];
            char spiffs_free_str[32], spiffs_total_str[32];
            char sdcard_free_str[32], sdcard_total_str[32];

            wifi_app_get_ip_string(ip_str, sizeof(ip_str));
            format_bytes(g_system_status.dram_total, dram_total_str, sizeof(dram_total_str));
            format_bytes(g_system_status.dram_free, dram_free_str, sizeof(dram_free_str));
            format_bytes(g_system_status.psram_total, psram_total_str, sizeof(psram_total_str));
            format_bytes(g_system_status.psram_free, psram_free_str, sizeof(psram_free_str));
            format_bytes(g_system_status.flash_total, flash_total_str, sizeof(flash_total_str));
            format_bytes(g_system_status.spiffs_total, spiffs_total_str, sizeof(spiffs_total_str));
            format_bytes(g_system_status.spiffs_free, spiffs_free_str, sizeof(spiffs_free_str));
            format_bytes(g_system_status.sdcard_total, sdcard_total_str, sizeof(sdcard_total_str));
            format_bytes(g_system_status.sdcard_free, sdcard_free_str, sizeof(sdcard_free_str));

            ESP_LOGI(TAG, "--- 系统状态 ---");
            ESP_LOGI(TAG, "IP: %s", ip_str);
            ESP_LOGI(TAG, "热敏: %.1fC  光敏: %.0flux  DHT11: %.1fC/%.1f%%",
                    g_system_status.thermistor_temp,
                    g_system_status.light,
                    g_system_status.dht11_temp,
                    g_system_status.dht11_humidity);
            ESP_LOGI(TAG, "水泵: %s  速度: %d%%  舵机: %d°",
                    g_system_status.pump_state ? "开启" : "关闭",
                    g_system_status.pump_speed,
                    g_system_status.servo_angle);
            ESP_LOGI(TAG, "--- 硬件资源 ---");
            ESP_LOGI(TAG, "DRAM: %s / %s (%.1f%% used)",
                    dram_free_str, dram_total_str,
                    g_system_status.dram_total > 0 ?
                    (100.0f * (g_system_status.dram_total - g_system_status.dram_free) / g_system_status.dram_total) : 0);
            if (g_system_status.psram_total > 0) {
                ESP_LOGI(TAG, "PSRAM: %s / %s (%.1f%% used)",
                        psram_free_str, psram_total_str,
                        (100.0f * (g_system_status.psram_total - g_system_status.psram_free) / g_system_status.psram_total));
            } else {
                ESP_LOGI(TAG, "PSRAM: 无");
            }
            ESP_LOGI(TAG, "Flash: %s", flash_total_str);
            ESP_LOGI(TAG, "SPIFFS: %s / %s",
                    spiffs_free_str, spiffs_total_str);
            if (g_system_status.sdcard_mounted) {
                ESP_LOGI(TAG, "TF卡: %s / %s (已挂载)",
                        sdcard_free_str, sdcard_total_str);
            } else {
                ESP_LOGI(TAG, "TF卡: 未挂载");
            }
            ESP_LOGI(TAG, "CPU: %lu MHz  运行时间: %lu秒",
                    (unsigned long)g_system_status.cpu_freq_mhz,
                    (unsigned long)g_system_status.uptime_seconds);
            ESP_LOGI(TAG, "-----------------");
        }

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

    /* 打印系统信息 */
    print_system_info();

    /* 系统初始化 */
    esp_err_t ret = system_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "系统初始化失败，错误码: %s", esp_err_to_name(ret));
        ESP_LOGI(TAG, "系统将重启...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "         系统初始化成功！");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "  请连接Wi-Fi热点: %s", WIFI_SSID);
    ESP_LOGI(TAG, "  密码: %s", WIFI_PASSWORD);
    ESP_LOGI(TAG, "  热点IP: %d.%d.%d.%d", WIFI_AP_IP_1, WIFI_AP_IP_2, WIFI_AP_IP_3, WIFI_AP_IP_4);
    ESP_LOGI(TAG, "  HTTP端口: %d", HTTP_PORT);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "  在浏览器中访问: http://%d.%d.%d.%d:%d",
             WIFI_AP_IP_1, WIFI_AP_IP_2, WIFI_AP_IP_3, WIFI_AP_IP_4, HTTP_PORT);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");

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
        ESP_LOGE(TAG, "创建应用主任务失败");
    }
}
