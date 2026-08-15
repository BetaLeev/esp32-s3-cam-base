/**
 * @file servo_web.h
 * @brief 舵机 Web API接口
 */

#ifndef SERVO_WEB_H
#define SERVO_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief API: 舵机控制 (GET /api/servo?angle=0-180)
 */
esp_err_t servo_web_handler(httpd_req_t *req);

#endif // SERVO_WEB_H
