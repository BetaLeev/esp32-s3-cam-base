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

/* ========================================
 * WiFi 运行模式（避免与 esp_wifi_types.h 冲突）
 * ======================================== */
typedef enum {
    APP_WIFI_MODE_STA = 0,     /**< 仅 STA 模式 */
    APP_WIFI_MODE_AP = 1,      /**< 仅 AP 模式 */
    APP_WIFI_MODE_AP_STA = 2    /**< AP+STA 混合模式 */
} app_wifi_mode_t;

/**
 * @brief WiFi STA 用户配置结构（避免与 esp_wifi_types.h 冲突）
 */
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN + 1];
    char password[WIFI_PASSWORD_MAX_LEN + 1];
    bool auto_connect;
} app_wifi_sta_config_t;

/**
 * @brief WiFi AP 用户配置结构（避免与 esp_wifi_types.h 冲突）
 */
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN + 1];
    char password[WIFI_PASSWORD_MAX_LEN + 1];
    uint8_t channel;
} app_wifi_ap_config_t;

/**
 * @brief WiFi 完整配置结构
 */
typedef struct {
    app_wifi_mode_t mode;
    app_wifi_sta_config_t sta;
    app_wifi_ap_config_t ap;
} app_wifi_config_t;

/* ========================================
 * 基础配置接口
 * ======================================== */

/**
 * @brief WiFi 配置初始化
 */
esp_err_t wifi_config_init(void);

/**
 * @brief WiFi 配置反初始化
 */
esp_err_t wifi_config_deinit(void);

/* ========================================
 * 扫描接口
 * ======================================== */

/**
 * @brief 保存 WiFi STA 配置到 NVS
 */
esp_err_t wifi_config_save_sta(const app_wifi_sta_config_t *config);

/**
 * @brief 从 NVS 加载 WiFi STA 配置
 */
esp_err_t wifi_config_load_sta(app_wifi_sta_config_t *config);

/**
 * @brief 获取当前 WiFi STA 配置
 */
const app_wifi_sta_config_t* wifi_config_get_sta(void);

/**
 * @brief 连接到指定 WiFi
 */
esp_err_t wifi_config_connect(const char *ssid, const char *password);

/**
 * @brief 断开 WiFi 连接
 */
esp_err_t wifi_config_disconnect(void);

/**
 * @brief 获取 STA 连接状态
 */
bool wifi_config_is_sta_connected(void);

/* ========================================
 * AP 配置接口
 * ======================================== */

/**
 * @brief 保存 WiFi AP 配置到 NVS
 */
esp_err_t wifi_config_save_ap(const app_wifi_ap_config_t *config);

/**
 * @brief 从 NVS 加载 WiFi AP 配置
 */
esp_err_t wifi_config_load_ap(app_wifi_ap_config_t *config);

/**
 * @brief 获取当前 WiFi AP 配置
 */
const app_wifi_ap_config_t* wifi_config_get_ap(void);

/**
 * @brief 启动 AP 热点
 */
esp_err_t wifi_config_start_ap(const char *ssid, const char *password, uint8_t channel);

/**
 * @brief 停止 AP 热点
 */
esp_err_t wifi_config_stop_ap(void);

/**
 * @brief 获取 AP 运行状态
 */
bool wifi_config_is_ap_running(void);

/**
 * @brief 获取 AP 连接的客户端数量
 */
int wifi_config_get_ap_clients(void);

/* ========================================
 * 模式切换接口
 * ======================================== */

/**
 * @brief 设置 WiFi 运行模式
 */
esp_err_t wifi_config_set_mode(app_wifi_mode_t mode);

/**
 * @brief 获取当前 WiFi 运行模式
 */
app_wifi_mode_t wifi_config_get_mode(void);

/**
 * @brief 保存完整配置到 NVS
 */
esp_err_t wifi_config_save_full(const app_wifi_config_t *config);

/**
 * @brief 从 NVS 加载完整配置
 */
esp_err_t wifi_config_load_full(app_wifi_config_t *config);

/**
 * @brief 获取完整配置
 */
const app_wifi_config_t* wifi_config_get_full(void);

/* ========================================
 * 扫描接口
 * ======================================== */

/**
 * @brief 扫描可用 WiFi 网络
 */
esp_err_t wifi_config_scan(wifi_ap_record_t **ap_list, uint16_t *ap_count);

/**
 * @brief 释放扫描结果内存
 */
void wifi_config_scan_free(void);

/* ========================================
 * 内部接口（供 wifi.c 调用）
 * ======================================== */

/**
 * @brief 更新 STA 连接状态（由 wifi_event_handler 调用）
 */
void wifi_config_update_sta_connected(bool connected);

/**
 * @brief 更新 AP 客户端数量（由 wifi_event_handler 调用）
 */
void wifi_config_update_ap_clients(uint8_t count);

/**
 * @brief 获取 AP 客户端数量（内部）
 */
uint8_t wifi_config_get_ap_clients_internal(void);

#endif /* WIFI_CONFIG_H */
