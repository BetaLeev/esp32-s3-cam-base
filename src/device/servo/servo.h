/**
 * @file servo.h
 * @brief 舵机控制模块接口
 */

#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief 初始化舵机模块
 */
esp_err_t servo_init(void);

/**
 * @brief 设置舵机角度
 * @param angle 角度 (0-180)
 */
esp_err_t servo_set_angle(uint8_t angle);

/**
 * @brief 获取舵机当前角度
 */
uint8_t servo_get_angle(void);

#endif // SERVO_H
