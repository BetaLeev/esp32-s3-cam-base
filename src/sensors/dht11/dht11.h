/**
 * @file dht11.h
 * @brief DHT11 温湿度传感器模块
 *
 * 支持 DHT11 单总线数字温湿度传感器
 */
#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

/* ========================================
 * 传感器配置
 * ======================================== */

/**
 * @brief 默认引脚配置
 */
#ifndef DHT11_DEFAULT_GPIO
#define DHT11_DEFAULT_GPIO  GPIO_NUM_4  /**< 默认 GPIO4 */
#endif

/**
 * @brief 读取超时时间 (us)
 */
#ifndef DHT11_TIMEOUT_US
#define DHT11_TIMEOUT_US   100         /**< 位读取超时 */
#endif

/**
 * @brief 读取间隔 (ms)
 * @note DHT11 建议采样间隔不小于 1 秒
 */
#ifndef DHT11_READ_INTERVAL_MS
#define DHT11_READ_INTERVAL_MS  2000   /**< 2 秒 */
#endif

/* ========================================
 * 数据结构
 * ======================================== */

/**
 * @brief DHT11 数据
 */
typedef struct {
    float temperature;     /**< 温度值 (°C) */
    float humidity;        /**< 湿度值 (%) */
    uint8_t checksum;      /**< 校验和 */
    bool valid;            /**< 数据是否有效 */
} dht11_data_t;

/* ========================================
 * 接口函数
 * ======================================== */

/**
 * @brief 初始化 DHT11 模块
 * @param gpio 数据引脚，默认使用 DHT11_DEFAULT_GPIO
 * @return ESP_OK 成功，其他失败
 */
esp_err_t dht11_init(gpio_num_t gpio);

/**
 * @brief 设置 DHT11 引脚
 * @param gpio GPIO 引脚号
 */
void dht11_set_gpio(gpio_num_t gpio);

/**
 * @brief 获取当前引脚配置
 * @return 当前使用的 GPIO 引脚
 */
gpio_num_t dht11_get_gpio(void);

/**
 * @brief 读取温湿度数据
 * @param data 数据结构体指针，用于存储结果
 * @return ESP_OK 成功，ESP_ERR_TIMEOUT 超时，ESP_ERR_INVALID_CRC 校验失败
 */
esp_err_t dht11_read(dht11_data_t *data);

/**
 * @brief 获取最近一次温度值
 * @return 温度值 (°C)，无效时返回 0
 */
float dht11_get_temperature(void);

/**
 * @brief 获取最近一次湿度值
 * @return 湿度值 (%)，无效时返回 0
 */
float dht11_get_humidity(void);

/**
 * @brief 检查数据是否有效
 * @return true 有效，false 无效
 */
bool dht11_is_valid(void);

#endif /* DHT11_H */
