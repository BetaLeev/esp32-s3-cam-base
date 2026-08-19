/**
 * @file motor_web.c
 * @brief 电机/水泵 Web API实现
 */

#include "motor_web.h"
#include "motor.h"
#include "../../web/web_module.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG __attribute__((unused)) = "MOTOR_WEB";

/**
 * @brief 解析URL查询参数
 */
static esp_err_t parse_query_param(httpd_req_t *req, const char *param_name, int *out_value)
{
    char query_buf[128];
    char *query = query_buf;
    size_t query_len = httpd_req_get_url_query_len(req) + 1;

    if (query_len > sizeof(query_buf)) {
        query = malloc(query_len);
        if (query == NULL) return ESP_ERR_NO_MEM;
    }

    if (httpd_req_get_url_query_str(req, query, query_len) == ESP_OK) {
        char param_buf[64];
        if (httpd_query_key_value(query, param_name, param_buf, sizeof(param_buf)) == ESP_OK) {
            *out_value = atoi(param_buf);
            if (query != query_buf) free(query);
            return ESP_OK;
        }
    }

    if (query != query_buf) free(query);
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief API: 水泵控制 (GET /api/pump?gear=0-3)
 */
esp_err_t motor_web_pump_handler(httpd_req_t *req)
{
    int gear = -1;
    parse_query_param(req, "gear", &gear);

    if (gear >= 0 && gear <= 3) {
        esp_err_t ret = motor_pump_set_gear((pump_gear_t)gear);
        pump_gear_t current = motor_pump_get_gear();

        cJSON *data = cJSON_CreateObject();
        cJSON_AddNumberToObject(data, "gear", current);
        cJSON_AddStringToObject(data, "state", motor_pump_get_gear_name(current));

        return send_success(req, data, ret == ESP_OK ? "水泵控制成功" : "水泵控制失败");
    }

    // 无参数时返回当前状态
    pump_gear_t current = motor_pump_get_gear();
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "gear", current);
    cJSON_AddStringToObject(data, "state", motor_pump_get_gear_name(current));

    return send_success(req, data, "获取水泵状态成功");
}

/**
 * @brief API: 电机控制 (GET /api/motor?cmd=start|stop|speed)
 */
esp_err_t motor_web_handler(httpd_req_t *req)
{
    char cmd_buf[16] = {0};
    char query_buf[256];
    size_t query_len = httpd_req_get_url_query_len(req) + 1;

    esp_err_t ret = ESP_OK;

    if (query_len <= sizeof(query_buf)) {
        if (httpd_req_get_url_query_str(req, query_buf, query_len) == ESP_OK) {
            httpd_query_key_value(query_buf, "cmd", cmd_buf, sizeof(cmd_buf));
        }
    }

    if (strcmp(cmd_buf, "start") == 0) {
        int speed = -1;
        parse_query_param(req, "speed", &speed);
        if (speed >= 0 && speed <= 100) {
            motor_set_speed((uint8_t)speed);
        }
        ret = motor_start();
    } else if (strcmp(cmd_buf, "stop") == 0) {
        ret = motor_stop();
    } else if (strcmp(cmd_buf, "speed") == 0) {
        int speed = -1;
        if (parse_query_param(req, "speed", &speed) == ESP_OK && speed >= 0 && speed <= 100) {
            ret = motor_set_speed((uint8_t)speed);
        }
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "state", motor_get_state());
    cJSON_AddNumberToObject(data, "speed", motor_get_speed());

    return send_success(req, data, ret == ESP_OK ? "电机控制成功" : "电机控制失败");
}