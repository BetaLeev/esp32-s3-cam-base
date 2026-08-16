/**
 * @file wifi.h
 * @brief Wi-Fi应用模块头文件 - ESP32-S3 AP+STA共存模式
 */
#ifndef WIFI_H
#define WIFI_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_netif.h"

/**
 * @brief Wi-Fi应用初始化
 * @return ESP_OK成功初始化，其他失败
 */
esp_err_t wifi_init(void);

/**
 * @brief 获取AP网络接口
 */
esp_netif_t* wifi_get_netif(void);

/**
 * @brief 获取STA网络接口
 */
esp_netif_t* wifi_get_sta_netif(void);

/**
 * @brief 获取本机IP地址字符串 (优先返回STA IP)
 */
void wifi_get_ip_string(char* ip_str, size_t max_len);

/**
 * @brief 获取STA IP地址字符串
 */
void wifi_get_sta_ip_string(char* ip_str, size_t max_len);

/**
 * @brief 获取AP IP地址字符串
 */
void wifi_get_ap_ip_string(char* ip_str, size_t max_len);

/**
 * @brief 获取STA连接状态
 */
bool wifi_is_sta_connected(void);

/**
 * @brief 获取STA信号强度
 */
int8_t wifi_get_sta_rssi(void);

/**
 * @brief 获取STA的BSSID
 */
void wifi_get_sta_bssid(char* bssid, size_t max_len);

/**
 * @brief 重置重试次数计数器
 */
void wifi_reset_retry_count(void);

#endif /* WIFI_H */