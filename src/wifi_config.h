/**
 * @file wifi_config.h
 * @brief WiFi 配置管理模块 - NVS持久化存储
 */
#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include "esp_err.h"
#include "esp_wifi_types.h"

#define WIFI_CONFIG_NAMESPACE "wifi_cfg"
#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASSWORD_MAX_LEN 64

/**
 * @brief WiFi STA 用户配置结构
 */
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN + 1];
    char password[WIFI_PASSWORD_MAX_LEN + 1];
    bool auto_connect;
} wifi_user_config_t;

/**
 * @brief WiFi 配置初始化
 */
esp_err_t wifi_config_init(void);

/**
 * @brief 保存 WiFi STA 配置到 NVS
 */
esp_err_t wifi_config_save(const wifi_user_config_t *config);

/**
 * @brief 从 NVS 加载 WiFi STA 配置
 */
esp_err_t wifi_config_load(wifi_user_config_t *config);

/**
 * @brief 获取当前 WiFi STA 配置
 */
const wifi_user_config_t* wifi_config_get_current(void);

/**
 * @brief 连接到指定 WiFi
 */
esp_err_t wifi_config_connect(const char *ssid, const char *password);

/**
 * @brief 断开 WiFi 连接
 */
esp_err_t wifi_config_disconnect(void);

/**
 * @brief 扫描可用 WiFi 网络
 */
esp_err_t wifi_config_scan(wifi_ap_record_t **ap_list, uint16_t *ap_count);

/**
 * @brief 释放扫描结果内存
 */
void wifi_config_scan_free(void);

/**
 * @brief 获取 WiFi 连接状态
 */
bool wifi_config_is_connected(void);

#endif /* WIFI_CONFIG_H */
