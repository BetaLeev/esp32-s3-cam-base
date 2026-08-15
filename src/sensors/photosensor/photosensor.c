/**
 * @file photosensor.c
 * @brief 光敏电阻传感器实现
 *
 * 使用光敏电阻 (LDR)，通过分压电路连接 ADC
 * 光敏电阻特性：光照越强，阻值越小，ADC 值越大
 */

#include "photosensor.h"
#include "../../config.h"
#include "esp_adc/adc_oneshot.h"
#include "../adc_manager.h"
#include <math.h>

#define LOG_TAG "PHOTOSENSOR"

/* ========================================
 * 内部变量
 * ======================================== */
static adc_channel_t s_adc_channel = ADC_CHANNEL_6;  /**< 默认 GPIO7 */
static gpio_num_t s_gpio = PHOTOSENSOR_DEFAULT_GPIO;

/* ========================================
 * 内部函数
 * ======================================== */

static esp_err_t adc_init(void)
{
    adc_oneshot_unit_handle_t handle = adc_manager_get_handle();
    if (handle == NULL) {
        SENSORS_LOGE(TAG, "ADC 句柄获取失败");
        return ESP_FAIL;
    }

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };

    esp_err_t ret = adc_oneshot_config_channel(handle, s_adc_channel, &config);
    if (ret != ESP_OK) {
        SENSORS_LOGE(TAG, "ADC 通道配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    SENSORS_LOGI(TAG, "ADC 初始化完成, GPIO%d, 通道 %d", s_gpio, s_adc_channel);
    return ESP_OK;
}

/* ========================================
 * 接口实现
 * ======================================== */

esp_err_t photosensor_init(gpio_num_t gpio)
{
    s_gpio = gpio;

    // GPIO 到 ADC 通道映射
    switch (gpio) {
        case GPIO_NUM_1:  s_adc_channel = ADC_CHANNEL_0; break;
        case GPIO_NUM_2:  s_adc_channel = ADC_CHANNEL_1; break;
        case GPIO_NUM_3:  s_adc_channel = ADC_CHANNEL_2; break;
        case GPIO_NUM_4:  s_adc_channel = ADC_CHANNEL_3; break;
        case GPIO_NUM_5:  s_adc_channel = ADC_CHANNEL_4; break;
        case GPIO_NUM_6:  s_adc_channel = ADC_CHANNEL_5; break;
        case GPIO_NUM_7:  s_adc_channel = ADC_CHANNEL_6; break;
        case GPIO_NUM_8:  s_adc_channel = ADC_CHANNEL_7; break;
        case GPIO_NUM_9:  s_adc_channel = ADC_CHANNEL_8; break;
        case GPIO_NUM_10: s_adc_channel = ADC_CHANNEL_9; break;
        default:
            SENSORS_LOGW(TAG, "GPIO%d 不是有效的 ADC 引脚, 使用默认 GPIO7", gpio);
            s_gpio = GPIO_NUM_7;
            s_adc_channel = ADC_CHANNEL_6;
            break;
    }

    esp_err_t ret = adc_init();
    if (ret == ESP_OK) {
        SENSORS_LOGI(TAG, "光敏电阻初始化完成, GPIO%d", s_gpio);
    }

    return ret;
}

void photosensor_set_gpio(gpio_num_t gpio)
{
    if (gpio == s_gpio) {
        return;
    }

    s_gpio = gpio;

    // 更新通道映射
    switch (gpio) {
        case GPIO_NUM_1:  s_adc_channel = ADC_CHANNEL_0; break;
        case GPIO_NUM_2:  s_adc_channel = ADC_CHANNEL_1; break;
        case GPIO_NUM_3:  s_adc_channel = ADC_CHANNEL_2; break;
        case GPIO_NUM_4:  s_adc_channel = ADC_CHANNEL_3; break;
        case GPIO_NUM_5:  s_adc_channel = ADC_CHANNEL_4; break;
        case GPIO_NUM_6:  s_adc_channel = ADC_CHANNEL_5; break;
        case GPIO_NUM_7:  s_adc_channel = ADC_CHANNEL_6; break;
        case GPIO_NUM_8:  s_adc_channel = ADC_CHANNEL_7; break;
        case GPIO_NUM_9:  s_adc_channel = ADC_CHANNEL_8; break;
        case GPIO_NUM_10: s_adc_channel = ADC_CHANNEL_9; break;
        default:
            SENSORS_LOGW(TAG, "GPIO%d 不是有效的 ADC 引脚", gpio);
            return;
    }

    // 重新配置通道（ADC 单元已初始化，只需配置通道）
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };

    adc_oneshot_unit_handle_t handle = adc_manager_get_handle();
    if (handle != NULL) {
        adc_oneshot_config_channel(handle, s_adc_channel, &config);
    }
}

gpio_num_t photosensor_get_gpio(void)
{
    return s_gpio;
}

uint32_t photosensor_read_raw(void)
{
    adc_oneshot_unit_handle_t handle = adc_manager_get_handle();
    if (handle == NULL) {
        SENSORS_LOGW(TAG, "ADC 句柄无效");
        return 0;
    }

    int raw_value = 0;
    esp_err_t ret = adc_oneshot_read(handle, s_adc_channel, &raw_value);
    if (ret != ESP_OK) {
        SENSORS_LOGW(TAG, "ADC 读取失败: %s", esp_err_to_name(ret));
        return 0;
    }

    return (uint32_t)raw_value;
}

esp_err_t photosensor_read(photosensor_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    data->raw = photosensor_read_raw();
    data->lux = photosensor_calculate(data->raw);

    return ESP_OK;
}

float photosensor_calculate(uint32_t raw_value)
{
    // 光敏电阻特性：光照越强，阻值越小，ADC 值越大
    // 当无光时，阻值很大，ADC 值很小（接近 0）
    // 当强光时，阻值很小，ADC 值很大（接近 4095）

    if (raw_value < 100) {
        SENSORS_LOGI(TAG, "ADC raw=%lu, 光照: <100(无光)", (unsigned long)raw_value);
        return 0;  // 低于阈值视为无光
    }

    // 使用经验公式将 ADC 值映射到 lux
    // raw=100 -> 约1 lux, raw=4095 -> 约1000 lux
    float lux = powf((float)raw_value / PHOTOSENSOR_ADC_MAX, 1.5f) * PHOTOSENSOR_MAX_LUX;

    // 限制范围
    if (lux < 1) {
        lux = 1;
    }
    if (lux > PHOTOSENSOR_MAX_LUX) {
        lux = PHOTOSENSOR_MAX_LUX;
    }

    SENSORS_LOGI(TAG, "ADC raw=%lu, 光照: %.0flux", (unsigned long)raw_value, lux);
    return lux;
}
