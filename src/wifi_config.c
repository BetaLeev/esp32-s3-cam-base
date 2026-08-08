/**
 * @file wifi_config.c
 * @brief WiFi 配置管理模块实现 - NVS持久化存储
 */
#include "wifi_config.h"
#include "wifi_app.h"
#include "config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_event.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WIFI_CONFIG";
#define LOG_TAG TAG

static wifi_user_config_t g_wifi_config = {0};
static bool g_connected = false;
static wifi_ap_record_t *g_ap_list = NULL;
static uint16_t g_ap_count = 0;

/**
 * @brief WiFi 事件处理
 */
static void wifi_config_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_CONNECTED: {
                wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*) event_data;
                g_connected = true;
                ESP_LOGI(TAG, "WiFi 已连接: BSSID=%02x:%02x:%02x:%02x:%02x:%02x",
                         event->bssid[0], event->bssid[1], event->bssid[2],
                         event->bssid[3], event->bssid[4], event->bssid[5]);
                break;
            }
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                g_connected = false;
                ESP_LOGI(TAG, "WiFi 已断开, 原因: %d", event->reason);
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
                ESP_LOGI(TAG, "STA 获得 IP: %s", ip_str);
                break;
            }
            case IP_EVENT_STA_LOST_IP:
                ESP_LOGI(TAG, "STA 失去 IP");
                break;
            default:
                break;
        }
    }
}

/**
 * @brief WiFi 配置初始化
 */
esp_err_t wifi_config_init(void)
{
    esp_err_t ret;

    // 初始化 NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 加载保存的配置
    ret = wifi_config_load(&g_wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "未找到保存的 WiFi 配置，使用默认值");
        strncpy(g_wifi_config.ssid, "xiangjaizhegebu", WIFI_SSID_MAX_LEN);
        strncpy(g_wifi_config.password, "bjbjbjbj", WIFI_PASSWORD_MAX_LEN);
        g_wifi_config.auto_connect = true;
    } else {
        ESP_LOGI(TAG, "已加载 WiFi 配置: %s", g_wifi_config.ssid);
    }

    // 注册事件处理器（必须先注册，才能接收连接事件）
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_config_event_handler, NULL));

    // 注册 IP 事件（获取 IP 地址后触发）
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_config_event_handler, NULL));

    // 如果开启了自动连接，立即连接
    if (g_wifi_config.auto_connect && strlen(g_wifi_config.ssid) > 0) {
        ESP_LOGI(TAG, "正在自动连接 WiFi: %s", g_wifi_config.ssid);
        wifi_config_connect(g_wifi_config.ssid, g_wifi_config.password);
    }

    return ESP_OK;
}

/**
 * @brief 保存 WiFi STA 配置到 NVS
 */
esp_err_t wifi_config_save(const wifi_user_config_t *config)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;

    ret = nvs_open(WIFI_CONFIG_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        WIFI_LOGE(TAG, "打开 NVS 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_str(nvs_handle, "ssid", config->ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "保存 SSID 失败");
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_set_str(nvs_handle, "password", config->password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "保存密码失败");
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_set_u8(nvs_handle, "auto_connect", config->auto_connect ? 1 : 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "保存自动连接设置失败");
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (ret == ESP_OK) {
        // 更新全局配置
        memcpy(&g_wifi_config, config, sizeof(wifi_user_config_t));
        ESP_LOGI(TAG, "WiFi 配置已保存");
    }

    return ret;
}

/**
 * @brief 从 NVS 加载 WiFi STA 配置
 */
esp_err_t wifi_config_load(wifi_user_config_t *config)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    size_t len;

    ret = nvs_open(WIFI_CONFIG_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    // 读取 SSID
    len = WIFI_SSID_MAX_LEN + 1;
    ret = nvs_get_str(nvs_handle, "ssid", config->ssid, &len);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }

    // 读取密码
    len = WIFI_PASSWORD_MAX_LEN + 1;
    ret = nvs_get_str(nvs_handle, "password", config->password, &len);
    if (ret != ESP_OK) {
        strcpy(config->password, "");
    }

    // 读取自动连接设置
    uint8_t auto_conn = 0;
    ret = nvs_get_u8(nvs_handle, "auto_connect", &auto_conn);
    config->auto_connect = (auto_conn == 1);

    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "从 NVS 加载 WiFi 配置成功");
    return ESP_OK;
}

/**
 * @brief 获取当前 WiFi STA 配置
 */
const wifi_user_config_t* wifi_config_get_current(void)
{
    return &g_wifi_config;
}

/**
 * @brief 连接到指定 WiFi
 */
esp_err_t wifi_config_connect(const char *ssid, const char *password)
{
    wifi_config_t wifi_config = {0};

    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password && strlen(password) > 0) {
        strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.threshold.rssi = -127;
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

    ESP_LOGI(TAG, "配置 WiFi: %s", ssid);

    // 先断开现有连接
    esp_wifi_disconnect();

    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置 WiFi 配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "连接 WiFi 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "正在连接到: %s", ssid);
    return ESP_OK;
}

/**
 * @brief 断开 WiFi 连接
 */
esp_err_t wifi_config_disconnect(void)
{
    esp_err_t ret = esp_wifi_disconnect();
    if (ret == ESP_OK) {
        g_connected = false;
    }
    return ret;
}

/**
 * @brief 扫描可用 WiFi 网络
 */
esp_err_t wifi_config_scan(wifi_ap_record_t **ap_list, uint16_t *ap_count)
{
    // 释放之前的扫描结果
    wifi_config_scan_free();

    ESP_LOGI(TAG, "开始扫描 WiFi 网络...");

    wifi_scan_config_t scan_config = {0};

    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi 扫描失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_scan_get_ap_num(ap_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取 AP 数量失败");
        return ret;
    }

    if (*ap_count == 0) {
        ESP_LOGI(TAG, "未找到 WiFi 网络");
        return ESP_OK;
    }

    g_ap_list = malloc(sizeof(wifi_ap_record_t) * (*ap_count));
    if (g_ap_list == NULL) {
        ESP_LOGE(TAG, "分配内存失败");
        *ap_count = 0;
        return ESP_ERR_NO_MEM;
    }

    ret = esp_wifi_scan_get_ap_records(ap_count, g_ap_list);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取 AP 列表失败");
        free(g_ap_list);
        g_ap_list = NULL;
        *ap_count = 0;
        return ret;
    }

    ESP_LOGI(TAG, "扫描完成，找到 %d 个网络", *ap_count);
    *ap_list = g_ap_list;

    return ESP_OK;
}

/**
 * @brief 释放扫描结果内存
 */
void wifi_config_scan_free(void)
{
    if (g_ap_list) {
        free(g_ap_list);
        g_ap_list = NULL;
        g_ap_count = 0;
    }
}

/**
 * @brief 获取 WiFi 连接状态
 */
bool wifi_config_is_connected(void)
{
    return g_connected;
}
