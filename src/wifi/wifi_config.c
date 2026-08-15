/**
 * @file wifi_config.c
 * @brief WiFi 配置管理模块实现 - NVS持久化存储 + AP/STA配置管理
 */
#include "wifi_config.h"
#include "wifi.h"
#include "../config.h"
#include "config/hw_wifi.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_event.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "WIFI_CONFIG";

/* 全局配置状态 */
static app_wifi_config_t g_wifi_config = {0};
static app_wifi_mode_t g_current_mode = APP_WIFI_MODE_AP_STA;  /* 默认混合模式 */
static bool g_sta_connected = false;
static bool g_ap_running = false;
static uint8_t g_ap_connected_clients = 0;
static wifi_ap_record_t *g_ap_list = NULL;
static uint16_t g_ap_count = 0;

/* 默认 AP 配置 */
static const char* DEFAULT_AP_SSID = WIFI_AP_SSID;
static const char* DEFAULT_AP_PASSWORD = WIFI_AP_PASSWORD;
static const uint8_t DEFAULT_AP_CHANNEL = WIFI_AP_CHANNEL;

/* ========================================
 * NVS 操作辅助函数
 * ======================================== */

/**
 * @brief 打开 NVS 句柄
 */
static esp_err_t nvs_open_handle(nvs_handle_t *handle, nvs_open_mode_t mode)
{
    return nvs_open(WIFI_CONFIG_NAMESPACE, mode, handle);
}

/**
 * @brief 保存字符串到 NVS
 */
static esp_err_t nvs_save_string(nvs_handle_t handle, const char *key, const char *value)
{
    if (value == NULL || strlen(value) == 0) {
        return nvs_set_str(handle, key, "");
    }
    return nvs_set_str(handle, key, value);
}

/* ========================================
 * STA 配置操作
 * ======================================== */

/**
 * @brief 保存 WiFi STA 配置到 NVS
 */
esp_err_t wifi_config_save_sta(const app_wifi_sta_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open_handle(&handle, NVS_READWRITE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "打开 NVS 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_save_string(handle, "sta_ssid", config->ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "保存 STA SSID 失败");
        nvs_close(handle);
        return ret;
    }

    ret = nvs_save_string(handle, "sta_password", config->password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "保存 STA 密码失败");
        nvs_close(handle);
        return ret;
    }

    ret = nvs_set_u8(handle, "sta_auto", config->auto_connect ? 1 : 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "保存 STA 自动连接失败");
        nvs_close(handle);
        return ret;
    }

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret == ESP_OK) {
        memcpy(&g_wifi_config.sta, config, sizeof(app_wifi_sta_config_t));
        ESP_LOGI(TAG, "STA 配置已保存");
    }

    return ret;
}

/**
 * @brief 从 NVS 加载 WiFi STA 配置
 */
esp_err_t wifi_config_load_sta(app_wifi_sta_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open_handle(&handle, NVS_READONLY);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t len = WIFI_SSID_MAX_LEN + 1;
    ret = nvs_get_str(handle, "sta_ssid", config->ssid, &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        config->ssid[0] = '\0';
    }

    len = WIFI_PASSWORD_MAX_LEN + 1;
    ret = nvs_get_str(handle, "sta_password", config->password, &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        config->password[0] = '\0';
    }

    uint8_t auto_conn = 1;
    ret = nvs_get_u8(handle, "sta_auto", &auto_conn);
    config->auto_connect = (auto_conn == 1);

    nvs_close(handle);
    return ESP_OK;
}

/**
 * @brief 获取当前 WiFi STA 配置
 */
const app_wifi_sta_config_t* wifi_config_get_sta(void)
{
    return &g_wifi_config.sta;
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

    ESP_LOGI(TAG, "配置 STA: %s", ssid);

    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置 STA 配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "连接 STA 失败: %s", esp_err_to_name(ret));
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
        g_sta_connected = false;
    }
    return ret;
}

/**
 * @brief 获取 STA 连接状态
 */
bool wifi_config_is_sta_connected(void)
{
    return g_sta_connected;
}

/* ========================================
 * AP 配置操作
 * ======================================== */

