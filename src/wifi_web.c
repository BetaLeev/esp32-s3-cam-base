/**
 * @file wifi_web.c
 * @brief WiFi Web API 处理模块实现
 */
#include "wifi_web.h"
#include "wifi_config.h"
#include "wifi_app.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "WIFI_WEB";

/**
 * @brief 从 URL 参数中提取值
 */
static bool get_query_param(httpd_req_t *req, const char *key, char *value, size_t max_len)
{
    char buf[256];
    size_t buf_len = req->content_len;

    if (buf_len >= sizeof(buf)) {
        buf_len = sizeof(buf) - 1;
    }

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
esp_err_t wifi_web_status_handler(httpd_req_t *req)
{
    char buffer[512];
    char sta_ip[16] = "0.0.0.0";

    wifi_app_get_sta_ip_string(sta_ip, sizeof(sta_ip));

    wifi_ap_record_t ap_info;
    int8_t rssi = 0;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        rssi = ap_info.rssi;
    }

    bool connected = wifi_config_is_connected();
    const wifi_user_config_t *cfg = wifi_config_get_current();

    snprintf(buffer, sizeof(buffer),
        "{"
        "\"connected\":%s,"
        "\"ssid\":\"%s\","
        "\"rssi\":%d,"
        "\"ip\":\"%s\","
        "\"auto_connect\":%s"
        "}",
        connected ? "true" : "false",
        cfg ? cfg->ssid : "",
        rssi,
        sta_ip,
        (cfg && cfg->auto_connect) ? "true" : "false"
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));

    return ESP_OK;
}

/**
 * @brief API: 获取 WiFi 配置
 */
esp_err_t wifi_web_config_handler(httpd_req_t *req)
{
    char buffer[256];
    const wifi_user_config_t *cfg = wifi_config_get_current();

    if (cfg) {
        snprintf(buffer, sizeof(buffer),
            "{"
            "\"ssid\":\"%s\","
            "\"password\":\"%s\","
            "\"auto_connect\":%s"
            "}",
            cfg->ssid,
            cfg->password,
            cfg->auto_connect ? "true" : "false"
        );
    } else {
        snprintf(buffer, sizeof(buffer), "{\"ssid\":\"\",\"password\":\"\",\"auto_connect\":false}");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));

    return ESP_OK;
}

/**
 * @brief API: 设置 WiFi 配置并连接
 */
esp_err_t wifi_web_set_handler(httpd_req_t *req)
{
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

    char response[256];
    esp_err_t ret = wifi_config_save(&config);
    if (ret != ESP_OK) {
        snprintf(response, sizeof(response),
            "{\"success\":false,\"error\":\"保存配置失败\"}");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }

    // 连接到新 WiFi
    ret = wifi_config_connect(ssid, password);
    if (ret != ESP_OK) {
        snprintf(response, sizeof(response),
            "{\"success\":true,\"connecting\":false,\"message\":\"配置已保存，但连接失败\"}");
    } else {
        snprintf(response, sizeof(response),
            "{\"success\":true,\"connecting\":true,\"message\":\"正在连接...\"}");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}

/**
 * @brief API: 扫描 WiFi 网络
 */
esp_err_t wifi_web_scan_handler(httpd_req_t *req)
{
    wifi_ap_record_t *ap_list = NULL;
    uint16_t ap_count = 0;

    esp_err_t ret = wifi_config_scan(&ap_list, &ap_count);
    if (ret != ESP_OK) {
        char response[128];
        snprintf(response, sizeof(response),
            "{\"success\":false,\"error\":\"扫描失败\"}");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }

    // 构建 JSON 响应
    char *response = malloc(8192);
    if (!response) {
        wifi_config_scan_free();
        return ESP_ERR_NO_MEM;
    }

    char *ptr = response;
    int remaining = 8192;
    int written;

    written = snprintf(ptr, remaining, "{\"success\":true,\"networks\":[");
    ptr += written;
    remaining -= written;

    for (int i = 0; i < ap_count && remaining > 0; i++) {
        if (i > 0) {
            written = snprintf(ptr, remaining, ",");
            ptr += written;
            remaining -= written;
        }

        written = snprintf(ptr, remaining,
            "{\"ssid\":\"%s\",\"rssi\":%d,\"authmode\":%d}",
            (char*)ap_list[i].ssid,
            ap_list[i].rssi,
            ap_list[i].authmode);
        ptr += written;
        remaining -= written;
    }

    written = snprintf(ptr, remaining, "]}");
    ptr += written;
    remaining -= written;

    wifi_config_scan_free();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    free(response);

    return ESP_OK;
}

/**
 * @brief API: 断开 WiFi 连接
 */
esp_err_t wifi_web_disconnect_handler(httpd_req_t *req)
{
    esp_err_t ret = wifi_config_disconnect();

    char response[128];
    snprintf(response, sizeof(response),
        "{\"success\":%s}", ret == ESP_OK ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));

    return ESP_OK;
}
