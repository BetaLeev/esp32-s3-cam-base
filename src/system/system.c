/**
 * @file system.c
 * @brief 系统管理模块实现
 */
#include "system.h"
#include "../config.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_flash.h"
#include "esp_clk_tree.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "driver/temperature_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "string.h"
#include "esp_wifi.h"

static const char *TAG = "SYSTEM";

/* 系统启动时间 */
static int64_t g_system_start_time = 0;

/* 睡眠模式全局变量 */
char g_sleep_mode[16] = "deep";  // 'light' 轻度睡眠, 'deep' 深度睡眠

/* 温度传感器状态 */
static bool s_temp_sensor_ok = false;
static float s_last_chip_temp = 0.0f;
static temperature_sensor_handle_t s_temp_handle = NULL;

/* 固件信息 - FIRMWARE_VERSION 在 system.h 中定义 */
#define FIRMWARE_BUILD_TIME __DATE__ " " __TIME__
#define CHIP_MODEL "ESP32-S3"

/* ========================================
 * 温度传感器任务
 * ======================================== */

#define TEMP_TASK_STACK_SIZE 2048
#define TEMP_READ_INTERVAL_MS 2000  // 2秒读取一次

/**
 * @brief 温度传感器任务
 */
static void temp_sensor_task(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "温度传感器任务启动，读取间隔: %dms", TEMP_READ_INTERVAL_MS);

    while (1) {
        if (s_temp_sensor_ok) {
            float temp = system_get_chip_temp();
            if (temp < 0) {
                ESP_LOGW(TAG, "读取温度失败");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(TEMP_READ_INTERVAL_MS));
    }
}

/**
 * @brief 初始化片内温度传感器
 */
static esp_err_t temp_sensor_init(void)
{
    ESP_LOGI(TAG, "初始化片内温度传感器...");

    // ESP32-S3 片内温度传感器配置
    temperature_sensor_config_t temp_config = {
        .range_min = 20,   // 量程最小值 (°C)
        .range_max = 100,  // 量程最大值 (°C)
    };

    esp_err_t ret = temperature_sensor_install(&temp_config, &s_temp_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "安装温度传感器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 启动温度传感器
    ret = temperature_sensor_enable(s_temp_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动温度传感器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 等待传感器稳定
    vTaskDelay(pdMS_TO_TICKS(100));

    // 验证读取
    float test_temp = 0;
    ret = temperature_sensor_get_celsius(s_temp_handle, &test_temp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取温度传感器测试值失败: %s", esp_err_to_name(ret));
        return ret;
    }

    s_temp_sensor_ok = true;
    ESP_LOGI(TAG, "温度传感器初始化成功，测试温度: %.1f°C", test_temp);

    return ESP_OK;
}

/* ========================================
 * 公共接口实现
 * ======================================== */

esp_err_t system_init(void)
{
    ESP_LOGI(TAG, "系统管理模块初始化...");

    g_system_start_time = esp_timer_get_time();

    // 初始化温度传感器
    esp_err_t ret = temp_sensor_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "温度传感器初始化失败，继续运行");
        // 不影响主流程，继续运行
    }

    // 创建温度读取任务
    BaseType_t task_ret = xTaskCreate(
        temp_sensor_task,
        "temp_sensor",
        TEMP_TASK_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "创建温度传感器任务失败");
    } else {
        ESP_LOGI(TAG, "温度传感器任务已创建");
    }

    ESP_LOGI(TAG, "系统管理模块初始化完成");
    ESP_LOGI(TAG, "固件版本: %s", FIRMWARE_VERSION);
    ESP_LOGI(TAG, "编译时间: %s", FIRMWARE_BUILD_TIME);
    ESP_LOGI(TAG, "芯片型号: %s", CHIP_MODEL);

    return ESP_OK;
}

float system_get_chip_temp(void)
{
    if (!s_temp_sensor_ok || s_temp_handle == NULL) {
        return -1.0f;
    }

    float temp = 0;
    esp_err_t ret = temperature_sensor_get_celsius(s_temp_handle, &temp);
    if (ret == ESP_OK) {
        s_last_chip_temp = temp;
        return temp;
    }

    return -1.0f;
}

bool system_temp_sensor_initialized(void)
{
    return s_temp_sensor_ok;
}

esp_err_t system_reboot(void)
{
    ESP_LOGW(TAG, "系统即将重启...");
    esp_restart();
    return ESP_OK;  // 不会执行到这里
}

esp_err_t system_shutdown(int wakeup_pin, int wakeup_level)
{
    // 检查是否有睡眠模式参数（在外部通过 NVS 或全局变量传递）
    // 默认使用深度睡眠
    extern char g_sleep_mode[16];
    bool is_light_sleep = (strcmp(g_sleep_mode, "light") == 0);

    if (is_light_sleep) {
        ESP_LOGW(TAG, "系统即将进入轻度睡眠...");
        // 轻度睡眠：Wi-Fi断开，CPU暂停，但程序状态保持
        esp_wifi_disconnect();
        esp_wifi_stop();

        // 配置定时唤醒
        esp_sleep_enable_timer_wakeup(60 * 1000000);  // 60秒后唤醒
        ESP_LOGI(TAG, "已配置轻度睡眠，60秒后自动唤醒");

        // 进入轻度睡眠
        esp_light_sleep_start();

        ESP_LOGI(TAG, "系统从轻度睡眠唤醒");
    } else {
        ESP_LOGW(TAG, "系统即将进入深度睡眠...");

        if (wakeup_pin >= 0) {
            // 配置唤醒源
            esp_sleep_enable_ext0_wakeup(wakeup_pin, wakeup_level);
            ESP_LOGI(TAG, "已配置唤醒引脚: GPIO%d, 电平: %d", wakeup_pin, wakeup_level);
        } else {
            // 只使用定时器唤醒（短暂睡眠后自动唤醒，防止完全死机）
            esp_sleep_enable_timer_wakeup(60 * 1000000);  // 60秒后唤醒
            ESP_LOGI(TAG, "已配置定时器唤醒: 60秒");
        }

        esp_deep_sleep_start();
    }

    return ESP_OK;
}

uint32_t system_get_dram_total(void)
{
    return heap_caps_get_total_size(MALLOC_CAP_8BIT);
}

uint32_t system_get_dram_free(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

uint32_t system_get_psram_total(void)
{
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
}

uint32_t system_get_psram_free(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

uint32_t system_get_flash_total(void)
{
    uint32_t flash_size = 0;
    if (esp_flash_get_size(esp_flash_default_chip, &flash_size) == ESP_OK) {
        return flash_size;
    }
    return 0;
}

uint32_t system_get_flash_free(void)
{
    return system_get_flash_total();
}

uint32_t system_get_spiffs_total(void)
{
    return g_system_status.spiffs_total;
}

uint32_t system_get_spiffs_free(void)
{
    return g_system_status.spiffs_free;
}

uint32_t system_get_uptime_seconds(void)
{
    if (g_system_start_time > 0) {
        return (uint32_t)((esp_timer_get_time() - g_system_start_time) / 1000000);
    }
    return 0;
}

uint32_t system_get_cpu_freq_mhz(void)
{
    uint32_t cpu_freq_hz = 0;
    if (esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED,
                                     &cpu_freq_hz) == ESP_OK) {
        return cpu_freq_hz / 1000000;
    }
    return 240;
}

uint32_t system_get_version(void)
{
    return g_system_status.version;
}

const char* system_get_build_time(void)
{
    return FIRMWARE_BUILD_TIME;
}

const char* system_get_chip_model(void)
{
    return CHIP_MODEL;
}
