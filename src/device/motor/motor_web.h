/**
 * @file motor_web.h
 * @brief 电机/水泵 Web API接口
 */

#ifndef MOTOR_WEB_H
#define MOTOR_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief API: 水泵控制 (GET /api/pump?gear=0-3)
 */
esp_err_t motor_web_pump_handler(httpd_req_t *req);

/**
 * @brief API: 电机控制 (GET /api/motor?cmd=start|stop|speed)
 */
esp_err_t motor_web_handler(httpd_req_t *req);

#endif // MOTOR_WEB_H
