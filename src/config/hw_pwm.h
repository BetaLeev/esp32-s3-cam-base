/**
 * @file hw_pwm.h
 * @brief PWM/LEDC 配置
 */
#ifndef HW_PWM_H
#define HW_PWM_H

#include "driver/ledc.h"

/* ========================================
 * 水泵/电机 PWM 配置
 * ======================================== */
#define LEDC_MOTOR_MODE        LEDC_LOW_SPEED_MODE
#define LEDC_MOTOR_TIMER       LEDC_TIMER_0
#define LEDC_MOTOR_CHANNEL     LEDC_CHANNEL_0
#define LEDC_MOTOR_FREQUENCY   1000        /**< 1kHz */
#define LEDC_MOTOR_DUTY_RES    LEDC_TIMER_10_BIT  /**< 10位分辨率 0-1023 */
#define LEDC_MOTOR_DUTY_MAX    1023        /**< 100% 占空比 */

/* ========================================
 * 舵机 PWM 配置 (SG90)
 * ========================================
 * 
 * LEDC 配置 (50Hz, 14位):
 * - 频率: 50Hz (周期20ms = 20,000µs)
 * - 分辨率: 14位 (0-16383)
 * - 时钟: 1MHz (80MHz/80), 1计数 = 1µs
 * 
 * 占空比计算:
 * - 周期 = 20ms = 20,000µs
 * - 0° = 0.5ms = 500µs = 500 计数
 * - 90° = 1.5ms = 1500µs = 1500 计数
 * - 180° = 2.5ms = 2500µs = 2500 计数
 */
#define LEDC_SERVO_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_SERVO_TIMER      LEDC_TIMER_2
#define LEDC_SERVO_CHANNEL    LEDC_CHANNEL_2
#define LEDC_SERVO_FREQUENCY 50             /**< 50Hz 周期20ms */
#define LEDC_SERVO_DUTY_RES   LEDC_TIMER_14_BIT   /**< 14位分辨率 0-16383 */

/* 舵机占空比 (14位分辨率, 50Hz)
 * SG90 标准范围: 0.5ms - 2.5ms
 */
#define SERVO_DUTY_MIN      500    /**< 0.5ms ≈ 0° */
#define SERVO_DUTY_NEUTRAL  1500   /**< 1.5ms ≈ 90°中立 */
#define SERVO_DUTY_MAX      2500   /**< 2.5ms ≈ 180° */

#endif /* HW_PWM_H */
