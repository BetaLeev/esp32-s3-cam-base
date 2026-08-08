/**
 * @file wifi_web.h
 * @brief WiFi Web API 处理模块
 */
#ifndef WIFI_WEB_H
#define WIFI_WEB_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief API: 获取 WiFi 状态
 */
esp_err_t wifi_web_status_handler(httpd_req_t *req);

/**
 * @brief API: 获取 WiFi 配置
 */
esp_err_t wifi_web_config_handler(httpd_req_t *req);

/**
 * @brief API: 设置 WiFi 配置并连接
 */
esp_err_t wifi_web_set_handler(httpd_req_t *req);

/**
 * @brief API: 扫描 WiFi 网络
 */
esp_err_t wifi_web_scan_handler(httpd_req_t *req);

/**
 * @brief API: 断开 WiFi 连接
 */
esp_err_t wifi_web_disconnect_handler(httpd_req_t *req);

#endif /* WIFI_WEB_H */
