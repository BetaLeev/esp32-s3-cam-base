/**
 * @file sensors.c
 * @brief 传感器模块核心实现
 */

#include "sensors.h"
#include "config.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include <math.h>

static const char *TAG = "SENSORS";

/* ========== ADC传感器 ========== */

static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static SemaphoreHandle_t s_adc_mutex;

/* ========== DHT11传感器 ========== */

static gpio_num_t s_dht_gpio;
static SemaphoreHandle_t s_dht_mutex;
static float s_dht_temperature = 0.0f;
static float s_dht_humidity = 0.0f;
static bool s_dht_valid = false;
static uint32_t s_dht_fail_count = 0;

/* ========== 任务控制 ========== */

static bool s_task_running = false;

/* ========================================
 * ADC传感器实现
 * ======================================== */

static void adc_init(void)
{
    if (s_adc_handle != NULL) {
        return;
    }

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &s_adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };

    // 热敏电阻通道 (GPIO5 = ADC_CHANNEL_4)
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, ADC_CHANNEL_4, &config));

    // 光敏电阻通道 (GPIO3 = ADC_CHANNEL_2)
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, ADC_CHANNEL_2, &config));

    ESP_LOGI(TAG, "ADC初始化完成");
}

uint32_t sensors_read_thermistor_raw(void)
{
    if (s_adc_handle == NULL) {
        adc_init();
    }

    int raw_value = 0;
    esp_err_t ret = adc_oneshot_read(s_adc_handle, ADC_CHANNEL_4, &raw_value);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "热敏电阻读取失败");
        return 0;
    }
    return (uint32_t)raw_value;
}

uint32_t sensors_read_photosensor_raw(void)
{
    if (s_adc_handle == NULL) {
        adc_init();
    }

    int raw_value = 0;
    esp_err_t ret = adc_oneshot_read(s_adc_handle, ADC_CHANNEL_2, &raw_value);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "光敏电阻读取失败");
        return 0;
    }
    return (uint32_t)raw_value;
}

float sensors_calculate_temperature(uint32_t raw_value)
{
    if (raw_value == 0) return 25.0f;

    const float BETA = 3950.0f;
    const float R0 = 10000.0f;
    const float T0 = 298.15f;
    const float VCC = 3300.0f;
    const float R_PULLUP = 10000.0f;

    float voltage = (float)raw_value * VCC / 4095.0f;
    float resistance = R_PULLUP * voltage / (VCC - voltage);
    float temperature = 1.0f / (1.0f/T0 + logf(resistance/R0)/BETA);
    temperature -= 273.15f;

    if (temperature < -40) temperature = -40;
    if (temperature > 125) temperature = 125;

    return temperature;
}

float sensors_calculate_light(uint32_t raw_value)
{
    return ((float)raw_value / 4095.0f) * 1000.0f;
}

/* ========================================
 * DHT11传感器实现
 * ======================================== */

static void dht_delay_us(uint32_t us)
{
    ets_delay_us(us);
}

static esp_err_t dht11_read_raw(uint8_t *raw_data)
{
    if (raw_data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_dht_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "DHT11: 无法获取互斥锁");
        return ESP_ERR_TIMEOUT;
    }

    // 发送起始信号
    gpio_set_direction(s_dht_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(s_dht_gpio, 0);
    dht_delay_us(18000);
    gpio_set_level(s_dht_gpio, 1);
    dht_delay_us(40);

    gpio_set_direction(s_dht_gpio, GPIO_MODE_INPUT);

    // 等待响应 - 增加超时时间到200us
    int timeout = 200;
    while (gpio_get_level(s_dht_gpio) == 0) {
        if (timeout-- == 0) {
            ESP_LOGW(TAG, "DHT11: 等待响应超时1 (GPIO=%d, level=%d)", s_dht_gpio, gpio_get_level(s_dht_gpio));
            xSemaphoreGive(s_dht_mutex);
            return ESP_ERR_TIMEOUT;
        }
        dht_delay_us(1);
    }

    timeout = 200;
    while (gpio_get_level(s_dht_gpio) == 1) {
        if (timeout-- == 0) {
            ESP_LOGW(TAG, "DHT11: 等待响应超时2 (GPIO=%d, level=%d)", s_dht_gpio, gpio_get_level(s_dht_gpio));
            xSemaphoreGive(s_dht_mutex);
            return ESP_ERR_TIMEOUT;
        }
        dht_delay_us(1);
    }

    // 读取40位数据 - 增加超时时间到200us
    for (int i = 0; i < 5; i++) {
        raw_data[i] = 0;
        for (int j = 7; j >= 0; j--) {
            timeout = 200;
            while (gpio_get_level(s_dht_gpio) == 0) {
                if (timeout-- == 0) {
                    ESP_LOGW(TAG, "DHT11: 读取位超时1 (byte=%d, bit=%d)", i, j);
                    xSemaphoreGive(s_dht_mutex);
                    return ESP_ERR_TIMEOUT;
                }
                dht_delay_us(1);
            }

            dht_delay_us(50);

            if (gpio_get_level(s_dht_gpio) == 1) {
                raw_data[i] |= (1 << j);
            }

            timeout = 200;
            while (gpio_get_level(s_dht_gpio) == 1) {
                if (timeout-- == 0) {
                    ESP_LOGW(TAG, "DHT11: 读取位超时2 (byte=%d, bit=%d)", i, j);
                    xSemaphoreGive(s_dht_mutex);
                    return ESP_ERR_TIMEOUT;
                }
                dht_delay_us(1);
            }
        }
    }

    xSemaphoreGive(s_dht_mutex);

    // 校验
    uint8_t sum = raw_data[0] + raw_data[1] + raw_data[2] + raw_data[3];
    if (sum != raw_data[4]) {
        ESP_LOGW(TAG, "DHT11: CRC校验失败 (计算=%d, 收到=%d)", sum, raw_data[4]);
        return ESP_ERR_INVALID_CRC;
    }

    ESP_LOGI(TAG, "DHT11: 读取成功 温度=%d.%dC 湿度=%d.%d%%",
             raw_data[2], raw_data[3], raw_data[0], raw_data[1]);

    return ESP_OK;
}

