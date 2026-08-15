/**
 * @file wifi_web.c
 * @brief Wi-Fi Web API 实现 - 支持 AP/STA/混合模式
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

/* ========================================
 * JSON 响应构建辅助函数
 * ======================================== */

/**
 * @brief 创建错误响应
 */
static cJSON* create_error_json(const char *message)
{
    cJSON *data = cJSON_CreateObject();
    cJSON_AddBoolToObject(data, "success", false);
    cJSON_AddStringToObject(data, "error", message);
    return data;
}

/* ========================================
 * API: 获取 WiFi 状态 (STA)
 * GET /api/wifi/status
 * ======================================== */
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

    /* STA 状态 */
    cJSON_AddBoolToObject(data, "connected", wifi_config_is_sta_connected());
    cJSON_AddNumberToObject(data, "rssi", wifi_get_sta_rssi());
    cJSON_AddStringToObject(data, "ip", sta_ip);
    cJSON_AddStringToObject(data, "sta_ip", sta_ip);
    cJSON_AddStringToObject(data, "sta_bssid", sta_bssid);

    /* 当前配置 */
    const app_wifi_sta_config_t *sta_cfg = wifi_config_get_sta();
    if (sta_cfg) {
        cJSON_AddStringToObject(data, "ssid", sta_cfg->ssid);
        cJSON_AddBoolToObject(data, "auto_connect", sta_cfg->auto_connect);
    } else {
        cJSON_AddStringToObject(data, "ssid", "");
        cJSON_AddBoolToObject(data, "auto_connect", false);
    }

    return send_success(req, data, "获取WiFi状态成功");
}

/* ========================================
 * API: 获取 WiFi 配置
 * GET /api/wifi/config
 * ======================================== */
static esp_err_t wifi_web_config_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    const app_wifi_config_t *cfg = wifi_config_get_full();

    cJSON *data = cJSON_CreateObject();

    /* 当前模式 */
    if (cfg) {
        const char *mode_str;
        switch (cfg->mode) {
            case APP_WIFI_MODE_STA:  mode_str = "sta"; break;
            case APP_WIFI_MODE_AP:   mode_str = "ap"; break;
            case APP_WIFI_MODE_AP_STA: mode_str = "ap-sta"; break;
            default:             mode_str = "sta"; break;
        }
        cJSON_AddStringToObject(data, "mode", mode_str);

        /* STA 配置 */
        cJSON *sta = cJSON_CreateObject();
        cJSON_AddStringToObject(sta, "ssid", cfg->sta.ssid);
        cJSON_AddStringToObject(sta, "password", cfg->sta.password);
        cJSON_AddBoolToObject(sta, "auto_connect", cfg->sta.auto_connect);
        cJSON_AddItemToObject(data, "sta", sta);

        /* AP 配置 */
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", cfg->ap.ssid);
        cJSON_AddStringToObject(ap, "password", cfg->ap.password);
        cJSON_AddNumberToObject(ap, "channel", cfg->ap.channel);
        cJSON_AddItemToObject(data, "ap", ap);
    } else {
        cJSON_AddStringToObject(data, "mode", "sta");
        cJSON_AddItemToObject(data, "sta", cJSON_CreateObject());
        cJSON_AddItemToObject(data, "ap", cJSON_CreateObject());
    }

    return send_success(req, data, "获取WiFi配置成功");
}

/* ========================================
 * API: 设置 WiFi 模式
 * POST /api/wifi/mode
 * ======================================== */
static esp_err_t wifi_web_mode_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    /* 解析请求体 */
    cJSON *root = parse_request_json(req);
    if (root == NULL) {
        cJSON *data = create_error_json("无效的JSON格式");
        return send_success(req, data, "解析失败");
    }

    cJSON *mode_json = cJSON_GetObjectItem(root, "mode");
    if (!cJSON_IsString(mode_json)) {
        cJSON_Delete(root);
        cJSON *data = create_error_json("缺少mode参数");
        return send_success(req, data, "参数错误");
    }

    const char *mode_str = mode_json->valuestring;
    app_wifi_mode_t mode;

    if (strcmp(mode_str, "sta") == 0) {
        mode = APP_WIFI_MODE_STA;
    } else if (strcmp(mode_str, "ap") == 0) {
        mode = APP_WIFI_MODE_AP;
    } else if (strcmp(mode_str, "ap-sta") == 0) {
        mode = APP_WIFI_MODE_AP_STA;
    } else {
        cJSON_Delete(root);
        cJSON *data = create_error_json("无效的mode值");
        return send_success(req, data, "参数错误");
    }

    cJSON_Delete(root);

    /* 切换模式 */
    esp_err_t ret = wifi_config_set_mode(mode);
    cJSON *data = cJSON_CreateObject();

    if (ret == ESP_OK) {
        cJSON_AddBoolToObject(data, "success", true);
        cJSON_AddStringToObject(data, "mode", mode_str);
        return send_success(req, data, "模式切换成功");
    } else {
        cJSON_AddBoolToObject(data, "success", false);
        cJSON_AddStringToObject(data, "error", "模式切换失败");
        return send_success(req, data, "模式切换失败");
    }
}

