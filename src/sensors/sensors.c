/**
 * @file sensors.c
 * @brief 传感器模块统一入口实现
 */

#include "sensors.h"
#include "../config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdbool.h>

#include "thermistor/thermistor.h"
#include "photosensor/photosensor.h"
#include "dht11/dht11.h"
#include "soilhumidity/soilhumidity.h"

static const char *TAG = "SENSORS";
static bool s_task_running = false;

typedef struct {
    float humidity;
    float temperature;
    uint8_t checksum;
    bool valid;
} sensors_dht11_data_t;

esp_err_t sensors_init(void)
{
    ESP_LOGI(TAG, "初始化传感器模块...");

    // 热敏电阻暂时禁用（与土壤湿度传感器共用 GPIO3）
    ESP_LOGI(TAG, "  热敏电阻已禁用（GPIO3 与土壤湿度传感器共用）");

    // 光敏电阻暂时禁用
    ESP_LOGI(TAG, "  光敏电阻已禁用（GPIO3 与土壤湿度传感器共用）");

    esp_err_t ret = dht11_init(DHT11_PIN);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "DHT11 初始化失败: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "  DHT11 GPIO:%d", dht11_get_gpio());
    }

    ret = soilhumidity_init(SOILHUMIDITY_DEFAULT_GPIO);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "土壤湿度传感器初始化失败: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "  土壤湿度传感器 GPIO:%d", soilhumidity_get_gpio());
    }

    ESP_LOGI(TAG, "传感器模块初始化完成");
    return ESP_OK;
}

static void sensors_task(void *pvParameters)
{
    dht11_data_t dht11_data;
    soilhumidity_data_t soilhumidity_data;

    SENSORS_LOGI(TAG, "传感器任务启动");
    s_task_running = true;

    while (s_task_running) {
        SENSORS_LOGI(TAG, "===== 传感器轮询开始 =====");

        // 热敏电阻已禁用
        g_system_status.thermistor_raw = 0;
        g_system_status.thermistor_temp = 0;

        // 光敏电阻已禁用
        g_system_status.photosensor_raw = 0;
        g_system_status.light = 0;

        esp_err_t ret = dht11_read(&dht11_data);
        if (ret == ESP_OK) {
            g_system_status.dht11_temp = dht11_data.temperature;
            g_system_status.dht11_humidity = dht11_data.humidity;
            g_system_status.dht11_valid = 1;
        } else {
            g_system_status.dht11_valid = 0;
        }

        SENSORS_LOGI(TAG, "准备读取土壤湿度...");
        soilhumidity_read(&soilhumidity_data);
        SENSORS_LOGI(TAG, "土壤湿度读取完成: raw=%lu, humidity=%.1f%%", 
                     (unsigned long)soilhumidity_data.raw, soilhumidity_data.humidity);
        g_system_status.soil_raw = soilhumidity_data.raw;
        g_system_status.soil_humidity = soilhumidity_data.humidity;

        SENSORS_LOGI(TAG, "===== 传感器轮询结束 =====");
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
        ESP_LOGE(TAG, "创建传感器任务失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "传感器任务创建成功");
    return ESP_OK;
}

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

uint32_t sensors_read_soilhumidity_raw(void)
{
    return soilhumidity_read_raw();
}

float sensors_calculate_soilhumidity(uint32_t raw_value)
{
    return soilhumidity_calculate(raw_value);
}