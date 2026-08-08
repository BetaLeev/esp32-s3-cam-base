/**
 * @file thermistor.c
 * @brief 热敏电阻传感器实现
 *
 * 使用 NTC 10K 热敏电阻，通过分压电路连接 ADC
 * 温度计算使用 Steinhart-Hart 方程
 */

#include "thermistor.h"
#include "../../config.h"
#include "esp_adc/adc_oneshot.h"
#include "../adc_manager.h"
#include <math.h>

static const char *TAG = "THERMISTOR";
#define LOG_TAG TAG

/* ========================================
 * 内部变量
 * ======================================== */
static adc_channel_t s_adc_channel = ADC_CHANNEL_4;  /**< 默认 GPIO5 */
static gpio_num_t s_gpio = THERMISTOR_DEFAULT_GPIO;

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

esp_err_t thermistor_init(gpio_num_t gpio)
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
            SENSORS_LOGW(TAG, "GPIO%d 不是有效的 ADC 引脚, 使用默认 GPIO5", gpio);
            s_gpio = GPIO_NUM_5;
            s_adc_channel = ADC_CHANNEL_4;
            break;
    }

    esp_err_t ret = adc_init();
    if (ret == ESP_OK) {
        SENSORS_LOGI(TAG, "热敏电阻初始化完成, GPIO%d", s_gpio);
    }

    return ret;
}

void thermistor_set_gpio(gpio_num_t gpio)
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

gpio_num_t thermistor_get_gpio(void)
{
    return s_gpio;
}

uint32_t thermistor_read_raw(void)
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

    SENSORS_LOGD(TAG, "ADC raw=%d", raw_value);
    return (uint32_t)raw_value;
}

esp_err_t thermistor_read(thermistor_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    data->raw = thermistor_read_raw();
    data->temperature = thermistor_calculate(data->raw);

    return ESP_OK;
}

float thermistor_calculate(uint32_t raw_value)
{
    if (raw_value == 0) {
        SENSORS_LOGW(TAG, "ADC raw=0, 返回默认温度25C");
        return 25.0f;  // 默认温度
    }

    // 计算分压后的电压 (mV)
    float voltage = (float)raw_value * THERMISTOR_VCC / THERMISTOR_ADC_MAX;

    // 计算热敏电阻阻值
    // Vout = Vcc * Rthermistor / (Rthermistor + Rpullup)
    // Rthermistor = Rpullup * Vout / (Vcc - Vout)
    float resistance = THERMISTOR_PULLUP_RESISTOR * voltage / (THERMISTOR_VCC - voltage);

    SENSORS_LOGI(TAG, "ADC raw=%lu, voltage=%.1fmV, resistance=%.1fΩ",
             (unsigned long)raw_value, voltage, resistance);

    if (resistance <= 0) {
        SENSORS_LOGW(TAG, "阻值异常(<=0), 返回最小温度");
        return THERMISTOR_MIN_TEMP;
    }

    // Steinhart-Hart 方程计算温度
    // 1/T = 1/T0 + (1/B) * ln(R/R0)
    float temp_kelvin = 1.0f / (1.0f / THERMISTOR_T0 + logf(resistance / THERMISTOR_R0) / THERMISTOR_B_VALUE);
    float temperature = temp_kelvin - 273.15f;

    SENSORS_LOGI(TAG, "计算温度: %.1fC", temperature);

    // 限制温度范围
    if (temperature < THERMISTOR_MIN_TEMP) {
        temperature = THERMISTOR_MIN_TEMP;
    } else if (temperature > THERMISTOR_MAX_TEMP) {
        temperature = THERMISTOR_MAX_TEMP;
    }

    return temperature;
}
