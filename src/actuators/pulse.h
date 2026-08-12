/**
 * @file pulse.h
 * @brief 脉冲控制模块
 */

#ifndef PULSE_H
#define PULSE_H

#include "esp_err.h"
#include <stdint.h>

/* ========================================
 * 脉冲模式定义
 * ======================================== */
typedef enum {
    PULSE_MODE_SINGLE = 0,     /**< 单次脉冲 */
    PULSE_MODE_CONTINUOUS = 1  /**< 连续脉冲 */
} pulse_mode_t;

/* ========================================
 * 脉冲配置结构
 * ======================================== */
typedef struct {
    int pin;                   /**< GPIO引脚号 */
    pulse_mode_t mode;         /**< 脉冲模式 */
    uint8_t intensity;         /**< 强度 0-100% */
    uint32_t frequency;        /**< 频率 1-1000 Hz */
    uint32_t pulse_width;      /**< 脉冲宽度 ms（单次模式） */
    bool enabled;              /**< 是否启用 */
} pulse_config_t;

/* ========================================
 * 脉冲状态结构
 * ======================================== */
typedef struct {
    uint8_t current_intensity; /**< 当前强度 */
    uint32_t pulse_count;      /**< 已发送脉冲数 */
    float elapsed_time;        /**< 运行时间（秒） */
    uint8_t pin_level;         /**< 引脚电平 */
} pulse_status_t;

/* ========================================
 * 函数声明
 * ======================================== */

/**
 * @brief 初始化脉冲控制模块
 */
esp_err_t pulse_init(void);

/**
 * @brief 停止脉冲控制
 */
esp_err_t pulse_deinit(void);

/**
 * @brief 启动脉冲
 * @param pin GPIO引脚号
 * @param mode 模式 single/continuous
 * @param intensity 强度 0-100%
 * @param frequency 频率 Hz
 * @param pulse_width 脉冲宽度 ms（单次模式）
 */
esp_err_t pulse_start(int pin, pulse_mode_t mode, uint8_t intensity,
                      uint32_t frequency, uint32_t pulse_width);

/**
 * @brief 停止脉冲
 */
esp_err_t pulse_stop(void);

/**
 * @brief 获取当前配置
 */
const pulse_config_t* pulse_get_config(void);

/**
 * @brief 获取当前状态
 */
void pulse_get_status(pulse_status_t *status);

#endif // PULSE_H
