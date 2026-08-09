/**
 * @file wifi_web.h
 * @brief Wi-Fi Web API 接口
 */
#ifndef WIFI_WEB_H
#define WIFI_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 注册 Wi-Fi 模块的所有 URI 路由
 * @param server HTTP 服务器句柄
 */
void wifi_web_register_routes(httpd_handle_t server);

#endif /* WIFI_WEB_H */
