/**
 * @file hw_pwm.h
 * @brief PWM/LEDC 配置
 */
#ifndef HW_PWM_H
#define HW_PWM_H

#include "driver/ledc.h"

/* ========================================
 * 电机 PWM 配置
 * ======================================== */
#define LEDC_MOTOR_TIMER       LEDC_TIMER_0
#define LEDC_MOTOR_MODE        LEDC_LOW_SPEED_MODE
#define LEDC_MOTOR_CHANNEL     LEDC_CHANNEL_0
#define LEDC_MOTOR_FREQUENCY   1000        /**< 1kHz */
#define LEDC_MOTOR_DUTY_RES    LEDC_TIMER_10_BIT  /**< 10位分辨率 0-1023 */
#define LEDC_MOTOR_DUTY_MAX    1023        /**< 100% 占空比 */

/* ========================================
 * 舵机 PWM 配置
 * ======================================== */
#define LEDC_SERVO_TIMER       LEDC_TIMER_1
#define LEDC_SERVO_CHANNEL     LEDC_CHANNEL_1
#define LEDC_SERVO_FREQUENCY   50          /**< 50Hz (20ms周期) */
#define LEDC_SERVO_DUTY_RES    LEDC_TIMER_16_BIT /**< 16位分辨率 */

/* 舵机角度对应的占空比 (对于50Hz, 16位分辨率) */
#define SERVO_DUTY_MIN         1638       /**< 0度 (~1ms) */
#define SERVO_DUTY_MAX         8192       /**< 180度 (~2ms) */
#define SERVO_DUTY_NEUTRAL     4915       /**< 90度 (~1.5ms) */

/* 角度到占空比计算: duty = (angle/180 * (DUTY_MAX - DUTY_MIN)) + DUTY_MIN */

/* ========================================
 * RGB LED PWM 配置 (预留)
 * ======================================== */
#define LEDC_RGB_TIMER         LEDC_TIMER_2
#define LEDC_RGB_FREQUENCY     5000        /**< 5kHz */

/* ========================================
 * PWM 通道分配表
 * ======================================== */
/**
 * LEDC 通道分配:
 * - CH0: 电机 PWM
 * - CH1: 舵机 PWM
 * - CH2: RGB LED R (预留)
 * - CH3: RGB LED G (预留)
 * - CH4: RGB LED B (预留)
 */

#endif /* HW_PWM_H */
