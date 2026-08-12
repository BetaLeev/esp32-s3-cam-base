/**
 * @file led_web.h
 * @brief LED Web API接口
 */

#ifndef LED_WEB_H
#define LED_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 注册LED Web路由
 */
void led_web_register_routes(httpd_handle_t server);

/**
 * @brief API: LED控制
 * GET /api/led
 *
 * 参数:
 *   pin          - GPIO引脚号 (0-39)
 *   action       - start/stop/config
 *   trigger_mode - static/blink
 *   initial_level - 0=低/1=高
 *   high_duration - 高电平时长(秒)
 *   low_duration  - 低电平时长(秒)
 *   repeat_count  - 重复次数，-1=无限
 */
esp_err_t led_web_handler(httpd_req_t *req);

/**
 * @brief API: 获取已使用的GPIO引脚
 * GET /api/gpio/used
 */
esp_err_t gpio_used_web_handler(httpd_req_t *req);

#endif // LED_WEB_H
