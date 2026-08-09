/**
 * @file sensors_web.h
 * @brief 传感器 Web API 接口
 */

#ifndef SENSORS_WEB_H
#define SENSORS_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 注册传感器模块的所有 URI 路由
 */
void sensors_web_register_routes(httpd_handle_t server);

/**
 * @brief API: 获取传感器数据
 * GET /api/sensors/data
 */
esp_err_t sensors_web_get_data_handler(httpd_req_t *req);

/**
 * @brief API: 获取传感器配置
 * GET /api/sensors/config
 */
esp_err_t sensors_web_get_config_handler(httpd_req_t *req);

/**
 * @brief API: 配置传感器引脚
 * POST /api/sensors/config
 */
esp_err_t sensors_web_set_config_handler(httpd_req_t *req);

#endif // SENSORS_WEB_H