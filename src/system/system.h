/**
 * @file system.h
 * @brief 系统管理模块 - 系统状态管理
 */
#ifndef SYSTEM_H
#define SYSTEM_H

#include "esp_err.h"
#include <stdint.h>

/* 固件版本宏 */
#define FIRMWARE_VERSION "v1.0.0"

/* ========================================
 * 系统管理初始化
 * ======================================== */

/**
 * @brief 系统管理初始化
 */
esp_err_t system_init(void);

/* ========================================
 * 温度传感器接口
 * ======================================== */

/**
 * @brief 获取芯片温度（摄氏度）
 * @return 温度值，失败返回负数
 */
float system_get_chip_temp(void);

/**
 * @brief 温度传感器是否已初始化
 */
bool system_temp_sensor_initialized(void);

/* ========================================
 * 电源管理接口
 * ======================================== */

/**
 * @brief 系统重启
 */
esp_err_t system_reboot(void);

/**
 * @brief 系统关机（进入深度睡眠）
 * @param wakeup_pin 唤醒引脚，-1表示不使用
 * @param wakeup_level 唤醒电平，0或1
 */
esp_err_t system_shutdown(int wakeup_pin, int wakeup_level);

/* ========================================
 * 内存资源接口
 * ======================================== */

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

/* ========================================
 * 系统信息接口
 * ======================================== */

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

/**
 * @brief 获取固件编译时间
 */
const char* system_get_build_time(void);

/**
 * @brief 获取芯片型号
 */
const char* system_get_chip_model(void);

#endif /* SYSTEM_H */
