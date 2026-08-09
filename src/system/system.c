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

static const char *TAG = "SYSTEM";

static int64_t g_system_start_time = 0;

/**
 * @brief 系统管理初始化
 */
esp_err_t system_init(void) {
    g_system_start_time = esp_timer_get_time();
    ESP_LOGI(TAG, "系统管理模块初始化完成");
    return ESP_OK;
}

/**
 * @brief 获取 DRAM 总大小
 */
uint32_t system_get_dram_total(void) {
    return heap_caps_get_total_size(MALLOC_CAP_8BIT);
}

/**
 * @brief 获取 DRAM 空闲大小
 */
uint32_t system_get_dram_free(void) {
    return heap_caps_get_free_size(MALLOC_CAP_8BIT);
}

/**
 * @brief 获取 PSRAM 总大小
 */
uint32_t system_get_psram_total(void) {
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
}

/**
 * @brief 获取 PSRAM 空闲大小
 */
uint32_t system_get_psram_free(void) {
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}

/**
 * @brief 获取 Flash 总大小
 */
uint32_t system_get_flash_total(void) {
    uint32_t flash_size = 0;
    if (esp_flash_get_size(esp_flash_default_chip, &flash_size) == ESP_OK) {
        return flash_size;
    }
    return 0;
}

/**
 * @brief 获取 Flash 空闲大小
 */
uint32_t system_get_flash_free(void) {
    return system_get_flash_total();
}

/**
 * @brief 获取 SPIFFS 总大小
 */
uint32_t system_get_spiffs_total(void) {
    return g_system_status.spiffs_total;
}

/**
 * @brief 获取 SPIFFS 空闲大小
 */
uint32_t system_get_spiffs_free(void) {
    return g_system_status.spiffs_free;
}

/**
 * @brief 获取系统运行时间（秒）
 */
uint32_t system_get_uptime_seconds(void) {
    if (g_system_start_time > 0) {
        return (uint32_t)((esp_timer_get_time() - g_system_start_time) / 1000000);
    }
    return 0;
}

/**
 * @brief 获取 CPU 频率（MHz）
 */
uint32_t system_get_cpu_freq_mhz(void) {
    uint32_t cpu_freq_hz = 0;
    if (esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_CACHED,
                                     &cpu_freq_hz) == ESP_OK) {
        return cpu_freq_hz / 1000000;
    }
    return 240;  // 默认值
}

/**
 * @brief 获取固件版本
 */
uint32_t system_get_version(void) {
    return g_system_status.version;
}
