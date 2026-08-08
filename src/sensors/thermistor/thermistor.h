/**
 * @file thermistor.h
 * @brief 热敏电阻传感器模块
 *
 * 支持 NTC 10K 热敏电阻，通过 ADC 读取温度
 */
#ifndef THERMISTOR_H
#define THERMISTOR_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

/* ========================================
 * 传感器配置
 * ======================================== */

/**
 * @brief 默认引脚配置
 * @note 可通过 thermistor_set_gpio() 动态修改
 */
#ifndef THERMISTOR_DEFAULT_GPIO
#define THERMISTOR_DEFAULT_GPIO  GPIO_NUM_5  /**< 默认 GPIO5 (ADC_CHANNEL_4) */
#endif

/**
 * @brief NTC 热敏电阻参数
 */
#ifndef THERMISTOR_B_VALUE
#define THERMISTOR_B_VALUE      3950        /**< B值 */
#endif
#ifndef THERMISTOR_R0
#define THERMISTOR_R0           10000       /**< 标称阻值 10KΩ @ 25°C */
#endif
#ifndef THERMISTOR_T0
#define THERMISTOR_T0           298.15      /**< 标称温度 25°C (Kelvin) */
#endif

/**
 * @brief 分压电阻值 (Ω)
 */
#ifndef THERMISTOR_PULLUP_RESISTOR
#define THERMISTOR_PULLUP_RESISTOR  10000    /**< 10KΩ 上拉电阻 */
#endif

/**
 * @brief ADC 参考电压 (mV)
 */
#ifndef THERMISTOR_VCC
#define THERMISTOR_VCC          3300        /**< 3.3V */
#endif

/**
 * @brief ADC 分辨率
 */
#ifndef THERMISTOR_ADC_MAX
#define THERMISTOR_ADC_MAX      4095        /**< 12位 ADC */
#endif

/**
 * @brief 温度范围限制
 */
#define THERMISTOR_MIN_TEMP     -40.0f      /**< 最小温度 (°C) */
#define THERMISTOR_MAX_TEMP     125.0f      /**< 最大温度 (°C) */

/* ========================================
 * 数据结构
 * ======================================== */

/**
 * @brief 热敏电阻数据
 */
typedef struct {
    uint32_t raw;              /**< ADC 原始值 */
    float temperature;        /**< 计算后的温度值 (°C) */
} thermistor_data_t;

/* ========================================
 * 接口函数
 * ======================================== */

/**
 * @brief 初始化热敏电阻模块
 * @param gpio ADC 引脚，默认使用 THERMISTOR_DEFAULT_GPIO
 * @return ESP_OK 成功，其他失败
 */
esp_err_t thermistor_init(gpio_num_t gpio);

/**
 * @brief 设置热敏电阻引脚
 * @param gpio GPIO 引脚号
 */
void thermistor_set_gpio(gpio_num_t gpio);

/**
 * @brief 获取当前引脚配置
 * @return 当前使用的 GPIO 引脚
 */
gpio_num_t thermistor_get_gpio(void);

/**
 * @brief 读取原始 ADC 值
 * @return ADC 原始值 (0-4095)
 */
uint32_t thermistor_read_raw(void);

/**
 * @brief 读取温度值
 * @param data 数据结构体指针，用于存储结果
 * @return ESP_OK 成功
 */
esp_err_t thermistor_read(thermistor_data_t *data);

/**
 * @brief 计算温度（根据原始值）
 * @param raw_value ADC 原始值
 * @return 温度值 (°C)
 */
float thermistor_calculate(uint32_t raw_value);

#endif /* THERMISTOR_H */
