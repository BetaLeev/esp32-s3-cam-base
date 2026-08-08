/**
 * @file sensors_web.h
 * @brief 传感器 Web API 接口
 */

#ifndef SENSORS_WEB_H
#define SENSORS_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief API: 获取传感器数据
 * GET /api/sensors/data
 *
 * 响应示例:
 * @code
 * {
 *     "status": "success",
 *     "code": 200,
 *     "message": "获取传感器数据成功",
 *     "data": {
 *         "thermistor": {
 *             "gpio": 5,
 *             "raw": 2048,
 *             "temperature": 25.0
 *         },
 *         "photosensor": {
 *             "gpio": 3,
 *             "raw": 1500,
 *             "light": 350.0
 *         },
 *         "dht11": {
 *             "gpio": 4,
 *             "temperature": 25.0,
 *             "humidity": 60.0,
 *             "valid": true
 *         }
 *     }
 * }
 * @endcode
 */
esp_err_t sensors_web_get_data_handler(httpd_req_t *req);

/**
 * @brief API: 获取传感器配置
 * GET /api/sensors/config
 *
 * 响应示例:
 * @code
 * {
 *     "status": "success",
 *     "code": 200,
 *     "message": "获取传感器配置成功",
 *     "data": {
 *         "thermistor": { "gpio": 5, "type": "thermistor", "unit": "celsius" },
 *         "photosensor": { "gpio": 3, "type": "photosensor", "unit": "lux" },
 *         "dht11": { "gpio": 4, "type": "dht11" }
 *     }
 * }
 * @endcode
 */
esp_err_t sensors_web_get_config_handler(httpd_req_t *req);

/**
 * @brief API: 配置传感器引脚
 * POST /api/sensors/config
 *
 * 请求体:
 * @code
 * {
 *     "thermistor_gpio": 5,
 *     "photosensor_gpio": 3,
 *     "dht11_gpio": 4
 * }
 * @endcode
 */
esp_err_t sensors_web_set_config_handler(httpd_req_t *req);

#endif // SENSORS_WEB_H
