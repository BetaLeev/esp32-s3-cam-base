/**
 * @file web_module.c
 * @brief Web 模块通用工具实现
 */

#include "web_module.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "WEB_MODULE";

/* ========================================
 * JSON 响应封装函数
 * ======================================== */

esp_err_t send_json_response(httpd_req_t *req,
                              response_status_t status,
                              cJSON *data,
                              const char *message,
                              http_status_code_t http_code)
{
    cJSON *response = cJSON_CreateObject();
    if (!response) {
        ESP_LOGE(TAG, "创建响应 JSON 失败");
        return ESP_FAIL;
    }

    // 添加状态字段
    const char *status_str;
    switch (status) {
        case RESP_SUCCESS:
            status_str = "success";
            break;
        case RESP_WARNING:
            status_str = "warning";
            break;
        case RESP_ERROR:
        default:
            status_str = "error";
            break;
    }
    cJSON_AddStringToObject(response, "status", status_str);

    // 添加 HTTP 状态码
    cJSON_AddNumberToObject(response, "code", http_code);

    // 添加消息
    if (message) {
        cJSON_AddStringToObject(response, "message", message);
    } else {
        cJSON_AddStringToObject(response, "message", "");
    }

    // 添加数据
    if (data) {
        cJSON_AddItemToObject(response, "data", data);
    } else {
        cJSON_AddItemToObject(response, "data", cJSON_CreateNull());
    }

    // 序列化为字符串
    char *json_str = cJSON_Print(response);
    if (!json_str) {
        ESP_LOGE(TAG, "序列化 JSON 失败");
        cJSON_Delete(response);
        return ESP_FAIL;
    }

    // 设置 HTTP 状态码
    httpd_resp_set_status(req, http_code == HTTP_OK ? "200 OK" :
                           http_code == HTTP_CREATED ? "201 Created" :
                           http_code == HTTP_BAD_REQUEST ? "400 Bad Request" :
                           http_code == HTTP_NOT_FOUND ? "404 Not Found" :
                           http_code == HTTP_SERVICE_UNAVAILABLE ? "503 Service Unavailable" :
                           "500 Internal Server Error");

    // 发送响应
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json_str, strlen(json_str));

    // 释放内存
    cJSON_free(json_str);
    cJSON_Delete(response);

    return ret;
}

/* ========================================
 * JSON 辅助函数
 * ======================================== */

cJSON *json_create_sensor_data(const char *name, cJSON *values)
{
    cJSON *obj = cJSON_CreateObject();
    if (obj) {
        cJSON_AddStringToObject(obj, "name", name);
        if (values) {
            cJSON_AddItemToObject(obj, "values", values);
        }
    }
    return obj;
}

void json_add_int(cJSON *obj, const char *key, int value)
{
    if (obj && key) {
        cJSON_AddNumberToObject(obj, key, value);
    }
}

void json_add_float(cJSON *obj, const char *key, double value, int decimals)
{
    if (obj && key) {
        char format[16];
        snprintf(format, sizeof(format), "%%.%df", decimals);
        char buf[32];
        snprintf(buf, sizeof(buf), format, value);
        cJSON_AddStringToObject(obj, key, buf);
    }
}

void json_add_bool(cJSON *obj, const char *key, bool value)
{
    if (obj && key) {
        cJSON_AddBoolToObject(obj, key, value);
    }
}

void json_add_string(cJSON *obj, const char *key, const char *value)
{
    if (obj && key && value) {
        cJSON_AddStringToObject(obj, key, value);
    }
}

/* ========================================
 * 请求解析函数
 * ======================================== */

bool get_query_string(httpd_req_t *req, const char *key, char *buf, size_t buf_len)
{
    if (!req || !key || !buf || buf_len == 0) {
        return false;
    }

    char query_buf[512];
    esp_err_t ret = httpd_req_get_url_query_str(req, query_buf, sizeof(query_buf));
    if (ret != ESP_OK) {
        return false;
    }

    ret = httpd_query_key_value(query_buf, key, buf, buf_len);
    return ret == ESP_OK;
}

cJSON *parse_request_json(httpd_req_t *req)
{
    if (!req) {
        return NULL;
    }

    // 读取请求体
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "接收请求体超时");
        } else {
            ESP_LOGE(TAG, "接收请求体失败: %d", ret);
        }
        return NULL;
    }
    buf[ret] = '\0';

    // 解析 JSON
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        ESP_LOGW(TAG, "解析 JSON 失败: %s", cJSON_GetErrorPtr());
        return NULL;
    }

    return json;
}