static void dht11_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_dht_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);
}

esp_err_t sensors_dht11_read(sensors_dht11_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw_data[5] = {0};
    esp_err_t ret = dht11_read_raw(raw_data);

    if (ret == ESP_OK) {
        data->humidity = raw_data[0];
        data->temperature = raw_data[2];
        data->checksum = raw_data[4];
        data->valid = true;

        // 更新全局状态
        s_dht_temperature = (float)raw_data[2];
        s_dht_humidity = (float)raw_data[0];
        s_dht_valid = true;
        s_dht_fail_count = 0;
    } else {
        data->valid = false;
        s_dht_valid = false;
        s_dht_fail_count++;
    }

    return ret;
}

float sensors_dht11_get_temperature(void)
{
    return s_dht_temperature;
}

float sensors_dht11_get_humidity(void)
{
    return s_dht_humidity;
}

bool sensors_dht11_is_valid(void)
{
    return s_dht_valid;
}

/* ========================================
 * 公共接口实现
 * ======================================== */

esp_err_t sensors_init(void)
{
    ESP_LOGI(TAG, "初始化传感器模块...");

    // ADC互斥锁
    s_adc_mutex = xSemaphoreCreateMutex();
    if (s_adc_mutex == NULL) {
        ESP_LOGE(TAG, "创建ADC互斥锁失败");
        return ESP_FAIL;
    }

    // DHT11配置
    s_dht_gpio = DHT11_PIN;
    dht11_init();

    // DHT11互斥锁
    s_dht_mutex = xSemaphoreCreateMutex();
    if (s_dht_mutex == NULL) {
        ESP_LOGE(TAG, "创建DHT11互斥锁失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "传感器模块初始化完成");
    ESP_LOGI(TAG, "  热敏电阻 GPIO:%d", THERMISTOR_GPIO);
    ESP_LOGI(TAG, "  光敏电阻 GPIO:%d", PHOTOSENSOR_GPIO);
    ESP_LOGI(TAG, "  DHT11 GPIO:%d", DHT11_PIN);

    return ESP_OK;
}

static void sensors_task(void *pvParameters)
{
    sensors_dht11_data_t dht_data;
    uint32_t last_dht_log_time = 0;
    TickType_t last_wake_time = xTaskGetTickCount();

    ESP_LOGI(TAG, "传感器任务启动");

    s_task_running = true;

    while (s_task_running) {
        // 读取热敏电阻
        uint32_t thermistor_raw = sensors_read_thermistor_raw();
        g_system_status.thermistor_raw = thermistor_raw;
        g_system_status.thermistor_temp = sensors_calculate_temperature(thermistor_raw);

        // 读取光敏电阻
        uint32_t photosensor_raw = sensors_read_photosensor_raw();
        g_system_status.photosensor_raw = photosensor_raw;
        g_system_status.light = sensors_calculate_light(photosensor_raw);

        // 读取DHT11
        esp_err_t ret = sensors_dht11_read(&dht_data);
        if (ret == ESP_OK) {
            g_system_status.dht11_temp = (float)dht_data.temperature;
            g_system_status.dht11_humidity = (float)dht_data.humidity;
            g_system_status.dht11_valid = 1;

            ESP_LOGD(TAG, "DHT11: %.1fC %.1f%%", dht_data.temperature, dht_data.humidity);
        } else {
            g_system_status.dht11_valid = 0;

            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (s_dht_fail_count <= 3 || (now - last_dht_log_time) > 30000) {
                ESP_LOGW(TAG, "DHT11离线 (连续失败: %lu)", s_dht_fail_count);
                last_dht_log_time = now;
            }
        }

        ESP_LOGI(TAG, "ADC: 温度%.1fC 光照%.0flux | DHT11: %.1fC %.1f%%",
                 g_system_status.thermistor_temp, g_system_status.light,
                 g_system_status.dht11_temp, g_system_status.dht11_humidity);

        // 递增版本号，标记数据已更新
        g_system_status.version++;

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(3000));
    }

    s_task_running = false;
    vTaskDelete(NULL);
}

esp_err_t sensors_create_task(void)
{
    BaseType_t ret = xTaskCreate(
        sensors_task,
        "sensors_task",
        4096,
        NULL,
        5,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建传感器任务失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "传感器任务创建成功");
    return ESP_OK;
}
