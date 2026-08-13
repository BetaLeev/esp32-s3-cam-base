/**
 * @file soilhumidity.c
 * @brief 土壤湿度传感器实现
 *
 * 使用 LM393 土壤湿度传感器，通过分压电路连接 ADC
 * 湿度越高，ADC 值越低（传感器输出阻抗变化）
 */

#include "soilhumidity.h"
#include "../../config.h"
#include "esp_adc/adc_oneshot.h"
#include "../adc_manager.h"

static const char *TAG = "SOILHUMIDITY";
#define LOG_TAG TAG

/* ========================================
 * 内部变量
 * ======================================== */
static adc_channel_t s_adc_channel = ADC_CHANNEL_2;  /**< 默认 GPIO3 */
static gpio_num_t s_gpio = SOILHUMIDITY_DEFAULT_GPIO;

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

esp_err_t soilhumidity_init(gpio_num_t gpio)
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
            SENSORS_LOGW(TAG, "GPIO%d 不是有效的 ADC 引脚, 使用默认 GPIO3", gpio);
            s_gpio = GPIO_NUM_3;
            s_adc_channel = ADC_CHANNEL_2;
            break;
    }

    esp_err_t ret = adc_init();
    if (ret == ESP_OK) {
        SENSORS_LOGI(TAG, "土壤湿度传感器初始化完成, GPIO%d", s_gpio);
    }

    return ret;
}

void soilhumidity_set_gpio(gpio_num_t gpio)
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

gpio_num_t soilhumidity_get_gpio(void)
{
    return s_gpio;
}

uint32_t soilhumidity_read_raw(void)
{
    SENSORS_LOGI(TAG, "开始读取土壤湿度传感器...");
    
    adc_oneshot_unit_handle_t handle = adc_manager_get_handle();
    if (handle == NULL) {
        SENSORS_LOGE(TAG, "ADC 句柄无效");
        return 0;
    }

    SENSORS_LOGI(TAG, "ADC 通道: %d, GPIO: %d", s_adc_channel, s_gpio);

    int raw_value = 0;
    esp_err_t ret = adc_oneshot_read(handle, s_adc_channel, &raw_value);
    if (ret != ESP_OK) {
        SENSORS_LOGE(TAG, "ADC 读取失败: %s", esp_err_to_name(ret));
        return 0;
    }

    SENSORS_LOGI(TAG, "ADC 读取成功, raw=%d", raw_value);
    return (uint32_t)raw_value;
}

esp_err_t soilhumidity_read(soilhumidity_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    data->raw = soilhumidity_read_raw();
    data->humidity = soilhumidity_calculate(data->raw);

    return ESP_OK;
}

float soilhumidity_calculate(uint32_t raw_value)
{
    // LM393 土壤湿度传感器特性：
    // - 干土：传感器电阻大，输出电压高，ADC 值接近 4095
    // - 湿土：传感器电阻小，输出电压低，ADC 值低

    if (raw_value >= SOILHUMIDITY_DRY_VALUE) {
        SENSORS_LOGI(TAG, "ADC raw=%lu, 湿度=0%% (干土)", (unsigned long)raw_value);
        return SOILHUMIDITY_MIN;
    }

    if (raw_value <= SOILHUMIDITY_WET_VALUE) {
        SENSORS_LOGI(TAG, "ADC raw=%lu, 湿度=100%% (水中)", (unsigned long)raw_value);
        return SOILHUMIDITY_MAX;
    }

    // 计算湿度百分比
    // 湿度 = (ADC_max - raw) / (ADC_max - ADC_min) * 100
    float humidity = (float)(SOILHUMIDITY_DRY_VALUE - raw_value) /
                     (SOILHUMIDITY_DRY_VALUE - SOILHUMIDITY_WET_VALUE) * 100.0f;

    SENSORS_LOGI(TAG, "ADC raw=%lu, 湿度=%.1f%%", (unsigned long)raw_value, humidity);

    // 限制湿度范围
    if (humidity < SOILHUMIDITY_MIN) {
        humidity = SOILHUMIDITY_MIN;
    } else if (humidity > SOILHUMIDITY_MAX) {
        humidity = SOILHUMIDITY_MAX;
    }

    return humidity;
}

const char* soilhumidity_get_status(float humidity)
{
    if (humidity < 20.0f) {
        return "干燥";
    } else if (humidity < 40.0f) {
        return "偏干";
    } else if (humidity < 60.0f) {
        return "适中";
    } else if (humidity < 80.0f) {
        return "偏湿";
    } else {
        return "湿润";
    }
}
