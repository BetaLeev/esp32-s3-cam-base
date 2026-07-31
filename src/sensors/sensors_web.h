/**
 * @file sensors_web.h
 * @brief 传感器Web API接口
 */

#ifndef SENSORS_WEB_H
#define SENSORS_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief API: 获取传感器数据
 */
esp_err_t sensors_web_get_data_handler(httpd_req_t *req);

#endif // SENSORS_WEB_H
