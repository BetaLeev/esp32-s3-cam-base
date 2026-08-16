/**
 * @file servo_web.c
 * @brief 舵机 Web API实现
 */

#include "servo_web.h"
#include "servo.h"
#include "../../web/web_module.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "SERVO_WEB";

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
 * @brief API: 舵机控制 (GET /api/servo?angle=0-180)
 */
esp_err_t servo_web_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "舵机请求到达, URI: %s", req->uri);

    int angle = -1;

    if (parse_query_param(req, "angle", &angle) == ESP_OK) {
        ESP_LOGI(TAG, "解析到角度: %d", angle);
        if (angle >= 0 && angle <= 150) {
            esp_err_t ret = servo_set_angle((uint8_t)angle);
            cJSON *data = cJSON_CreateObject();
            cJSON_AddNumberToObject(data, "angle", angle);
            return send_success(req, data, ret == ESP_OK ? "舵机控制成功" : "舵机控制失败");
        } else {
            cJSON *data = cJSON_CreateObject();
            cJSON_AddNumberToObject(data, "angle", servo_get_angle());
            return send_success(req, data, "角度超出范围(0-150)");
        }
    }

    ESP_LOGI(TAG, "无angle参数，返回当前角度");
    // 无参数时返回当前角度
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "angle", servo_get_angle());
    return send_success(req, data, "获取舵机角度成功");
}
