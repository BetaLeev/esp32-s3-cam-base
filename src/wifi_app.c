/**
 * @file wifi_app.c
 * @brief Wi-Fi应用模块实现 - ESP32-S3 AP+STA共存模式
 * @note 使用ESP-IDF 4.x标准API
 */
#include "wifi_app.h"
#include "config.h"
#include "config/hw_wifi.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "lwip/ip_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WIFI_APP";

static esp_netif_t *g_ap_netif = NULL;
static esp_netif_t *g_sta_netif = NULL;

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
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "客户端断开: AID=%d", event->aid);
                break;
            }

            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "STA模式已启动，正在连接Wi-Fi: %s", WIFI_STA_SSID);
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_CONNECTED: {
                wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*) event_data;
                ESP_LOGI(TAG, "已连接到Wi-Fi: %s, 信道: %d, BSSID: %02x:%02x:%02x:%02x:%02x:%02x",
                         WIFI_STA_SSID, event->channel,
                         event->bssid[0], event->bssid[1], event->bssid[2],
                         event->bssid[3], event->bssid[4], event->bssid[5]);
                
                // 更新全局状态
                g_system_status.sta_connected = 1;
                snprintf(g_system_status.sta_bssid, sizeof(g_system_status.sta_bssid),
                        "%02x:%02x:%02x:%02x:%02x:%02x",
                        event->bssid[0], event->bssid[1], event->bssid[2],
                        event->bssid[3], event->bssid[4], event->bssid[5]);
                break;
            }

            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGI(TAG, "Wi-Fi断开连接, 原因: %d, 正在重连...", event->reason);
                
                // 更新全局状态
                g_system_status.sta_connected = 0;
                g_system_status.sta_rssi = 0;
                
                // 尝试重新连接
                esp_wifi_connect();
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
static void wifi_app_update_rssi(void)
{
    if (g_system_status.sta_connected) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            g_system_status.sta_rssi = ap_info.rssi;
        }
    }
}

/**
 * @brief Wi-Fi状态监控任务 - 定期更新RSSI
 */
static void wifi_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Wi-Fi监控任务启动");

    while (1) {
        wifi_app_update_rssi();
        vTaskDelay(pdMS_TO_TICKS(5000));  // 每5秒更新一次
    }
}

/**
 * @brief Wi-Fi应用初始化 - AP+STA共存模式
 */
esp_err_t wifi_app_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "开始初始化Wi-Fi (AP+STA共存模式)...");

    /* 初始化网络接口 */
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "网络接口初始化完成");

    /* 创建默认事件循环 */
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建事件循环失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "事件循环创建完成");

    /* 创建AP网络接口 */
    g_ap_netif = esp_netif_create_default_wifi_ap();
    if (g_ap_netif == NULL) {
        ESP_LOGE(TAG, "创建AP网络接口失败");
        return ESP_FAIL;
    }

    /* 停止DHCP服务（设置静态IP前必须先停止） */
    esp_netif_dhcps_stop(g_ap_netif);

    /* 配置AP静态IP（从config.h读取） */
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, WIFI_AP_IP_1, WIFI_AP_IP_2, WIFI_AP_IP_3, WIFI_AP_IP_4);
    IP4_ADDR(&ip_info.gw, WIFI_AP_IP_1, WIFI_AP_IP_2, WIFI_AP_IP_3, WIFI_AP_IP_4);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(g_ap_netif, &ip_info));

    /* 重新启动DHCP服务 */
    esp_netif_dhcps_start(g_ap_netif);

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
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                    &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册Wi-Fi事件处理器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 注册IP事件处理函数 */
    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                    &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册IP事件处理器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP,
                                    &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册IP丢失事件处理器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 配置Wi-Fi为AP+STA共存模式 */
    ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置Wi-Fi模式失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 配置AP参数 */
    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASSWORD,
            .channel = WIFI_CHANNEL,
            .max_connection = WIFI_MAX_CONNECT,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false
            }
        }
    };

    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置AP配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 配置STA参数 */
    wifi_config_t sta_config = {
        .sta = {
            .ssid = WIFI_STA_SSID,
            .password = WIFI_STA_PASSWORD,
            .scan_method = WIFI_FAST_SCAN,
            .threshold = {
                .rssi = 0,
                .authmode = WIFI_AUTH_OPEN
            }
        }
    };

    ret = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置STA配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 启动Wi-Fi */
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动Wi-Fi失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Wi-Fi AP+STA共存模式初始化完成");
    ESP_LOGI(TAG, "AP模式:");
    ESP_LOGI(TAG, "  SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "  IP: %d.%d.%d.%d", WIFI_AP_IP_1, WIFI_AP_IP_2, WIFI_AP_IP_3, WIFI_AP_IP_4);
    ESP_LOGI(TAG, "  Channel: %d", WIFI_CHANNEL);
    ESP_LOGI(TAG, "STA模式:");
    ESP_LOGI(TAG, "  目标Wi-Fi: %s", WIFI_STA_SSID);
    ESP_LOGI(TAG, "  注意: Captive Portal通过HTTP服务器实现");

    /* 创建Wi-Fi监控任务 */
    ret = xTaskCreate(wifi_monitor_task, "wifi_monitor", 2048, NULL, 3, NULL);
    if (ret != pdPASS) {
        ESP_LOGW(TAG, "创建Wi-Fi监控任务失败");
    } else {
        ESP_LOGI(TAG, "Wi-Fi监控任务已创建");
    }

    return ESP_OK;
}

/**
 * @brief 获取Wi-Fi事件句柄（兼容性接口）
 */
void* wifi_app_get_event_handle(void)
{
    return NULL;  // 旧版API不需要此参数
}

/**
 * @brief 获取AP网络接口
 */
esp_netif_t* wifi_app_get_netif(void)
{
    return g_ap_netif;
}

/**
 * @brief 获取STA网络接口
 */
esp_netif_t* wifi_app_get_sta_netif(void)
{
    return g_sta_netif;
}

/**
 * @brief 获取本机IP地址字符串 (优先返回STA IP)
 */
void wifi_app_get_ip_string(char* ip_str, size_t max_len)
{
    // 优先返回STA IP地址（如果已连接）
    if (g_sta_netif != NULL && g_system_status.sta_connected) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(g_sta_netif, &ip_info) == ESP_OK) {
            snprintf(ip_str, max_len, IPSTR, IP2STR(&ip_info.ip));
            return;
        }
    }
    
    // 否则返回AP IP地址
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
void wifi_app_get_sta_ip_string(char* ip_str, size_t max_len)
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
void wifi_app_get_ap_ip_string(char* ip_str, size_t max_len)
{
    if (g_ap_netif != NULL) {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(g_ap_netif, &ip_info);
        snprintf(ip_str, max_len, IPSTR, IP2STR(&ip_info.ip));
    } else {
        snprintf(ip_str, max_len, "0.0.0.0");
    }
}
