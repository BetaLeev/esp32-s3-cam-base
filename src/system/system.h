/**
 * @file system.h
 * @brief 系统管理模块 - 系统状态管理
 */
#ifndef SYSTEM_H
#define SYSTEM_H

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief 系统管理初始化
 */
esp_err_t system_init(void);

/**
 * @brief 获取 DRAM 总大小
 */
uint32_t system_get_dram_total(void);

/**
 * @brief 获取 DRAM 空闲大小
 */
uint32_t system_get_dram_free(void);

/**
 * @brief 获取 PSRAM 总大小
 */
uint32_t system_get_psram_total(void);

/**
 * @brief 获取 PSRAM 空闲大小
 */
uint32_t system_get_psram_free(void);

/**
 * @brief 获取 Flash 总大小
 */
uint32_t system_get_flash_total(void);

/**
 * @brief 获取 Flash 空闲大小
 */
uint32_t system_get_flash_free(void);

/**
 * @brief 获取 SPIFFS 总大小
 */
uint32_t system_get_spiffs_total(void);

/**
 * @brief 获取 SPIFFS 空闲大小
 */
uint32_t system_get_spiffs_free(void);

/**
 * @brief 获取系统运行时间（秒）
 */
uint32_t system_get_uptime_seconds(void);

/**
 * @brief 获取 CPU 频率（MHz）
 */
uint32_t system_get_cpu_freq_mhz(void);

/**
 * @brief 获取固件版本
 */
uint32_t system_get_version(void);

#endif /* SYSTEM_H */
