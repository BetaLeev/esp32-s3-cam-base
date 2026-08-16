/**
 * @file web_module.h
 * @brief 统一 HTTP 响应工具
 */
#ifndef WEB_MODULE_H
#define WEB_MODULE_H

#include "esp_http_server.h"
#include "cJSON.h"

/* HTTP 状态码 */
#define HTTP_OK                  200
#define HTTP_BAD_REQUEST         400
#define HTTP_NOT_FOUND           404
#define HTTP_INTERNAL_ERROR      500
#define HTTP_SERVICE_UNAVAILABLE 503

/**
 * @brief 发送 JSON 成功响应
 */
esp_err_t send_success(httpd_req_t *req, cJSON *data, const char *message);

/**
 * @brief 发送 JSON 错误响应
 */
esp_err_t send_error(httpd_req_t *req, const char *message, int http_code);

/**
 * @brief 发送 Bad Request 响应
 */
esp_err_t send_bad_request(httpd_req_t *req, const char *message);

/**
 * @brief 发送 404 Not Found 响应
 */
esp_err_t send_not_found(httpd_req_t *req, const char *message);

/**
 * @brief 发送内部错误响应
 */
esp_err_t send_internal_error(httpd_req_t *req, const char *message);

/**
 * @brief 发送成功响应（仅消息，无数据）
 */
esp_err_t send_success_msg(httpd_req_t *req, const char *message);

/**
 * @brief 解析请求体中的 JSON
 */
cJSON *parse_request_json(httpd_req_t *req);

#endif // WEB_MODULE_H
