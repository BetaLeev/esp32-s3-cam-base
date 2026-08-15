/**
 * @file motor.h
 * @brief 电机控制模块接口
 */

#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ========== 水泵档位定义 ========== */

/**
 * @brief 水泵档位定义
 */
typedef enum {
    PUMP_OFF = 0,       /**< 关闭 */
    PUMP_LOW = 1,       /**< 低档 - 30%速度 */
    PUMP_MEDIUM = 2,    /**< 中档 - 60%速度 */
    PUMP_HIGH = 3       /**< 高档 - 100%速度 */
} pump_gear_t;

/**
 * @brief 水泵档位速度配置
 */
#define PUMP_SPEED_LOW     30   /**< 低档速度 30% */
#define PUMP_SPEED_MEDIUM  60   /**< 中档速度 60% */
#define PUMP_SPEED_HIGH   100   /**< 高档速度 100% */
#define PUMP_SPEED_MIN     20   /**< 最小有效速度，防止电机不动 */

/* ========== 电机接口 ========== */

/**
 * @brief 初始化电机模块
 */
esp_err_t motor_init(void);

/**
 * @brief 电机停止
 */
esp_err_t motor_stop(void);

/**
 * @brief 电机开启
 */
esp_err_t motor_start(void);

/**
 * @brief 设置电机速度
 * @param speed 速度值 0-100%
 */
esp_err_t motor_set_speed(uint8_t speed);

/**
 * @brief 获取电机当前速度
 */
uint8_t motor_get_speed(void);

/**
 * @brief 设置电机状态（开/关）
 * @param state true=开启, false=关闭
 */
esp_err_t motor_set_state(bool state);

/**
 * @brief 获取电机状态
 */
bool motor_get_state(void);

/* ========== 水泵档位接口 ========== */

/**
 * @brief 设置水泵档位
 * @param gear 档位 (PUMP_OFF/PUMP_LOW/PUMP_MEDIUM/PUMP_HIGH)
 */
esp_err_t motor_pump_set_gear(pump_gear_t gear);

/**
 * @brief 获取当前档位
 */
pump_gear_t motor_pump_get_gear(void);

/**
 * @brief 获取当前档位名称
 */
const char* motor_pump_get_gear_name(pump_gear_t gear);

#endif // MOTOR_H
