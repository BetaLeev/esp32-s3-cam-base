/**
 * @file hw_adc.h
 * @brief ADC 配置
 *
 * @note 此文件只定义 ADC 配置宏，不包含头文件
 *       使用此配置的文件需要包含: #include "esp_adc/adc_types.h"
 */
#ifndef HW_ADC_H
#define HW_ADC_H

/* ========================================
 * ADC 通道配置
 * ======================================== */
/**
 * @brief ADC1 通道分配
 * - GPIO1 已用于电机 PWM
 * - GPIO3 = ADC_CHANNEL_2 (光敏)
 * - GPIO5 = ADC_CHANNEL_4 (热敏)
 */
#define ADC_THERMISTOR_ATTE     ADC_ATTEN_DB_11       /**< 0-3300mV */
#define ADC_THERMISTOR_WIDTH    ADC_BITWIDTH_12       /**< 12位精度 */
#define ADC_THERMISTOR_CHANNEL  ADC_CHANNEL_4          /**< GPIO5 */

#define ADC_PHOTOSENSOR_ATTE   ADC_ATTEN_DB_11       /**< 0-3300mV */
#define ADC_PHOTOSENSOR_WIDTH  ADC_BITWIDTH_12       /**< 12位精度 */
#define ADC_PHOTOSENSOR_CHANNEL ADC_CHANNEL_6         /**< GPIO7 */

/* ========================================
 * ADC 采样配置
 * ======================================== */
#define ADC_SAMPLE_COUNT       64          /**< 采样次数取平均 */
#define ADC_SAMPLE_DELAY_MS    5           /**< 采样间隔 */

/* ========================================
 * ADC 校准 (预留)
 * ======================================== */
#define ADC_USE_CALIBRATION    0           /**< 0=禁用 1=启用校准 */

/* ========================================
 * 传感器转换参数 (根据实际硬件调整)
 * ======================================== */
/**
 * @brief 热敏电阻 NTC 10K 参数
 * - B值: 3950
 * - 25°C 阻值: 10KΩ
 */
#define THERMISTOR_B_VALUE     3950
#define THERMISTOR_R0          10000       /**< 10KΩ @ 25°C */
#define THERMISTOR_T0          298.15       /**< 25°C in Kelvin */

/**
 * @brief 光敏电阻参数 (根据实际型号调整)
 */
#define PHOTOSENSOR_MIN_LUX    0           /**< 最小光照 (暗) */
#define PHOTOSENSOR_MAX_LUX    1000        /**< 最大光照 (直射阳光) */
#define PHOTOSENSOR_RESISTOR   10000       /**< 分压电阻 10KΩ */

#endif /* HW_ADC_H */