/**
 * @brief 保存 WiFi AP 配置到 NVS
 */
esp_err_t wifi_config_save_ap(const app_wifi_ap_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open_handle(&handle, NVS_READWRITE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "打开 NVS 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_save_string(handle, "ap_ssid", config->ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "保存 AP SSID 失败");
        nvs_close(handle);
        return ret;
    }

    ret = nvs_save_string(handle, "ap_password", config->password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "保存 AP 密码失败");
        nvs_close(handle);
        return ret;
    }

    ret = nvs_set_u8(handle, "ap_channel", config->channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "保存 AP 频道失败");
        nvs_close(handle);
        return ret;
    }

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret == ESP_OK) {
        memcpy(&g_wifi_config.ap, config, sizeof(app_wifi_ap_config_t));
        ESP_LOGI(TAG, "AP 配置已保存");
    }

    return ret;
}

/**
 * @brief 从 NVS 加载 WiFi AP 配置
 */
esp_err_t wifi_config_load_ap(app_wifi_ap_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open_handle(&handle, NVS_READONLY);
    if (ret != ESP_OK) {
        return ret;
    }

    size_t len = WIFI_SSID_MAX_LEN + 1;
    ret = nvs_get_str(handle, "ap_ssid", config->ssid, &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        strncpy(config->ssid, DEFAULT_AP_SSID, WIFI_SSID_MAX_LEN);
    }

    len = WIFI_PASSWORD_MAX_LEN + 1;
    ret = nvs_get_str(handle, "ap_password", config->password, &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        strncpy(config->password, DEFAULT_AP_PASSWORD, WIFI_PASSWORD_MAX_LEN);
    }

    ret = nvs_get_u8(handle, "ap_channel", &config->channel);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        config->channel = DEFAULT_AP_CHANNEL;
    }

    nvs_close(handle);
    return ESP_OK;
}

/**
 * @brief 获取当前 WiFi AP 配置
 */
const app_wifi_ap_config_t* wifi_config_get_ap(void)
{
    return &g_wifi_config.ap;
}

/**
 * @brief 启动 AP 热点
 */
esp_err_t wifi_config_start_ap(const char *ssid, const char *password, uint8_t channel)
{
    wifi_config_t ap_config = {0};

    /* 设置 SSID */
    if (ssid && strlen(ssid) > 0) {
        strncpy((char*)ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid) - 1);
        ap_config.ap.ssid_len = strlen(ssid);
    } else {
        strncpy((char*)ap_config.ap.ssid, DEFAULT_AP_SSID, sizeof(ap_config.ap.ssid) - 1);
        ap_config.ap.ssid_len = strlen(DEFAULT_AP_SSID);
    }

    /* 设置密码和加密模式 */
    if (password && strlen(password) >= 8) {
        strncpy((char*)ap_config.ap.password, password, sizeof(ap_config.ap.password) - 1);
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        ap_config.ap.password[0] = '\0';
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
        ESP_LOGW(TAG, "AP 密码为空或少于8位，将使用开放网络");
    }

    /* 设置频道 */
    ap_config.ap.channel = (channel > 0 && channel <= 13) ? channel : DEFAULT_AP_CHANNEL;
    ap_config.ap.max_connection = WIFI_AP_MAX_CONNECTIONS;
    ap_config.ap.pmf_cfg.required = false;

    ESP_LOGI(TAG, "配置 AP: ssid=%s, channel=%d", ap_config.ap.ssid, ap_config.ap.channel);

    esp_err_t ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置 AP 配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 确保 AP 启用 */
    ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置 AP+STA 模式失败: %s", esp_err_to_name(ret));
        return ret;
    }

    g_ap_running = true;
    ESP_LOGI(TAG, "AP 热点已启动: %s", ap_config.ap.ssid);
    return ESP_OK;
}

/**
 * @brief 停止 AP 热点
 */
