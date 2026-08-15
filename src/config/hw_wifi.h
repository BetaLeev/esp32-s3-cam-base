/**
 * @file hw_wifi.h
 * @brief Wi-Fi 配置
 */
#ifndef HW_WIFI_H
#define HW_WIFI_H

#include <stdint.h>

/* ========================================
 * AP 模式配置 (热点)
 * ======================================== */
#define WIFI_AP_SSID           "myEsp32S3"
#define WIFI_AP_PASSWORD       "88888888"
#define WIFI_AP_CHANNEL        1
#define WIFI_AP_MAX_CONNECTIONS 10

/* AP 静态 IP 配置 */
#define WIFI_AP_IP_ADDR_1      192
#define WIFI_AP_IP_ADDR_2      168
#define WIFI_AP_IP_ADDR_3      4
#define WIFI_AP_IP_ADDR_4      1

#define WIFI_AP_NETMASK_1      255
#define WIFI_AP_NETMASK_2      255
#define WIFI_AP_NETMASK_3      255
#define WIFI_AP_NETMASK_4      0

#define WIFI_AP_GW_1           192
#define WIFI_AP_GW_2           168
#define WIFI_AP_GW_3           4
#define WIFI_AP_GW_4           1

/* ========================================
 * STA 模式配置 (连接外部 Wi-Fi)
 * ======================================== */
#define WIFI_STA_SSID          "xiangjiazhegebu"
#define WIFI_STA_PASSWORD      "bjbjbjbj"
#define WIFI_STA_CONNECT_TIMEOUT_MS   30000
#define WIFI_STA_SCAN_METHOD   WIFI_FAST_SCAN
#define WIFI_STA_SORT_METHOD   WIFI_CONNECT_AP_BY_SIGNAL

/* STA 静态 IP (0 = 使用 DHCP) */
#define WIFI_STA_STATIC_IP_1  0
#define WIFI_STA_STATIC_IP_2  0
#define WIFI_STA_STATIC_IP_3  0
#define WIFI_STA_STATIC_IP_4  0

#define WIFI_STA_STATIC_GW_1  0
#define WIFI_STA_STATIC_GW_2  0
#define WIFI_STA_STATIC_GW_3  0
#define WIFI_STA_STATIC_GW_4  0

#define WIFI_STA_STATIC_NETMASK_1  0
#define WIFI_STA_STATIC_NETMASK_2  0
#define WIFI_STA_STATIC_NETMASK_3  0
#define WIFI_STA_STATIC_NETMASK_4  0

/* ========================================
 * Wi-Fi 重连配置
 * ======================================== */
#define WIFI_RECONNECT_INTERVAL_MS    5000
#define WIFI_RECONNECT_MAX_RETRIES   10

/* ========================================
 * 功率配置
 * ======================================== */
#define WIFI_TX_POWER_WATT    WIFI_POWER_19dBm  /**< 最大发射功率 */

#endif /* HW_WIFI_H */
