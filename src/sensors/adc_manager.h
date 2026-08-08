/**
 * @file adc_manager.h
 * @brief ADC 资源共享管理器
 *
 * 统一管理 ESP32-S3 的 ADC1 资源，避免多个传感器模块重复初始化导致冲突
 */
#ifndef ADC_MANAGER_H
#define ADC_MANAGER_H

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

/**
 * @brief 初始化 ADC1 单元（单例模式）
 * @return ADC 句柄
 */
adc_oneshot_unit_handle_t adc_manager_init(void);

/**
 * @brief 获取 ADC1 句柄
 * @return ADC 句柄，如果未初始化则返回 NULL
 */
adc_oneshot_unit_handle_t adc_manager_get_handle(void);

/**
 * @brief 反初始化 ADC1 单元
 */
void adc_manager_deinit(void);

#endif /* ADC_MANAGER_H */
