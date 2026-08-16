/**
 * @file sensors_web.c
 * @brief 传感器 Web API 实现
 */

#include "sensors_web.h"
#include "../config.h"
#include "../sensors/sensors.h"
#include "soilhumidity/soilhumidity.h"
#include "../web/web_module.h"
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "SENSORS_WEB";

// CORS 跨域预检 Handler
static esp_err_t sensors_web_options_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

void sensors_web_register_routes(httpd_handle_t server) {
    httpd_uri_t routes[] = {
        {.uri = "/api/sensors/data",   .method = HTTP_GET,     .handler = sensors_web_get_data_handler},
        {.uri = "/api/sensors/config", .method = HTTP_GET,     .handler = sensors_web_get_config_handler},
        {.uri = "/api/sensors/config", .method = HTTP_POST,    .handler = sensors_web_set_config_handler},
        {.uri = "/api/sensors/config", .method = HTTP_OPTIONS, .handler = sensors_web_options_handler},
    };

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }
}

/* ========================================
 * API: 获取传感器数据
 * GET /api/sensors/data
 * ======================================== */
esp_err_t sensors_web_get_data_handler(httpd_req_t *req) {
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return send_internal_error(req, "创建响应数据失败");
    }

    cJSON *thermistor = cJSON_CreateObject();
    cJSON_AddNumberToObject(thermistor, "gpio", thermistor_get_gpio());
    cJSON_AddNumberToObject(thermistor, "raw", g_system_status.thermistor_raw);
    cJSON_AddNumberToObject(thermistor, "temperature", g_system_status.thermistor_temp);
    cJSON_AddItemToObject(data, "thermistor", thermistor);

    cJSON *photosensor = cJSON_CreateObject();
    cJSON_AddNumberToObject(photosensor, "gpio", photosensor_get_gpio());
    cJSON_AddNumberToObject(photosensor, "raw", g_system_status.photosensor_raw);
    cJSON_AddNumberToObject(photosensor, "light", g_system_status.light);
    cJSON_AddItemToObject(data, "photosensor", photosensor);

    cJSON *dht11 = cJSON_CreateObject();
    cJSON_AddNumberToObject(dht11, "gpio", dht11_get_gpio());
    cJSON_AddNumberToObject(dht11, "temperature", g_system_status.dht11_temp);
    cJSON_AddNumberToObject(dht11, "humidity", g_system_status.dht11_humidity);
    cJSON_AddBoolToObject(dht11, "valid", g_system_status.dht11_valid);
    cJSON_AddItemToObject(data, "dht11", dht11);

    cJSON *soilhumidity = cJSON_CreateObject();
    cJSON_AddNumberToObject(soilhumidity, "gpio", soilhumidity_get_gpio());
    cJSON_AddNumberToObject(soilhumidity, "raw", g_system_status.soil_raw);
    cJSON_AddNumberToObject(soilhumidity, "humidity", g_system_status.soil_humidity);
    cJSON_AddStringToObject(soilhumidity, "status", soilhumidity_get_status(g_system_status.soil_humidity));
    cJSON_AddItemToObject(data, "soilhumidity", soilhumidity);

    return send_success(req, data, "获取传感器数据成功");
}

/* ========================================
 * API: 获取传感器配置
 * GET /api/sensors/config
 * ======================================== */
