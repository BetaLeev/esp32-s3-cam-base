/**
 * @file web_module.h
 * @brief Web 模块通用工具 - JSON 响应封装
 *
 * 提供统一的 JSON 响应格式和 HTTP 状态码处理
 */
#ifndef WEB_MODULE_H
#define WEB_MODULE_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "cJSON.h"

/* ========================================
 * HTTP 状态码定义
 * ======================================== */
typedef enum {
    HTTP_OK = 200,              /**< 成功 */
    HTTP_CREATED = 201,         /**< 创建成功 */
    HTTP_BAD_REQUEST = 400,     /**< 请求参数错误 */
    HTTP_NOT_FOUND = 404,       /**< 资源不存在 */
    HTTP_INTERNAL_ERROR = 500,   /**< 服务器内部错误 */
    HTTP_SERVICE_UNAVAILABLE = 503 /**< 服务不可用 */
} http_status_code_t;

/* ========================================
 * 响应状态定义
 * ======================================== */
typedef enum {
    RESP_SUCCESS = 0,   /**< 成功 */
    RESP_ERROR = 1,    /**< 错误 */
    RESP_WARNING = 2    /**< 警告 */
} response_status_t;

/* ========================================
 * JSON 响应封装函数
 * ======================================== */

/**
 * @brief 发送 JSON 响应
 *
 * 统一响应格式：
 * @code
 * {
 *     "status": "success" | "error" | "warning",
 *     "data": {...} | null,
 *     "message": "描述信息",
 *     "code": 200
 * }
 * @endcode
 *
 * @param req HTTP 请求
 * @param status 响应状态
 * @param data 数据对象（可为空）
 * @param message 消息
 * @param http_code HTTP 状态码
 * @return ESP_OK 成功
 */
esp_err_t send_json_response(httpd_req_t *req,
                              response_status_t status,
                              cJSON *data,
                              const char *message,
                              http_status_code_t http_code);

/**
 * @brief 发送成功响应
 */
static inline esp_err_t send_success(httpd_req_t *req, cJSON *data, const char *message)
{
    return send_json_response(req, RESP_SUCCESS, data, message, HTTP_OK);
}

/**
 * @brief 发送成功响应（无数据）
 */
static inline esp_err_t send_success_msg(httpd_req_t *req, const char *message)
{
    return send_json_response(req, RESP_SUCCESS, NULL, message, HTTP_OK);
}

/**
 * @brief 发送成功响应（带数据）
 */
static inline esp_err_t send_success_data(httpd_req_t *req, cJSON *data)
{
    return send_json_response(req, RESP_SUCCESS, data, "操作成功", HTTP_OK);
}

/**
 * @brief 发送错误响应
 */
static inline esp_err_t send_error(httpd_req_t *req, const char *message, http_status_code_t http_code)
{
    return send_json_response(req, RESP_ERROR, NULL, message, http_code);
}

/**
 * @brief 发送 Bad Request 错误 (400)
 */
static inline esp_err_t send_bad_request(httpd_req_t *req, const char *message)
{
    return send_json_response(req, RESP_ERROR, NULL, message, HTTP_BAD_REQUEST);
}

/**
 * @brief 发送 Not Found 错误 (404)
 */
static inline esp_err_t send_not_found(httpd_req_t *req, const char *message)
{
    return send_json_response(req, RESP_ERROR, NULL, message, HTTP_NOT_FOUND);
}

/**
 * @brief 发送内部错误 (500)
 */
static inline esp_err_t send_internal_error(httpd_req_t *req, const char *message)
{
    return send_json_response(req, RESP_ERROR, NULL, message, HTTP_INTERNAL_ERROR);
}

/* ========================================
 * JSON 辅助函数
 * ======================================== */

/**
 * @brief 创建传感器数据对象
 */
cJSON *json_create_sensor_data(const char *name, cJSON *values);

/**
 * @brief 添加整数字段到对象
 */
void json_add_int(cJSON *obj, const char *key, int value);

/**
 * @brief 添加浮点数字段到对象
 */
void json_add_float(cJSON *obj, const char *key, double value, int decimals);

/**
 * @brief 添加布尔字段到对象
 */
void json_add_bool(cJSON *obj, const char *key, bool value);

/**
 * @brief 添加字符串字段到对象
 */
void json_add_string(cJSON *obj, const char *key, const char *value);

/**
 * @brief 安全获取字符串参数
 *
 * @param req HTTP 请求
 * @param key 参数名
 * @param buf 缓冲区
 * @param buf_len 缓冲区大小
 * @return true 成功获取，false 未找到
 */
bool get_query_string(httpd_req_t *req, const char *key, char *buf, size_t buf_len);

/**
 * @brief 解析请求体为 JSON
 *
 * @param req HTTP 请求
 * @return cJSON 对象，调用者需释放
 */
cJSON *parse_request_json(httpd_req_t *req);

#endif /* WEB_MODULE_H */
