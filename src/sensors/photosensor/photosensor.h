/**
 * @file photosensor.h
 * @brief 光敏电阻传感器模块
 *
 * 支持光敏电阻 (LDR)，通过 ADC 读取光照强度
 */
#ifndef PHOTOSENSOR_H
#define PHOTOSENSOR_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

/* ========================================
 * 传感器配置
 * ======================================== */

/**
 * @brief 默认引脚配置
 */
#ifndef PHOTOSENSOR_DEFAULT_GPIO
#define PHOTOSENSOR_DEFAULT_GPIO  GPIO_NUM_7  /**< 默认 GPIO7 (ADC_CHANNEL_6) */
#endif

/**
 * @brief 分压电阻值 (Ω)
 */
#ifndef PHOTOSENSOR_PULLUP_RESISTOR
#define PHOTOSENSOR_PULLUP_RESISTOR  10000    /**< 10KΩ 上拉电阻 */
#endif

/**
 * @brief ADC 参考电压 (mV)
 */
#ifndef PHOTOSENSOR_VCC
#define PHOTOSENSOR_VCC       3300        /**< 3.3V */
#endif

/**
 * @brief ADC 分辨率
 */
#ifndef PHOTOSENSOR_ADC_MAX
#define PHOTOSENSOR_ADC_MAX   4095        /**< 12位 ADC */
#endif

/**
 * @brief 光照强度范围 (lux)
 */
#define PHOTOSENSOR_MIN_LUX   0           /**< 无光 */
#define PHOTOSENSOR_MAX_LUX   1000        /**< 直射阳光 */

/* ========================================
 * 数据结构
 * ======================================== */

/**
 * @brief 光敏电阻数据
 */
typedef struct {
    uint32_t raw;              /**< ADC 原始值 */
    float lux;                 /**< 计算后的光照强度 (lux) */
} photosensor_data_t;

/* ========================================
 * 接口函数
 * ======================================== */

/**
 * @brief 初始化光敏电阻模块
 * @param gpio ADC 引脚，默认使用 PHOTOSENSOR_DEFAULT_GPIO
 * @return ESP_OK 成功，其他失败
 */
esp_err_t photosensor_init(gpio_num_t gpio);

/**
 * @brief 设置光敏电阻引脚
 * @param gpio GPIO 引脚号
 */
void photosensor_set_gpio(gpio_num_t gpio);

/**
 * @brief 获取当前引脚配置
 * @return 当前使用的 GPIO 引脚
 */
gpio_num_t photosensor_get_gpio(void);

/**
 * @brief 读取原始 ADC 值
 * @return ADC 原始值 (0-4095)
 */
uint32_t photosensor_read_raw(void);

/**
 * @brief 读取光照强度
 * @param data 数据结构体指针，用于存储结果
 * @return ESP_OK 成功
 */
esp_err_t photosensor_read(photosensor_data_t *data);

/**
 * @brief 计算光照强度（根据原始值）
 * @param raw_value ADC 原始值
 * @return 光照强度 (lux)
 */
float photosensor_calculate(uint32_t raw_value);

#endif /* PHOTOSENSOR_H */
