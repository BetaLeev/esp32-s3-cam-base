/**
 * @file wifi.c
 * @brief Wi-Fi应用模块实现 - ESP32-S3 AP+STA共存模式
 */
#include "wifi.h"
#include "../config.h"
#include "config/hw_wifi.h"
#include "wifi_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "lwip/ip_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "WIFI";

static esp_netif_t *g_ap_netif = NULL;
static esp_netif_t *g_sta_netif = NULL;
static bool g_sta_connected = false;
static int8_t g_sta_rssi = 0;
static char g_sta_bssid[18] = {0};

/**
 * @brief Wi-Fi事件处理函数
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP热点已启动");
                break;

            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "AP热点已停止");
                break;

            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "客户端连接: AID=%d", event->aid);
                wifi_config_update_ap_clients(wifi_config_get_ap_clients_internal() + 1);
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "客户端断开: AID=%d", event->aid);
                uint8_t current = wifi_config_get_ap_clients_internal();
                if (current > 0) {
                    wifi_config_update_ap_clients(current - 1);
                }
                break;
            }

            case WIFI_EVENT_STA_CONNECTED: {
                wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*) event_data;
                g_sta_connected = true;
                snprintf(g_sta_bssid, sizeof(g_sta_bssid), "%02x:%02x:%02x:%02x:%02x:%02x",
                         event->bssid[0], event->bssid[1], event->bssid[2],
                         event->bssid[3], event->bssid[4], event->bssid[5]);
                ESP_LOGI(TAG, "已连接到Wi-Fi: 信道=%d, BSSID=%s", event->channel, g_sta_bssid);
                wifi_config_update_sta_connected(true);
                break;
            }

            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                g_sta_connected = false;
                g_sta_rssi = 0;
                ESP_LOGI(TAG, "Wi-Fi断开连接, 原因: %d", event->reason);
                wifi_config_update_sta_connected(false);
                break;
            }

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                char ip_str[16];
                snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&event->ip_info.ip));
                ESP_LOGI(TAG, "STA获得IP地址: %s", ip_str);
                break;
            }

            case IP_EVENT_STA_LOST_IP:
                ESP_LOGI(TAG, "STA失去IP地址");
                break;

            default:
                break;
        }
    }
}

/**
 * @brief 更新STA信号强度RSSI
 */
static void update_rssi(void)
{
    if (g_sta_connected) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            g_sta_rssi = ap_info.rssi;
        }
    }
}

/**
 * @brief Wi-Fi状态监控任务
 */
