/**
 * @file pulse_web.h
 * @brief 脉冲控制 Web API 接口
 */

#ifndef PULSE_WEB_H
#define PULSE_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief API: 脉冲控制
 * GET /api/pulse
 */
esp_err_t pulse_web_handler(httpd_req_t *req);

/**
 * @brief 注册脉冲模块的 URI 路由
 */
void pulse_web_register_routes(httpd_handle_t server);

#endif // PULSE_WEB_H
