/**
 * @file sensors.h
 * @brief 传感器模块统一入口
 */
#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

/**
 * @brief 初始化所有传感器
 */
esp_err_t sensors_init(void);

/**
 * @brief 创建传感器读取任务
 */
esp_err_t sensors_create_task(void);

/* ========================================
 * 子模块统一接口
 * ======================================== */
#include "thermistor/thermistor.h"
#include "photosensor/photosensor.h"
#include "dht11/dht11.h"

/* ========================================
 * 传感器配置接口（引脚可配置）
 * ======================================== */

static inline void sensors_thermistor_set_gpio(gpio_num_t gpio) {
    thermistor_set_gpio(gpio);
}

static inline gpio_num_t sensors_thermistor_get_gpio(void) {
    return thermistor_get_gpio();
}

static inline void sensors_photosensor_set_gpio(gpio_num_t gpio) {
    photosensor_set_gpio(gpio);
}

static inline gpio_num_t sensors_photosensor_get_gpio(void) {
    return photosensor_get_gpio();
}

static inline void sensors_dht11_set_gpio(gpio_num_t gpio) {
    dht11_set_gpio(gpio);
}

static inline gpio_num_t sensors_dht11_get_gpio(void) {
    return dht11_get_gpio();
}

#endif /* SENSORS_H */