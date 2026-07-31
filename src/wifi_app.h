/**
 * @brief Wi-Fi应用模块头文件 - ESP32-S3 AP+STA共存模式
 */
#ifndef WIFI_APP_H
#define WIFI_APP_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_netif.h"

/**
 * @brief Wi-Fi应用初始化
 * @note 配置ESP32-S3为AP+STA共存模式
 * @return ESP_OK成功初始化，其他失败
 */
esp_err_t wifi_app_init(void);

/**
 * @brief 获取Wi-Fi事件句柄
 * @return 事件循环句柄（兼容性接口）
 */
void* wifi_app_get_event_handle(void);

/**
 * @brief 获取AP网络接口
 * @return esp_netif_t* AP网络接口指针
 */
esp_netif_t* wifi_app_get_netif(void);

/**
 * @brief 获取STA网络接口
 * @return esp_netif_t* STA网络接口指针
 */
esp_netif_t* wifi_app_get_sta_netif(void);

/**
 * @brief 获取本机IP地址字符串 (优先返回STA IP)
 * @param ip_str 输出缓冲区
 * @param max_len 缓冲区最大长度
 */
void wifi_app_get_ip_string(char* ip_str, size_t max_len);

/**
 * @brief 获取STA IP地址字符串
 * @param ip_str 输出缓冲区
 * @param max_len 缓冲区最大长度
 */
void wifi_app_get_sta_ip_string(char* ip_str, size_t max_len);

/**
 * @brief 获取AP IP地址字符串
 * @param ip_str 输出缓冲区
 * @param max_len 缓冲区最大长度
 */
void wifi_app_get_ap_ip_string(char* ip_str, size_t max_len);

#endif /* WIFI_APP_H */
