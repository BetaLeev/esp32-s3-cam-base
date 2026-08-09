/**
 * @file wifi_web.c
 * @brief Wi-Fi Web API 实现
 */
#include "wifi_web.h"
#include "wifi_config.h"
#include "wifi.h"
#include "../web_module.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WIFI_WEB";

/**
 * @brief 从 URL 参数中提取值
 */
static bool get_query_param(httpd_req_t *req, const char *key, char *value, size_t max_len)
{
    char buf[256];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        if (httpd_query_key_value(buf, key, value, max_len) == ESP_OK) {
            return true;
        }
    }
    return false;
}

/**
 * @brief URL 解码
 */
static void url_decode(char *str)
{
    char *src = str, *dst = str;
    while (*src) {
        if (*src == '%' && *(src + 1) && *(src + 2)) {
            char hex[3] = {*(src + 1), *(src + 2), 0};
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/**
 * @brief API: 获取 WiFi 状态
 */
static esp_err_t wifi_web_status_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    char sta_ip[16] = "0.0.0.0";
    wifi_get_sta_ip_string(sta_ip, sizeof(sta_ip));

    char sta_bssid[18] = "";
    wifi_get_sta_bssid(sta_bssid, sizeof(sta_bssid));

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "connected", wifi_is_sta_connected());
    cJSON_AddNumberToObject(data, "rssi", wifi_get_sta_rssi());
    cJSON_AddStringToObject(data, "ip", sta_ip);
    cJSON_AddStringToObject(data, "sta_ip", sta_ip);
    cJSON_AddStringToObject(data, "sta_bssid", sta_bssid);

    const wifi_user_config_t *cfg = wifi_config_get_current();
    if (cfg) {
        cJSON_AddStringToObject(data, "ssid", cfg->ssid);
        cJSON_AddBoolToObject(data, "auto_connect", cfg->auto_connect);
    } else {
        cJSON_AddStringToObject(data, "ssid", "");
        cJSON_AddBoolToObject(data, "auto_connect", false);
    }

    return send_success(req, data, "获取WiFi状态成功");
}

/**
 * @brief API: 获取 WiFi 配置
 */
static esp_err_t wifi_web_config_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    const wifi_user_config_t *cfg = wifi_config_get_current();

    cJSON *data = cJSON_CreateObject();
    if (cfg) {
        cJSON_AddStringToObject(data, "ssid", cfg->ssid);
        cJSON_AddStringToObject(data, "password", cfg->password);
        cJSON_AddBoolToObject(data, "auto_connect", cfg->auto_connect);
    } else {
        cJSON_AddStringToObject(data, "ssid", "");
        cJSON_AddStringToObject(data, "password", "");
        cJSON_AddBoolToObject(data, "auto_connect", false);
    }

    return send_success(req, data, "获取WiFi配置成功");
}

/**
 * @brief API: 设置 WiFi 配置并连接
 */
static esp_err_t wifi_web_set_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    char ssid[64] = {0};
    char password[64] = {0};
    char auto_conn_str[8] = {0};

    get_query_param(req, "ssid", ssid, sizeof(ssid));
    get_query_param(req, "password", password, sizeof(password));
    get_query_param(req, "auto_connect", auto_conn_str, sizeof(auto_conn_str));

    url_decode(ssid);
    url_decode(password);

    ESP_LOGI(TAG, "设置 WiFi: ssid=%s, auto_connect=%s", ssid, auto_conn_str);

    wifi_user_config_t config = {0};
    strncpy(config.ssid, ssid, sizeof(config.ssid) - 1);
    strncpy(config.password, password, sizeof(config.password) - 1);
    config.auto_connect = (strcmp(auto_conn_str, "true") == 0 || strcmp(auto_conn_str, "1") == 0);

    cJSON *data = cJSON_CreateObject();
    esp_err_t ret = wifi_config_save(&config);
    if (ret != ESP_OK) {
        cJSON_AddBoolToObject(data, "success", false);
        cJSON_AddStringToObject(data, "error", "保存配置失败");
        return send_success(req, data, "保存配置失败");
    }

    // 连接到新 WiFi
    ret = wifi_config_connect(ssid, password);
    if (ret != ESP_OK) {
        cJSON_AddBoolToObject(data, "success", true);
        cJSON_AddBoolToObject(data, "connecting", false);
        cJSON_AddStringToObject(data, "message", "配置已保存，但连接失败");
    } else {
        cJSON_AddBoolToObject(data, "success", true);
        cJSON_AddBoolToObject(data, "connecting", true);
        cJSON_AddStringToObject(data, "message", "正在连接...");
    }

    return send_success(req, data, "设置WiFi成功");
}

/**
 * @brief API: 扫描 WiFi 网络
 */
static esp_err_t wifi_web_scan_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    wifi_ap_record_t *ap_list = NULL;
    uint16_t ap_count = 0;

    cJSON *data = cJSON_CreateObject();

    esp_err_t ret = wifi_config_scan(&ap_list, &ap_count);
    if (ret != ESP_OK) {
        cJSON_AddBoolToObject(data, "success", false);
        cJSON_AddStringToObject(data, "error", "扫描失败");
        return send_success(req, data, "扫描失败");
    }

    cJSON_AddBoolToObject(data, "success", true);
    cJSON *networks = cJSON_AddArrayToObject(data, "networks");

    for (int i = 0; i < ap_count; i++) {
        cJSON *net = cJSON_CreateObject();
        cJSON_AddStringToObject(net, "ssid", (char *)ap_list[i].ssid);
        cJSON_AddNumberToObject(net, "rssi", ap_list[i].rssi);
        cJSON_AddNumberToObject(net, "authmode", ap_list[i].authmode);
        cJSON_AddItemToArray(networks, net);
    }

    wifi_config_scan_free();
    return send_success(req, data, "扫描成功");
}

/**
 * @brief API: 断开 WiFi 连接
 */
static esp_err_t wifi_web_disconnect_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    esp_err_t ret = wifi_config_disconnect();

    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "success", (ret == ESP_OK));

    return send_success(req, data, ret == ESP_OK ? "断开成功" : "断开失败");
}

/**
 * @brief 注册 Wi-Fi 模块的 URI 路由
 */
void wifi_web_register_routes(httpd_handle_t server)
{
    httpd_uri_t routes[] = {
        {.uri = "/api/wifi/status",     .method = HTTP_GET, .handler = wifi_web_status_handler},
        {.uri = "/api/wifi/config",     .method = HTTP_GET, .handler = wifi_web_config_handler},
        {.uri = "/api/wifi/set",        .method = HTTP_GET, .handler = wifi_web_set_handler},
        {.uri = "/api/wifi/scan",       .method = HTTP_GET, .handler = wifi_web_scan_handler},
        {.uri = "/api/wifi/disconnect",  .method = HTTP_GET, .handler = wifi_web_disconnect_handler},
    };

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }
}