/* ========================================
 * API: STA 连接
 * POST /api/wifi/sta/connect
 * ======================================== */
static esp_err_t wifi_web_sta_connect_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    /* 解析请求体 */
    cJSON *root = parse_request_json(req);
    if (root == NULL) {
        cJSON *data = create_error_json("无效的JSON格式");
        return send_success(req, data, "解析失败");
    }

    char ssid[64] = {0};
    char password[64] = {0};
    bool auto_connect = true;

    cJSON *ssid_json = cJSON_GetObjectItem(root, "ssid");
    if (cJSON_IsString(ssid_json) && ssid_json->valuestring) {
        strncpy(ssid, ssid_json->valuestring, sizeof(ssid) - 1);
    }

    cJSON *pwd_json = cJSON_GetObjectItem(root, "password");
    if (cJSON_IsString(pwd_json) && pwd_json->valuestring) {
        strncpy(password, pwd_json->valuestring, sizeof(password) - 1);
    }

    cJSON *auto_json = cJSON_GetObjectItem(root, "auto_connect");
    if (cJSON_IsBool(auto_json)) {
        auto_connect = cJSON_IsTrue(auto_json);
    }

    cJSON_Delete(root);

    if (strlen(ssid) == 0) {
        cJSON *data = create_error_json("SSID不能为空");
        return send_success(req, data, "参数错误");
    }

    /* 保存 STA 配置 */
    app_wifi_sta_config_t sta_config = {0};
    strncpy(sta_config.ssid, ssid, sizeof(sta_config.ssid) - 1);
    strncpy(sta_config.password, password, sizeof(sta_config.password) - 1);
    sta_config.auto_connect = auto_connect;

    esp_err_t ret = wifi_config_save_sta(&sta_config);
    cJSON *data = cJSON_CreateObject();

    if (ret != ESP_OK) {
        cJSON_AddBoolToObject(data, "success", false);
        cJSON_AddStringToObject(data, "error", "保存配置失败");
        return send_success(req, data, "保存失败");
    }

    /* 连接 */
    ret = wifi_config_connect(ssid, password);
    if (ret == ESP_OK) {
        cJSON_AddBoolToObject(data, "success", true);
        return send_success(req, data, "正在连接...");
    } else {
        cJSON_AddBoolToObject(data, "success", false);
        cJSON_AddStringToObject(data, "error", "连接失败");
        return send_success(req, data, "连接失败");
    }
}

/* ========================================
 * API: STA 断开
 * POST /api/wifi/sta/disconnect
 * ======================================== */
static esp_err_t wifi_web_sta_disconnect_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    esp_err_t ret = wifi_config_disconnect();
    cJSON *data = cJSON_CreateObject();

    if (ret == ESP_OK) {
        cJSON_AddBoolToObject(data, "success", true);
        return send_success(req, data, "已断开连接");
    } else {
        cJSON_AddBoolToObject(data, "success", false);
        cJSON_AddStringToObject(data, "error", "断开连接失败");
        return send_success(req, data, "断开失败");
    }
}

/* ========================================
 * API: AP 启动
 * POST /api/wifi/ap/start
 * ======================================== */
static esp_err_t wifi_web_ap_start_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    /* 解析请求体 */
    cJSON *root = parse_request_json(req);
    if (root == NULL) {
        cJSON *data = create_error_json("无效的JSON格式");
        return send_success(req, data, "解析失败");
    }

    char ssid[64] = {0};
    char password[64] = {0};
    uint8_t channel = 1;

    cJSON *ssid_json = cJSON_GetObjectItem(root, "ssid");
    if (cJSON_IsString(ssid_json) && ssid_json->valuestring) {
        strncpy(ssid, ssid_json->valuestring, sizeof(ssid) - 1);
    }

    cJSON *pwd_json = cJSON_GetObjectItem(root, "password");
    if (cJSON_IsString(pwd_json) && pwd_json->valuestring) {
        strncpy(password, pwd_json->valuestring, sizeof(password) - 1);
    }

    cJSON *ch_json = cJSON_GetObjectItem(root, "channel");
    if (cJSON_IsNumber(ch_json)) {
        channel = (uint8_t)ch_json->valueint;
    }

    cJSON_Delete(root);

    /* 保存 AP 配置 */
    app_wifi_ap_config_t ap_config = {0};
    strncpy(ap_config.ssid, ssid, sizeof(ap_config.ssid) - 1);
    strncpy(ap_config.password, password, sizeof(ap_config.password) - 1);
    ap_config.channel = channel;

    esp_err_t ret = wifi_config_save_ap(&ap_config);
    cJSON *data = cJSON_CreateObject();

    if (ret != ESP_OK) {
        cJSON_AddBoolToObject(data, "success", false);
        cJSON_AddStringToObject(data, "error", "保存配置失败");
        return send_success(req, data, "保存失败");
    }

    /* 启动 AP */
    ret = wifi_config_start_ap(ssid, password, channel);
    if (ret == ESP_OK) {
        cJSON_AddBoolToObject(data, "success", true);
        cJSON_AddStringToObject(data, "ssid", ssid);
        return send_success(req, data, "热点已启动");
    } else {
        cJSON_AddBoolToObject(data, "success", false);
        cJSON_AddStringToObject(data, "error", "启动热点失败");
        return send_success(req, data, "启动失败");
    }
}

