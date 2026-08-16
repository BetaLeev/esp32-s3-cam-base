/**
 * @file http.h
 * @brief HTTP 服务器
 */
#ifndef WEB_HTTP_H
#define WEB_HTTP_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 获取 HTTP 服务器句柄
 */
httpd_handle_t web_http_get_handle(void);

/**
 * @brief 初始化 HTTP 服务器
 */
esp_err_t web_http_init(void);

/**
 * @brief 反初始化 HTTP 服务器
 */
esp_err_t web_http_deinit(void);

#endif // WEB_HTTP_H
