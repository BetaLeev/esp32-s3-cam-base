/**
 * @file actuators_web.h
 * @brief 执行器Web API接口
 */

#ifndef ACTUATORS_WEB_H
#define ACTUATORS_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief API: 控制舵机
 */
esp_err_t actuators_web_servo_handler(httpd_req_t *req);

/**
 * @brief API: 控制电机
 */
esp_err_t actuators_web_motor_handler(httpd_req_t *req);

#endif // ACTUATORS_WEB_H