/* ========================================
 * API: AP 停止
 * POST /api/wifi/ap/stop
 * ======================================== */
static esp_err_t wifi_web_ap_stop_handler(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_bad_request(req, "仅支持 POST 请求");
    }

    esp_err_t ret = wifi_config_stop_ap();
    cJSON *data = cJSON_CreateObject();

    if (ret == ESP_OK) {
        cJSON_AddBoolToObject(data, "success", true);
        return send_success(req, data, "热点已关闭");
    } else {
        cJSON_AddBoolToObject(data, "success", false);
        cJSON_AddStringToObject(data, "error", "关闭热点失败");
        return send_success(req, data, "关闭失败");
    }
}

/* ========================================
 * API: AP 状态
 * GET /api/wifi/ap/status
 * ======================================== */
static esp_err_t wifi_web_ap_status_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    cJSON *data = cJSON_CreateObject();

    /* AP 运行状态 */
    cJSON_AddBoolToObject(data, "running", wifi_config_is_ap_running());
    cJSON_AddNumberToObject(data, "connected_clients", wifi_config_get_ap_clients());

    /* AP 配置 */
    const app_wifi_ap_config_t *ap_cfg = wifi_config_get_ap();
    if (ap_cfg) {
        cJSON_AddStringToObject(data, "ssid", ap_cfg->ssid);
        cJSON_AddNumberToObject(data, "channel", ap_cfg->channel);
    } else {
        cJSON_AddStringToObject(data, "ssid", "");
        cJSON_AddNumberToObject(data, "channel", 1);
    }

    /* AP IP */
    char ap_ip[16] = "192.168.4.1";
    wifi_get_ap_ip_string(ap_ip, sizeof(ap_ip));
    cJSON_AddStringToObject(data, "ip", ap_ip);

    return send_success(req, data, "获取AP状态成功");
}

/* ========================================
 * API: 扫描 WiFi 网络
 * GET /api/wifi/scan
 * ======================================== */
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

    return send_success(req, data, "扫描成功");
}

/* ========================================
 * 注册路由
 * ======================================== */
void wifi_web_register_routes(httpd_handle_t server)
{
    httpd_uri_t routes[] = {
        /* STA 相关 */
        {.uri = "/api/wifi/status",           .method = HTTP_GET,  .handler = wifi_web_status_handler},
        {.uri = "/api/wifi/config",           .method = HTTP_GET,  .handler = wifi_web_config_handler},
        {.uri = "/api/wifi/mode",             .method = HTTP_POST, .handler = wifi_web_mode_handler},
        {.uri = "/api/wifi/sta/connect",      .method = HTTP_POST, .handler = wifi_web_sta_connect_handler},
        {.uri = "/api/wifi/sta/disconnect",   .method = HTTP_POST, .handler = wifi_web_sta_disconnect_handler},

        /* AP 相关 */
        {.uri = "/api/wifi/ap/start",         .method = HTTP_POST, .handler = wifi_web_ap_start_handler},
        {.uri = "/api/wifi/ap/stop",          .method = HTTP_POST, .handler = wifi_web_ap_stop_handler},
        {.uri = "/api/wifi/ap/status",        .method = HTTP_GET,  .handler = wifi_web_ap_status_handler},

        /* 扫描 */
        {.uri = "/api/wifi/scan",             .method = HTTP_GET,  .handler = wifi_web_scan_handler},
    };

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        esp_err_t ret = httpd_register_uri_handler(server, &routes[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "注册路由失败: %s", routes[i].uri);
        }
    }

    ESP_LOGI(TAG, "WiFi Web API 已注册 %d 个路由", (int)(sizeof(routes) / sizeof(routes[0])));
}
