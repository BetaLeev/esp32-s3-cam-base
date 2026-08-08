/**
 * @file sensors.c
 * @brief 传感器模块统一入口实现
 *
 * 统一初始化所有传感器子模块，创建传感器读取任务
 */

#include "sensors.h"
#include "../config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* 子模块 */
#include "thermistor/thermistor.h"
#include "photosensor/photosensor.h"
#include "dht11/dht11.h"

static const char *TAG = "SENSORS";
#define LOG_TAG TAG

/* ========================================
 * 任务控制
 * ======================================== */

static bool s_task_running = false;

/* ========================================
 * DHT11 数据兼容结构体
 * ======================================== */

/**
 * @brief DHT11 数据（兼容旧代码）
 */
typedef struct {
    float humidity;
    float temperature;
    uint8_t checksum;
    bool valid;
} sensors_dht11_data_t;

/* ========================================
 * 公共接口实现
 * ======================================== */

esp_err_t sensors_init(void)
{
    SENSORS_LOGI(TAG, "初始化传感器模块...");

    /* 初始化热敏电阻 */
    esp_err_t ret = thermistor_init(THERMISTOR_GPIO);
    if (ret != ESP_OK) {
        SENSORS_LOGW(TAG, "热敏电阻初始化失败: %s", esp_err_to_name(ret));
    } else {
        SENSORS_LOGI(TAG, "  热敏电阻 GPIO:%d", thermistor_get_gpio());
    }

    /* 初始化光敏电阻 */
    ret = photosensor_init(PHOTOSENSOR_GPIO);
    if (ret != ESP_OK) {
        SENSORS_LOGW(TAG, "光敏电阻初始化失败: %s", esp_err_to_name(ret));
    } else {
        SENSORS_LOGI(TAG, "  光敏电阻 GPIO:%d", photosensor_get_gpio());
    }

    /* 初始化 DHT11 */
    ret = dht11_init(DHT11_PIN);
    if (ret != ESP_OK) {
        SENSORS_LOGW(TAG, "DHT11 初始化失败: %s", esp_err_to_name(ret));
    } else {
        SENSORS_LOGI(TAG, "  DHT11 GPIO:%d", dht11_get_gpio());
    }

    SENSORS_LOGI(TAG, "传感器模块初始化完成");
    return ESP_OK;
}

static void sensors_task(void *pvParameters)
{
    thermistor_data_t thermistor_data;
    photosensor_data_t photosensor_data;
    dht11_data_t dht11_data;

    SENSORS_LOGI(TAG, "传感器任务启动");

    s_task_running = true;

    while (s_task_running) {
        /* 读取热敏电阻 */
        thermistor_read(&thermistor_data);
        g_system_status.thermistor_raw = thermistor_data.raw;
        g_system_status.thermistor_temp = thermistor_data.temperature;

        /* 读取光敏电阻 */
        photosensor_read(&photosensor_data);
        g_system_status.photosensor_raw = photosensor_data.raw;
        g_system_status.light = photosensor_data.lux;

        /* 读取 DHT11 */
        esp_err_t ret = dht11_read(&dht11_data);
        if (ret == ESP_OK) {
            g_system_status.dht11_temp = dht11_data.temperature;
            g_system_status.dht11_humidity = dht11_data.humidity;
            g_system_status.dht11_valid = 1;
        } else {
            g_system_status.dht11_valid = 0;
        }

        /* 递增版本号，标记数据已更新 */
        g_system_status.version++;

        vTaskDelay(pdMS_TO_TICKS(3000));
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
        SENSORS_LOGE(TAG, "创建传感器任务失败");
        return ESP_FAIL;
    }

    SENSORS_LOGI(TAG, "传感器任务创建成功");
    return ESP_OK;
}

/* ========================================
 * 兼容旧代码的接口
 * ======================================== */

uint32_t sensors_read_thermistor_raw(void)
{
    return thermistor_read_raw();
}

uint32_t sensors_read_photosensor_raw(void)
{
    return photosensor_read_raw();
}

float sensors_calculate_temperature(uint32_t raw_value)
{
    return thermistor_calculate(raw_value);
}

float sensors_calculate_light(uint32_t raw_value)
{
    return photosensor_calculate(raw_value);
}

esp_err_t sensors_dht11_read(sensors_dht11_data_t *data)
{
    dht11_data_t dht_data;
    esp_err_t ret = dht11_read(&dht_data);

    if (data != NULL) {
        data->temperature = dht_data.temperature;
        data->humidity = dht_data.humidity;
        data->checksum = dht_data.checksum;
        data->valid = dht_data.valid;
    }

    return ret;
}

float sensors_dht11_get_temperature(void)
{
    return dht11_get_temperature();
}

float sensors_dht11_get_humidity(void)
{
    return dht11_get_humidity();
}

bool sensors_dht11_is_valid(void)
{
    return dht11_is_valid();
}