static void wifi_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Wi-Fi监控任务启动");
    while (1) {
        update_rssi();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/**
 * @brief Wi-Fi应用初始化 - AP+STA共存模式
 */
esp_err_t wifi_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "开始初始化Wi-Fi (AP+STA混合模式)...");

    /* 初始化 WiFi 配置管理 */
    ret = wifi_config_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi配置初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 初始化网络接口 */
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 创建默认事件循环 */
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "创建事件循环失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 创建AP网络接口 */
    g_ap_netif = esp_netif_create_default_wifi_ap();
    if (g_ap_netif == NULL) {
        ESP_LOGE(TAG, "创建AP网络接口失败");
        return ESP_FAIL;
    }

    /* 配置AP静态IP */
    ret = esp_netif_dhcps_stop(g_ap_netif);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "停止DHCP失败，继续配置");
    }

    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, WIFI_AP_IP_ADDR_1, WIFI_AP_IP_ADDR_2, WIFI_AP_IP_ADDR_3, WIFI_AP_IP_ADDR_4);
    IP4_ADDR(&ip_info.gw, WIFI_AP_GW_1, WIFI_AP_GW_2, WIFI_AP_GW_3, WIFI_AP_GW_4);
    IP4_ADDR(&ip_info.netmask, WIFI_AP_NETMASK_1, WIFI_AP_NETMASK_2, WIFI_AP_NETMASK_3, WIFI_AP_NETMASK_4);
    ret = esp_netif_set_ip_info(g_ap_netif, &ip_info);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "设置IP失败: %s", esp_err_to_name(ret));
    }

    ret = esp_netif_dhcps_start(g_ap_netif);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "启动DHCP失败: %s", esp_err_to_name(ret));
    }

    /* 创建STA网络接口 */
    g_sta_netif = esp_netif_create_default_wifi_sta();
    if (g_sta_netif == NULL) {
        ESP_LOGE(TAG, "创建STA网络接口失败");
        return ESP_FAIL;
    }

    /* 配置Wi-Fi初始化参数 */
    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&init_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 注册Wi-Fi事件处理函数 */
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (ret != ESP_OK) return ret;

    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
    if (ret != ESP_OK) return ret;

    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &wifi_event_handler, NULL);
    if (ret != ESP_OK) return ret;

    /* 获取配置并启动 */
    const app_wifi_config_t *cfg = wifi_config_get_full();

    /* 配置Wi-Fi为AP+STA共存模式 */
    wifi_mode_t mode = WIFI_MODE_APSTA;
    if (cfg) {
        switch (cfg->mode) {
            case APP_WIFI_MODE_STA: mode = WIFI_MODE_STA; break;
            case APP_WIFI_MODE_AP: mode = WIFI_MODE_AP; break;
            case APP_WIFI_MODE_AP_STA: mode = WIFI_MODE_APSTA; break;
            default: mode = WIFI_MODE_APSTA; break;
        }
    }

    ret = esp_wifi_set_mode(mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置Wi-Fi模式失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 配置AP参数 */
    wifi_config_t ap_config = {0};

    /* 设置SSID */
    if (cfg && strlen(cfg->ap.ssid) > 0) {
        strncpy((char*)ap_config.ap.ssid, cfg->ap.ssid, sizeof(ap_config.ap.ssid) - 1);
    } else {
        strncpy((char*)ap_config.ap.ssid, WIFI_AP_SSID, sizeof(ap_config.ap.ssid) - 1);
    }
    ap_config.ap.ssid_len = strlen((char*)ap_config.ap.ssid);

    /* 设置密码 */
    if (cfg && strlen(cfg->ap.password) >= 8) {
        strncpy((char*)ap_config.ap.password, cfg->ap.password, sizeof(ap_config.ap.password) - 1);
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        strncpy((char*)ap_config.ap.password, WIFI_AP_PASSWORD, sizeof(ap_config.ap.password) - 1);
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ap_config.ap.channel = (cfg && cfg->ap.channel > 0) ? cfg->ap.channel : WIFI_AP_CHANNEL;
    ap_config.ap.max_connection = WIFI_AP_MAX_CONNECTIONS;
    ap_config.ap.pmf_cfg.required = false;

    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置AP配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 启动Wi-Fi */
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动Wi-Fi失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Wi-Fi混合模式初始化完成");
    ESP_LOGI(TAG, "AP模式: SSID=%s", ap_config.ap.ssid);

    /* 如果有保存的STA配置，自动连接 */
    if (cfg && cfg->sta.auto_connect && strlen(cfg->sta.ssid) > 0) {
        ESP_LOGI(TAG, "正在连接保存的WiFi: %s", cfg->sta.ssid);
        wifi_config_connect(cfg->sta.ssid, cfg->sta.password);
    }

    /* 创建Wi-Fi监控任务 */
    xTaskCreate(wifi_monitor_task, "wifi_monitor", 2048, NULL, 3, NULL);

    return ESP_OK;
}

/**
 * @brief 获取AP网络接口
 */
esp_netif_t* wifi_get_netif(void)
{
    return g_ap_netif;
}

/**
 * @brief 获取STA网络接口
 */
esp_netif_t* wifi_get_sta_netif(void)
{
    return g_sta_netif;
}

/**
 * @brief 获取本机IP地址字符串 (优先返回STA IP)
 */
void wifi_get_ip_string(char* ip_str, size_t max_len)
{
    if (g_sta_netif != NULL && g_sta_connected) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(g_sta_netif, &ip_info) == ESP_OK) {
            snprintf(ip_str, max_len, IPSTR, IP2STR(&ip_info.ip));
            return;
        }
    }

    if (g_ap_netif != NULL) {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(g_ap_netif, &ip_info);
        snprintf(ip_str, max_len, IPSTR, IP2STR(&ip_info.ip));
    } else {
        snprintf(ip_str, max_len, "0.0.0.0");
    }
}

/**
 * @brief 获取STA IP地址字符串
 */
void wifi_get_sta_ip_string(char* ip_str, size_t max_len)
{
    if (g_sta_netif != NULL) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(g_sta_netif, &ip_info) == ESP_OK) {
            snprintf(ip_str, max_len, IPSTR, IP2STR(&ip_info.ip));
            return;
        }
    }
    snprintf(ip_str, max_len, "0.0.0.0");
}

/**
 * @brief 获取AP IP地址字符串
 */
void wifi_get_ap_ip_string(char* ip_str, size_t max_len)
{
    if (g_ap_netif != NULL) {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(g_ap_netif, &ip_info);
        snprintf(ip_str, max_len, IPSTR, IP2STR(&ip_info.ip));
    } else {
        snprintf(ip_str, max_len, "0.0.0.0");
    }
}

/**
 * @brief 获取STA连接状态
 */
bool wifi_is_sta_connected(void)
{
    return g_sta_connected;
}

/**
 * @brief 获取STA信号强度
 */
int8_t wifi_get_sta_rssi(void)
{
    return g_sta_rssi;
}

/**
 * @brief 获取STA的BSSID
 */
void wifi_get_sta_bssid(char* bssid, size_t max_len)
{
    snprintf(bssid, max_len, "%s", g_sta_bssid);
}
