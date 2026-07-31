/**
 * @file dns_server.h
 * @brief DNS服务器头文件 - Captive Portal DNS劫持
 */
#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include "esp_err.h"

/**
 * @brief DNS服务器初始化
 * @note 启动DNS服务器，拦截所有DNS查询并返回AP的IP地址
 * @return ESP_OK成功初始化，其他失败
 */
esp_err_t dns_server_init(void);

/**
 * @brief DNS服务器反初始化
 * @return ESP_OK成功，其他失败
 */
esp_err_t dns_server_deinit(void);

#endif /* DNS_SERVER_H */
