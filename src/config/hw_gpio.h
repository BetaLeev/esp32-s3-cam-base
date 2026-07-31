/**
 * @file hw_gpio.h
 * @brief GPIO 引脚配置
 */
#ifndef HW_GPIO_H
#define HW_GPIO_H

#include "driver/gpio.h"

/* ========================================
 * 电机驱动 (TB6612) 引脚
 * ======================================== */
/**
 * @brief TB6612 电机驱动引脚
 * - STBY 引脚已硬件连接到 3.3V
 */
#define GPIO_MOTOR_PWMA        GPIO_NUM_1
#define GPIO_MOTOR_AIN1        GPIO_NUM_2
#define GPIO_MOTOR_AIN2        GPIO_NUM_42

/* ========================================
 * 传感器引脚
 * ======================================== */
/**
 * @brief DHT11 温湿度传感器 (单总线)
 */
#define GPIO_DHT11             GPIO_NUM_4

/**
 * @brief ADC 传感器 (使用 GPIO1 和 GPIO5 避免冲突)
 */
#define GPIO_ADC_THERMISTOR    GPIO_NUM_5   /**< 热敏电阻 */
#define GPIO_ADC_PHOTOSENSOR   GPIO_NUM_3   /**< 光敏电阻 */

/* ========================================
 * 执行器引脚
 * ======================================== */
/**
 * @brief 舵机控制引脚 (SG90)
 */
#define GPIO_SERVO             GPIO_NUM_48

/* ========================================
 * TF 卡 (SD卡) 引脚 - SDMMC 1-bit 模式
 * ======================================== */
#define GPIO_SD_CLK            GPIO_NUM_39
#define GPIO_SD_CMD            GPIO_NUM_38
#define GPIO_SD_D0             GPIO_NUM_40

/* ========================================
 * I2C 引脚 (预留 - OLED, IMU 等)
 * ======================================== */
#define GPIO_I2C_SCL           GPIO_NUM_41
#define GPIO_I2C_SDA           GPIO_NUM_42

/* ========================================
 * SPI 引脚 (预留 - LCD, 射频模块等)
 * ======================================== */
#define GPIO_SPI_CLK           GPIO_NUM_36
#define GPIO_SPI_MOSI          GPIO_NUM_35
#define GPIO_SPI_MISO          GPIO_NUM_37
#define GPIO_SPI_CS            GPIO_NUM_34

/* ========================================
 * 扩展引脚 (预留)
 * ======================================== */
#define GPIO_EXT_1              GPIO_NUM_6
#define GPIO_EXT_2              GPIO_NUM_7
#define GPIO_EXT_3              GPIO_NUM_8
#define GPIO_EXT_4              GPIO_NUM_9

#endif /* HW_GPIO_H */
