/**
 * @file wifi.h
 * @brief Wi-Fi应用模块头文件 - ESP32-S3 AP+STA共存模式
 */
#ifndef WIFI_H
#define WIFI_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_netif.h"

/**
 * @brief Wi-Fi应用初始化
 * @note 配置ESP32-S3为AP+STA共存模式
 * @return ESP_OK成功初始化，其他失败
 */
esp_err_t wifi_init(void);

/**
 * @brief 获取AP网络接口
 * @return esp_netif_t* AP网络接口指针
 */
esp_netif_t* wifi_get_netif(void);

/**
 * @brief 获取STA网络接口
 * @return esp_netif_t* STA网络接口指针
 */
esp_netif_t* wifi_get_sta_netif(void);

/**
 * @brief 获取本机IP地址字符串 (优先返回STA IP)
 * @param ip_str 输出缓冲区
 * @param max_len 缓冲区最大长度
 */
void wifi_get_ip_string(char* ip_str, size_t max_len);

/**
 * @brief 获取STA IP地址字符串
 * @param ip_str 输出缓冲区
 * @param max_len 缓冲区最大长度
 */
void wifi_get_sta_ip_string(char* ip_str, size_t max_len);

/**
 * @brief 获取AP IP地址字符串
 * @param ip_str 输出缓冲区
 * @param max_len 缓冲区最大长度
 */
void wifi_get_ap_ip_string(char* ip_str, size_t max_len);

/**
 * @brief 获取STA连接状态
 * @return true 已连接，false 未连接
 */
bool wifi_is_sta_connected(void);

/**
 * @brief 获取STA信号强度
 * @return RSSI值
 */
int8_t wifi_get_sta_rssi(void);

/**
 * @brief 获取STA的BSSID
 * @param bssid 输出缓冲区(至少18字节)
 */
void wifi_get_sta_bssid(char* bssid, size_t max_len);

#endif /* WIFI_H */
