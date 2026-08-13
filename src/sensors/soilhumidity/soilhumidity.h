/**
 * @file soilhumidity.h
 * @brief 土壤湿度传感器模块
 *
 * 支持 LM393 土壤湿度传感器，通过 ADC 读取土壤湿度
 * 湿度越高，ADC 值越低（传感器特性）
 */
#ifndef SOILHUMIDITY_H
#define SOILHUMIDITY_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

/* ========================================
 * 传感器配置
 * ======================================== */

/**
 * @brief 默认引脚配置
 * @note 可通过 soilhumidity_set_gpio() 动态修改
 */
#ifndef SOILHUMIDITY_DEFAULT_GPIO
#define SOILHUMIDITY_DEFAULT_GPIO  GPIO_SOILHUMIDITY  /**< 默认 GPIO47 (ADC_CHANNEL_7) */
#endif

/**
 * @brief ADC 参考电压 (mV)
 */
#ifndef SOILHUMIDITY_VCC
#define SOILHUMIDITY_VCC          3300        /**< 3.3V */
#endif

/**
 * @brief ADC 分辨率
 */
#ifndef SOILHUMIDITY_ADC_MAX
#define SOILHUMIDITY_ADC_MAX      4095        /**< 12位 ADC */
#endif

/**
 * @brief 湿度范围限制
 */
#define SOILHUMIDITY_MIN          0.0f        /**< 最小湿度 (%) */
#define SOILHUMIDITY_MAX          100.0f      /**< 最大湿度 (%) */

/**
 * @brief LM393 传感器参数
 * - 干土 ADC 值: ~4095 (3.3V)
 * - 湿土 ADC 值: ~0-500 (接近0V)
 */
#ifndef SOILHUMIDITY_DRY_VALUE
#define SOILHUMIDITY_DRY_VALUE    4095        /**< 干土 ADC 值 */
#endif

#ifndef SOILHUMIDITY_WET_VALUE
#define SOILHUMIDITY_WET_VALUE    500         /**< 湿土 ADC 值 (下限) */
#endif

/* ========================================
 * 数据结构
 * ======================================== */

/**
 * @brief 土壤湿度数据
 */
typedef struct {
    uint32_t raw;              /**< ADC 原始值 */
    float humidity;            /**< 计算后的湿度值 (%) */
} soilhumidity_data_t;

/* ========================================
 * 接口函数
 * ======================================== */

/**
 * @brief 初始化土壤湿度传感器模块
 * @param gpio ADC 引脚，默认使用 SOILHUMIDITY_DEFAULT_GPIO
 * @return ESP_OK 成功，其他失败
 */
esp_err_t soilhumidity_init(gpio_num_t gpio);

/**
 * @brief 设置土壤湿度传感器引脚
 * @param gpio GPIO 引脚号
 */
void soilhumidity_set_gpio(gpio_num_t gpio);

/**
 * @brief 获取当前引脚配置
 * @return 当前使用的 GPIO 引脚
 */
gpio_num_t soilhumidity_get_gpio(void);

/**
 * @brief 读取原始 ADC 值
 * @return ADC 原始值 (0-4095)
 */
uint32_t soilhumidity_read_raw(void);

/**
 * @brief 读取土壤湿度
 * @param data 数据结构体指针，用于存储结果
 * @return ESP_OK 成功
 */
esp_err_t soilhumidity_read(soilhumidity_data_t *data);

/**
 * @brief 计算土壤湿度百分比（根据原始值）
 * @param raw_value ADC 原始值
 * @return 湿度值 (%)
 */
float soilhumidity_calculate(uint32_t raw_value);

/**
 * @brief 获取当前土壤湿度状态描述
 * @param humidity 湿度值 (%)
 * @return 状态描述字符串
 */
const char* soilhumidity_get_status(float humidity);

#endif /* SOILHUMIDITY_H */
