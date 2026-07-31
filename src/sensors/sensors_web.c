/**
 * @file sensors_web.c
 * @brief 传感器Web API实现
 */

#include "sensors_web.h"
#include "../config.h"
#include "../sensors/sensors.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "SENSORS_WEB";

esp_err_t sensors_web_get_data_handler(httpd_req_t *req)
{
    char response[512];
    int len = snprintf(response, sizeof(response),
        "{"
        "\"thermistor\":{"
            "\"raw\":%lu,"
            "\"temperature\":%.1f"
        "},"
        "\"photosensor\":{"
            "\"raw\":%lu,"
            "\"light\":%.1f"
        "},"
        "\"dht11\":{"
            "\"temperature\":%.1f,"
            "\"humidity\":%.1f,"
            "\"valid\":%d"
        "}"
        "}",
        (unsigned long)g_system_status.thermistor_raw,
        g_system_status.thermistor_temp,
        (unsigned long)g_system_status.photosensor_raw,
        g_system_status.light,
        g_system_status.dht11_temp,
        g_system_status.dht11_humidity,
        g_system_status.dht11_valid
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, len);

    return ESP_OK;
}
