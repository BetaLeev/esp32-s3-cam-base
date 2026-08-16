/**
 * @file web_module.c
 * @brief 统一 HTTP 响应工具实现
 */
#include "web_module.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "WEB_MODULE";

/**
 * @brief 发送 JSON 成功响应
 */
esp_err_t send_success(httpd_req_t *req, cJSON *data, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "success");
    cJSON_AddNumberToObject(root, "code", HTTP_OK);
    cJSON_AddStringToObject(root, "message", message ? message : "OK");
    if (data != NULL) {
        cJSON_AddItemToObject(root, "data", data);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        return ESP_ERR_NO_MEM;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ret;
}

/**
 * @brief 发送 JSON 错误响应
 */
esp_err_t send_error(httpd_req_t *req, const char *message, int http_code)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "error");
    cJSON_AddNumberToObject(root, "code", http_code);
    cJSON_AddStringToObject(root, "message", message ? message : "Error");

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str == NULL) {
        return ESP_ERR_NO_MEM;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send_err(req, http_code, json_str);
    free(json_str);
    return ret;
}

/**
 * @brief 发送 Bad Request 响应
 */
esp_err_t send_bad_request(httpd_req_t *req, const char *message)
{
    return send_error(req, message, HTTP_BAD_REQUEST);
}

/**
 * @brief 发送 404 Not Found 响应
 */
esp_err_t send_not_found(httpd_req_t *req, const char *message)
{
    return send_error(req, message, HTTP_NOT_FOUND);
}

/**
 * @brief 发送内部错误响应
 */
esp_err_t send_internal_error(httpd_req_t *req, const char *message)
{
    return send_error(req, message, HTTP_INTERNAL_ERROR);
}

/**
 * @brief 发送成功响应（仅消息，无数据）
 */
esp_err_t send_success_msg(httpd_req_t *req, const char *message)
{
    return send_success(req, NULL, message);
}

/**
 * @brief 解析请求体中的 JSON
 */
cJSON *parse_request_json(httpd_req_t *req)
{
    if (req->content_len == 0) {
        return NULL;
    }

    char *buf = malloc(req->content_len + 1);
    if (buf == NULL) {
        return NULL;
    }

    int ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0) {
        free(buf);
        return NULL;
    }
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    return json;
}