esp_err_t wifi_config_stop_ap(void)
{
    /* 关闭 AP - 设置空配置 */
    wifi_config_t ap_config = {0};
    ap_config.ap.authmode = WIFI_AUTH_OPEN;

    esp_err_t ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "停止 AP 失败: %s", esp_err_to_name(ret));
        return ret;
    }

    g_ap_running = false;
    g_ap_connected_clients = 0;
    ESP_LOGI(TAG, "AP 热点已停止");
    return ESP_OK;
}

/**
 * @brief 获取 AP 运行状态
 */
bool wifi_config_is_ap_running(void)
{
    return g_ap_running;
}

/**
 * @brief 获取 AP 连接的客户端数量
 */
int wifi_config_get_ap_clients(void)
{
    return g_ap_connected_clients;
}

/* ========================================
 * 模式切换操作
 * ======================================== */

/**
 * @brief 设置 WiFi 运行模式
 */
esp_err_t wifi_config_set_mode(app_wifi_mode_t mode)
{
    wifi_mode_t esp_mode;

    switch (mode) {
        case APP_WIFI_MODE_STA:
            esp_mode = WIFI_MODE_STA;
            break;
        case APP_WIFI_MODE_AP:
            esp_mode = WIFI_MODE_AP;
            break;
        case APP_WIFI_MODE_AP_STA:
        default:
            esp_mode = WIFI_MODE_APSTA;
            break;
    }

    esp_err_t ret = esp_wifi_set_mode(esp_mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "设置 WiFi 模式失败: %s", esp_err_to_name(ret));
        return ret;
    }

    g_current_mode = mode;

    /* 根据模式启动/停止对应服务 */
    if (mode == APP_WIFI_MODE_AP || mode == APP_WIFI_MODE_AP_STA) {
        /* 启动 AP */
        wifi_config_start_ap(g_wifi_config.ap.ssid, g_wifi_config.ap.password, g_wifi_config.ap.channel);
    } else {
        /* 停止 AP */
        wifi_config_stop_ap();
    }

    if (mode == APP_WIFI_MODE_STA || mode == APP_WIFI_MODE_AP_STA) {
        /* 启动 STA 连接 */
        if (g_wifi_config.sta.auto_connect && strlen(g_wifi_config.sta.ssid) > 0) {
            wifi_config_connect(g_wifi_config.sta.ssid, g_wifi_config.sta.password);
        }
    } else {
        /* 断开 STA */
        wifi_config_disconnect();
    }

    /* 保存模式到 NVS */
    nvs_handle_t handle;
    if (nvs_open_handle(&handle, NVS_READWRITE) == ESP_OK) {
        nvs_set_u8(handle, "wifi_mode", (uint8_t)mode);
        nvs_commit(handle);
        nvs_close(handle);
    }

    ESP_LOGI(TAG, "WiFi 模式已切换为: %d", mode);
    return ESP_OK;
}

/**
 * @brief 获取当前 WiFi 运行模式
 */
app_wifi_mode_t wifi_config_get_mode(void)
{
    return g_current_mode;
}

/* ========================================
 * 完整配置操作
 * ======================================== */

/**
 * @brief 保存完整配置到 NVS
 */
esp_err_t wifi_config_save_full(const app_wifi_config_t *config)
{
    /* 保存各部分配置 */
    esp_err_t ret = wifi_config_save_sta(&config->sta);
    if (ret != ESP_OK) return ret;

    ret = wifi_config_save_ap(&config->ap);
    if (ret != ESP_OK) return ret;

    /* 保存模式 */
    nvs_handle_t handle;
    ret = nvs_open_handle(&handle, NVS_READWRITE);
    if (ret != ESP_OK) return ret;

    ret = nvs_set_u8(handle, "wifi_mode", (uint8_t)config->mode);
    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret == ESP_OK) {
        memcpy(&g_wifi_config, config, sizeof(app_wifi_config_t));
    }

    return ret;
}

/**
 * @brief 从 NVS 加载完整配置
 */
