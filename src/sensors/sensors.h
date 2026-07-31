/**
 * @file sensors.h
 * @brief 传感器模块统一接口
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

/* ========== ADC传感器接口 ========== */

/**
 * @brief 读取热敏电阻原始ADC值
 */
uint32_t sensors_read_thermistor_raw(void);

/**
 * @brief 读取光敏电阻原始ADC值
 */
uint32_t sensors_read_photosensor_raw(void);

/**
 * @brief 计算热敏电阻温度
 * @param raw_value 原始ADC值
 * @return 温度值(摄氏度)
 */
float sensors_calculate_temperature(uint32_t raw_value);

/**
 * @brief 计算光照强度
 * @param raw_value 原始ADC值
 * @return 光照强度(lux)
 */
float sensors_calculate_light(uint32_t raw_value);

/* ========== DHT11传感器接口 ========== */

/**
 * @brief DHT11数据读取结果
 */
typedef struct {
    uint8_t humidity;     /**< 湿度整数值（%） */
    uint8_t temperature; /**< 温度整数值（℃） */
    uint8_t checksum;    /**< 校验和 */
    bool valid;           /**< 数据是否有效 */
} sensors_dht11_data_t;

/**
 * @brief 读取DHT11温湿度数据
 */
esp_err_t sensors_dht11_read(sensors_dht11_data_t *data);

/**
 * @brief 获取DHT11温度
 */
float sensors_dht11_get_temperature(void);

/**
 * @brief 获取DHT11湿度
 */
float sensors_dht11_get_humidity(void);

/**
 * @brief 检查DHT11数据是否有效
 */
bool sensors_dht11_is_valid(void);

#endif // SENSORS_H
