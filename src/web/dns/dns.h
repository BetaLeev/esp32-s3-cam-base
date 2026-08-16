/**
 * @file dns.h
 * @brief DNS 服务器 - Captive Portal DNS 劫持
 */
#ifndef WEB_DNS_H
#define WEB_DNS_H

#include "esp_err.h"

/**
 * @brief 初始化 DNS 服务器
 */
esp_err_t web_dns_init(void);

/**
 * @brief 反初始化 DNS 服务器
 */
esp_err_t web_dns_deinit(void);

/**
 * @brief 设置 DNS 返回的 AP IP 地址
 */
void web_dns_set_ap_ip(uint32_t ip);

#endif // WEB_DNS_H