esp_err_t wifi_config_load_full(app_wifi_config_t *config)
{
    /* 加载 STA 配置 */
    wifi_config_load_sta(&config->sta);

    /* 加载 AP 配置 */
    wifi_config_load_ap(&config->ap);

    /* 加载模式 */
    nvs_handle_t handle;
    if (nvs_open_handle(&handle, NVS_READONLY) == ESP_OK) {
        uint8_t mode = APP_WIFI_MODE_AP_STA;  /* 默认混合模式 */
        nvs_get_u8(handle, "wifi_mode", &mode);
        config->mode = (app_wifi_mode_t)mode;
        nvs_close(handle);
    } else {
        config->mode = APP_WIFI_MODE_AP_STA;
    }

    return ESP_OK;
}

/**
 * @brief 获取完整配置
 */
const app_wifi_config_t* wifi_config_get_full(void)
{
    return &g_wifi_config;
}

/* ========================================
 * 扫描操作
 * ======================================== */

/**
 * @brief 扫描可用 WiFi 网络
 */
esp_err_t wifi_config_scan(wifi_ap_record_t **ap_list, uint16_t *ap_count)
{
    wifi_config_scan_free();

    ESP_LOGI(TAG, "开始扫描 WiFi 网络...");

    wifi_scan_config_t scan_config = {0};
    scan_config.show_hidden = false;

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

/* ========================================
 * 初始化
 * ======================================== */

/**
 * @brief WiFi 配置初始化 - 默认混合模式
 */
esp_err_t wifi_config_init(void)
{
    esp_err_t ret;

    /* 初始化 NVS */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS 初始化失败: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 设置默认值 */
    g_wifi_config.mode = APP_WIFI_MODE_AP_STA;  /* 默认混合模式 */
    strncpy(g_wifi_config.ap.ssid, DEFAULT_AP_SSID, WIFI_SSID_MAX_LEN);
    strncpy(g_wifi_config.ap.password, DEFAULT_AP_PASSWORD, WIFI_PASSWORD_MAX_LEN);
    g_wifi_config.ap.channel = DEFAULT_AP_CHANNEL;
    g_wifi_config.sta.auto_connect = true;
    
    /* 设置默认 STA 配置（编译时默认值） */
    strncpy(g_wifi_config.sta.ssid, WIFI_STA_SSID, WIFI_SSID_MAX_LEN);
    strncpy(g_wifi_config.sta.password, WIFI_STA_PASSWORD, WIFI_PASSWORD_MAX_LEN);

    /* 从 NVS 加载配置 */
    ret = wifi_config_load_full(&g_wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "使用默认配置 (混合模式)");
    } else {
        ESP_LOGI(TAG, "已加载配置: mode=%d, STA=%s, AP=%s",
                 g_wifi_config.mode, g_wifi_config.sta.ssid, g_wifi_config.ap.ssid);
    }
    
    /* 如果 NVS 中 STA SSID 为空，使用默认配置 */
    if (strlen(g_wifi_config.sta.ssid) == 0) {
        strncpy(g_wifi_config.sta.ssid, WIFI_STA_SSID, WIFI_SSID_MAX_LEN);
        strncpy(g_wifi_config.sta.password, WIFI_STA_PASSWORD, WIFI_PASSWORD_MAX_LEN);
        ESP_LOGI(TAG, "使用编译时默认 STA 配置: %s", g_wifi_config.sta.ssid);
    }

    g_current_mode = g_wifi_config.mode;
    g_ap_running = true;  /* 默认开启 AP */

    return ESP_OK;
}

/**
 * @brief WiFi 配置反初始化
 */
esp_err_t wifi_config_deinit(void)
{
    wifi_config_scan_free();
    return ESP_OK;
}

/* ========================================
 * 内部事件处理（供 wifi.c 调用）
 * ======================================== */

/**
 * @brief 更新 STA 连接状态（由 wifi_event_handler 调用）
 */
void wifi_config_update_sta_connected(bool connected)
{
    g_sta_connected = connected;
}

/**
 * @brief 更新 AP 客户端数量（由 wifi_event_handler 调用）
 */
void wifi_config_update_ap_clients(uint8_t count)
{
    g_ap_connected_clients = count;
}

/**
 * @brief 获取 AP 客户端数量（内部）
 */
uint8_t wifi_config_get_ap_clients_internal(void)
{
    return g_ap_connected_clients;
}
