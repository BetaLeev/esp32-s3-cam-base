/**
 * @file adc_manager.c
 * @brief ADC 资源共享管理器实现
 *
 * 使用单例模式管理 ADC1 单元，所有需要使用 ADC 的传感器模块
 * 都应该通过此管理器获取 ADC 句柄，避免重复初始化导致冲突。
 */

#include "adc_manager.h"
#include "../config.h"

static const char *TAG = "ADC_MANAGER";
#define LOG_TAG TAG

/* 单例句柄 */
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static bool s_initialized = false;

adc_oneshot_unit_handle_t adc_manager_init(void)
{
    if (s_initialized && s_adc_handle != NULL) {
        return s_adc_handle;
    }

    if (s_adc_handle != NULL) {
        SENSORS_LOGW(TAG, "ADC1 已初始化但状态异常，重新初始化");
        s_initialized = false;
    }

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };

    esp_err_t ret = adc_oneshot_new_unit(&init_config, &s_adc_handle);
    if (ret != ESP_OK) {
        SENSORS_LOGE(TAG, "ADC1 初始化失败: %s", esp_err_to_name(ret));
        s_adc_handle = NULL;
        s_initialized = false;
        return NULL;
    }

    s_initialized = true;
    SENSORS_LOGI(TAG, "ADC1 初始化成功");

    return s_adc_handle;
}

adc_oneshot_unit_handle_t adc_manager_get_handle(void)
{
    if (!s_initialized || s_adc_handle == NULL) {
        return adc_manager_init();
    }
    return s_adc_handle;
}

void adc_manager_deinit(void)
{
    if (s_adc_handle != NULL) {
        SENSORS_LOGI(TAG, "ADC1 反初始化");
        // 注意：ESP-IDF 的 adc_oneshot_del_unit 在某些版本可能不支持
        // 如果需要反初始化，可以使用 adc_oneshot_del_unit(s_adc_handle);
        s_adc_handle = NULL;
        s_initialized = false;
    }
}
