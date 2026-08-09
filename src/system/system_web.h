/**
 * @file system_web.h
 * @brief 系统管理 Web API 接口
 */
#ifndef SYSTEM_WEB_H
#define SYSTEM_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 注册系统模块的所有 URI 路由
 * @param server HTTP 服务器句柄
 */
void system_web_register_routes(httpd_handle_t server);

/**
 * @brief API: 获取系统状态
 * GET /api/system/status
 */
esp_err_t system_web_status_handler(httpd_req_t *req);

/**
 * @brief API: 获取硬件资源信息
 * GET /api/system/resources
 */
esp_err_t system_web_resources_handler(httpd_req_t *req);

#endif /* SYSTEM_WEB_H */