esp_err_t sensors_web_get_config_handler(httpd_req_t *req) {
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return send_internal_error(req, "创建响应数据失败");
    }

    cJSON *thermistor = cJSON_CreateObject();
    cJSON_AddNumberToObject(thermistor, "gpio", thermistor_get_gpio());
    cJSON_AddStringToObject(thermistor, "type", "thermistor");
    cJSON_AddStringToObject(thermistor, "unit", "celsius");
    cJSON_AddItemToObject(data, "thermistor", thermistor);

    cJSON *photosensor = cJSON_CreateObject();
    cJSON_AddNumberToObject(photosensor, "gpio", photosensor_get_gpio());
    cJSON_AddStringToObject(photosensor, "type", "photosensor");
    cJSON_AddStringToObject(photosensor, "unit", "lux");
    cJSON_AddItemToObject(data, "photosensor", photosensor);

    cJSON *dht11 = cJSON_CreateObject();
    cJSON_AddNumberToObject(dht11, "gpio", dht11_get_gpio());
    cJSON_AddStringToObject(dht11, "type", "dht11");
    cJSON_AddItemToObject(data, "dht11", dht11);

    cJSON *soilhumidity = cJSON_CreateObject();
    cJSON_AddNumberToObject(soilhumidity, "gpio", soilhumidity_get_gpio());
    cJSON_AddStringToObject(soilhumidity, "type", "soilhumidity");
    cJSON_AddStringToObject(soilhumidity, "unit", "percent");
    cJSON_AddItemToObject(data, "soilhumidity", soilhumidity);

    return send_success(req, data, "获取传感器配置成功");
}

/* ========================================
 * API: 配置传感器引脚
 * POST /api/sensors/config
 * ======================================== */
esp_err_t sensors_web_set_config_handler(httpd_req_t *req) {
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    cJSON *json = parse_request_json(req);
    if (!json) {
        return send_bad_request(req, "无效的 JSON 格式");
    }

    bool updated = false;
    char message[256] = {0};

    cJSON *thermistor_gpio = cJSON_GetObjectItem(json, "thermistor_gpio");
    if (cJSON_IsNumber(thermistor_gpio)) {
        int gpio = thermistor_gpio->valueint;
        if (gpio >= 0 && gpio <= 48) {
            thermistor_set_gpio((gpio_num_t)gpio);
            ESP_LOGI(TAG, "设置热敏电阻 GPIO: %d", gpio);
            updated = true;
        } else {
            snprintf(message, sizeof(message), "无效的 GPIO 编号: %d", gpio);
            cJSON_Delete(json);
            return send_bad_request(req, message);
        }
    }

    cJSON *photosensor_gpio = cJSON_GetObjectItem(json, "photosensor_gpio");
    if (cJSON_IsNumber(photosensor_gpio)) {
        int gpio = photosensor_gpio->valueint;
        if (gpio >= 0 && gpio <= 48) {
            photosensor_set_gpio((gpio_num_t)gpio);
            ESP_LOGI(TAG, "设置光敏电阻 GPIO: %d", gpio);
            updated = true;
        } else {
            snprintf(message, sizeof(message), "无效的 GPIO 编号: %d", gpio);
            cJSON_Delete(json);
            return send_bad_request(req, message);
        }
    }

    cJSON *dht11_gpio = cJSON_GetObjectItem(json, "dht11_gpio");
    if (cJSON_IsNumber(dht11_gpio)) {
        int gpio = dht11_gpio->valueint;
        if (gpio >= 0 && gpio <= 48) {
            dht11_set_gpio((gpio_num_t)gpio);
            ESP_LOGI(TAG, "设置 DHT11 GPIO: %d", gpio);
            updated = true;
        } else {
            snprintf(message, sizeof(message), "无效的 GPIO 编号: %d", gpio);
            cJSON_Delete(json);
            return send_bad_request(req, message);
        }
    }

    cJSON *soilhumidity_gpio = cJSON_GetObjectItem(json, "soilhumidity_gpio");
    if (cJSON_IsNumber(soilhumidity_gpio)) {
        int gpio = soilhumidity_gpio->valueint;
        if (gpio >= 0 && gpio <= 48) {
            soilhumidity_set_gpio((gpio_num_t)gpio);
            ESP_LOGI(TAG, "设置土壤湿度传感器 GPIO: %d", gpio);
            updated = true;
        } else {
            snprintf(message, sizeof(message), "无效的 GPIO 编号: %d", gpio);
            cJSON_Delete(json);
            return send_bad_request(req, message);
        }
    }

    cJSON_Delete(json);

    if (updated) {
        return send_success_msg(req, "传感器配置已更新");
    } else {
        return send_success_msg(req, "未提供任何配置更新");
    }
}