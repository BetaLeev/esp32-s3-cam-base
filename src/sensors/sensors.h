/**
 * @file sensors.h
 * @brief 传感器模块统一入口
 *
 * 此文件汇总所有传感器子模块，提供统一的初始化和读取接口
 * 子模块位于 sensors/ 目录下：
 *   - thermistor/  热敏电阻
 *   - photosensor/  光敏电阻
 *   - dht11/       DHT11 温湿度
 */
#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief 初始化所有传感器
 */
esp_err_t sensors_init(void);

/**
 * @brief 创建传感器读取任务
 */
esp_err_t sensors_create_task(void);

/* ========================================
 * 子模块统一接口（兼容旧代码）
 * ======================================== */

/* 热敏电阻 */
#include "thermistor/thermistor.h"

/* 光敏电阻 */
#include "photosensor/photosensor.h"

/* DHT11 */
#include "dht11/dht11.h"

/* ========================================
 * 传感器配置接口（引脚可配置）
 * ======================================== */

/**
 * @brief 配置热敏电阻引脚
 * @param gpio GPIO 引脚号
 */
static inline void sensors_thermistor_set_gpio(gpio_num_t gpio) {
    thermistor_set_gpio(gpio);
}

/**
 * @brief 获取热敏电阻引脚
 * @return 当前 GPIO 引脚
 */
static inline gpio_num_t sensors_thermistor_get_gpio(void) {
    return thermistor_get_gpio();
}

/**
 * @brief 配置光敏电阻引脚
 * @param gpio GPIO 引脚号
 */
static inline void sensors_photosensor_set_gpio(gpio_num_t gpio) {
    photosensor_set_gpio(gpio);
}

/**
 * @brief 获取光敏电阻引脚
 * @return 当前 GPIO 引脚
 */
static inline gpio_num_t sensors_photosensor_get_gpio(void) {
    return photosensor_get_gpio();
}

/**
 * @brief 配置 DHT11 引脚
 * @param gpio GPIO 引脚号
 */
static inline void sensors_dht11_set_gpio(gpio_num_t gpio) {
    dht11_set_gpio(gpio);
}

/**
 * @brief 获取 DHT11 引脚
 * @return 当前 GPIO 引脚
 */
static inline gpio_num_t sensors_dht11_get_gpio(void) {
    return dht11_get_gpio();
}

#endif /* SENSORS_H */
