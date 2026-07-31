/**
 * @file http_server.c
 * @brief HTTP服务器模块实现 - API接口与路由
 * 
 * 前端静态文件通过 SPIFFS 文件系统提供
 * 文件位于 /web/ 目录，编译时打包到 Flash 分区
 * 支持 Captive Portal - 连接AP后自动跳转到控制页面
 */
#include "http_server.h"
#include "config.h"
#include "actuators/actuators.h"
#include "wifi_app.h"
#include "spiffs_web.h"
#include "sdcard/sdcard.h"
#include "sdcard/sdcard_web.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "sys/reent.h"
#include <string.h>

static const char *TAG = "HTTP_SERVER";

/* Web 目录前缀 */
#define WEB_BASE_PATH "web/"

/**
 * @brief 根据文件扩展名获取 MIME 类型
 */
static const char* get_mime_type(const char *file_path)
{
    const char *ext = strrchr(file_path, '.');
    if (ext == NULL) return "application/octet-stream";
    ext++;  // 跳过 '.'

    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
        return "text/html; charset=utf-8";
    } else if (strcasecmp(ext, "css") == 0) {
        return "text/css";
    } else if (strcasecmp(ext, "js") == 0) {
        return "application/javascript";
    } else if (strcasecmp(ext, "json") == 0) {
        return "application/json";
    } else if (strcasecmp(ext, "png") == 0) {
        return "image/png";
    } else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) {
        return "image/jpeg";
    } else if (strcasecmp(ext, "gif") == 0) {
        return "image/gif";
    } else if (strcasecmp(ext, "svg") == 0) {
        return "image/svg+xml";
    } else if (strcasecmp(ext, "ico") == 0) {
        return "image/x-icon";
    } else if (strcasecmp(ext, "woff") == 0) {
        return "font/woff";
    } else if (strcasecmp(ext, "woff2") == 0) {
        return "font/woff2";
    } else if (strcasecmp(ext, "txt") == 0) {
        return "text/plain";
    }

    return "application/octet-stream";
}

/**
 * @brief 通用静态文件处理函数
 */
static esp_err_t static_file_handler(httpd_req_t *req, const char *file_path)
{
    char *buffer = NULL;
    size_t len = 0;

    esp_err_t ret = spiffs_web_read_file(file_path, &buffer, &len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "文件未找到: %s", file_path);
        const char *error_page = "<html><body><h1>404 - 文件未找到</h1></body></html>";
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, error_page, strlen(error_page));
        return ESP_FAIL;
    }

    const char *mime_type = get_mime_type(file_path);
    httpd_resp_set_type(req, mime_type);
    httpd_resp_send(req, buffer, len);
    free(buffer);

    return ESP_OK;
}

/**
 * @brief Captive Portal 处理 - 捕获所有未知请求并重定向
 */
static esp_err_t captive_portal_handler(httpd_req_t *req)
{
    char *buffer = NULL;
    size_t len = 0;

    // 从SPIFFS读取 captive_portal.html
    esp_err_t ret = spiffs_web_read_file(WEB_BASE_PATH "captive_portal.html", &buffer, &len);
    if (ret != ESP_OK) {
        // 文件不存在，返回简单的重定向响应
        const char *fallback = "<html><head><meta charset='utf-8'>"
            "<script>window.location.replace('/');</script>"
            "</head><body><h1>跳转中...</h1></body></html>";
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        httpd_resp_send(req, fallback, strlen(fallback));
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Captive Portal 发送重定向页面");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, buffer, len);
    free(buffer);
    return ESP_OK;
}

/**
 * @brief 测试端点 - 用于验证服务器是否运行
 */
static esp_err_t ping_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "收到 ping 请求");
    const char *response = "ESP32-S3 HTTP Server is running!";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

/**
 * @brief 处理根路径 - 返回HTML页面
 */
static esp_err_t index_handler(httpd_req_t *req)
{
    return static_file_handler(req, WEB_BASE_PATH "index.html");
}

/**
 * @brief 处理 /styles.css 路径
 */
static esp_err_t css_handler(httpd_req_t *req)
{
    return static_file_handler(req, WEB_BASE_PATH "styles.css");
}

/**
 * @brief 处理 /app.js 路径
 */
static esp_err_t js_handler(httpd_req_t *req)
{
    return static_file_handler(req, WEB_BASE_PATH "app.js");
}

/**
 * @brief API: 获取系统状态
 */
static esp_err_t api_status_handler(httpd_req_t *req)
{
    char buffer[1500];

    // 获取水泵档位
    pump_gear_t pump_gear = actuators_pump_get_gear();

    // 格式化 JSON 响应 - 包含所有传感器数据和硬件资源监控
    snprintf(buffer, sizeof(buffer),
        "{"
        "\"thermistor_raw\":%lu,"
        "\"thermistor_temp\":%.1f,"
        "\"photosensor_raw\":%lu,"
        "\"light\":%.1f,"
        "\"dht11_temp\":%.1f,"
        "\"dht11_humidity\":%.1f,"
        "\"pump_state\":%d,"
        "\"pump_speed\":%d,"
        "\"pump_gear\":%d,"
        "\"pump_gear_name\":\"%s\","
        "\"servo_angle\":%d,"
        "\"sta_connected\":%d,"
        "\"sta_rssi\":%d,"
        "\"sta_bssid\":\"%s\","
        "\"dram_total\":%lu,"
        "\"dram_free\":%lu,"
        "\"dram_used\":%lu,"
        "\"psram_total\":%lu,"
        "\"psram_free\":%lu,"
        "\"psram_used\":%lu,"
        "\"flash_total\":%lu,"
        "\"spiffs_total\":%lu,"
        "\"spiffs_free\":%lu,"
        "\"spiffs_used\":%lu,"
        "\"sdcard_mounted\":%d,"
        "\"sdcard_total\":%llu,"
        "\"sdcard_free\":%llu,"
        "\"sdcard_used\":%llu,"
        "\"cpu_freq_mhz\":%lu,"
        "\"uptime_seconds\":%lu,"
        "\"version\":%lu"
        "}",
        (unsigned long)g_system_status.thermistor_raw,
        g_system_status.thermistor_temp,
        (unsigned long)g_system_status.photosensor_raw,
        g_system_status.light,
        g_system_status.dht11_temp,
        g_system_status.dht11_humidity,
        g_system_status.pump_state,
        g_system_status.pump_speed,
        pump_gear,
        actuators_pump_get_gear_name(pump_gear),
        g_system_status.servo_angle,
        g_system_status.sta_connected,
        g_system_status.sta_rssi,
        g_system_status.sta_bssid,
        (unsigned long)g_system_status.dram_total,
        (unsigned long)g_system_status.dram_free,
        (unsigned long)(g_system_status.dram_total > g_system_status.dram_free ?
            g_system_status.dram_total - g_system_status.dram_free : 0),
        (unsigned long)g_system_status.psram_total,
        (unsigned long)g_system_status.psram_free,
        (unsigned long)(g_system_status.psram_total > g_system_status.psram_free ?
            g_system_status.psram_total - g_system_status.psram_free : 0),
        (unsigned long)g_system_status.flash_total,
        (unsigned long)g_system_status.spiffs_total,
        (unsigned long)g_system_status.spiffs_free,
        (unsigned long)(g_system_status.spiffs_total > g_system_status.spiffs_free ?
            g_system_status.spiffs_total - g_system_status.spiffs_free : 0),
        g_system_status.sdcard_mounted,
        (unsigned long long)g_system_status.sdcard_total,
        (unsigned long long)g_system_status.sdcard_free,
        (unsigned long long)(g_system_status.sdcard_total > g_system_status.sdcard_free ?
            g_system_status.sdcard_total - g_system_status.sdcard_free : 0),
        (unsigned long)g_system_status.cpu_freq_mhz,
        (unsigned long)g_system_status.uptime_seconds,
        (unsigned long)g_system_status.version
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));

    return ESP_OK;
}

/**
 * @brief API: 获取网络状态
 */
static esp_err_t api_network_handler(httpd_req_t *req)
{
    char buffer[512];
    char sta_ip[16];
    char ap_ip[16];

    wifi_app_get_sta_ip_string(sta_ip, sizeof(sta_ip));
    wifi_app_get_ap_ip_string(ap_ip, sizeof(ap_ip));

    // 格式化 JSON 响应 - 包含网络状态信息
    snprintf(buffer, sizeof(buffer),
        "{"
        "\"sta_connected\":%d,"
        "\"sta_ssid\":\"%s\","
        "\"sta_ip\":\"%s\","
        "\"sta_rssi\":%d,"
        "\"sta_bssid\":\"%s\","
        "\"ap_ssid\":\"%s\","
        "\"ap_ip\":\"%s\""
        "}",
        g_system_status.sta_connected,
        g_system_status.sta_connected ? WIFI_STA_SSID : "",
        sta_ip,
        g_system_status.sta_rssi,
        g_system_status.sta_bssid,
        WIFI_SSID,
        ap_ip
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));

    return ESP_OK;
}

/**
 * @brief API: 水泵控制（支持档位和速度）
 */
static esp_err_t api_pump_handler(httpd_req_t *req)
{
    char buffer[256];
    char param_action[16] = {0};
    char param_speed[16] = {0};
    char param_gear[16] = {0};

    // 解析 URL 参数
    if (httpd_req_get_url_query_str(req, buffer, sizeof(buffer)) == ESP_OK) {
        ESP_LOGI(TAG, "参数: %s", buffer);
        httpd_query_key_value(buffer, "action", param_action, sizeof(param_action));
        httpd_query_key_value(buffer, "speed", param_speed, sizeof(param_speed));
        httpd_query_key_value(buffer, "gear", param_gear, sizeof(param_gear));
    }

    // 处理档位切换
    if (strlen(param_gear) > 0) {
        int gear = atoi(param_gear);
        if (gear >= PUMP_OFF && gear <= PUMP_HIGH) {
            actuators_pump_set_gear((pump_gear_t)gear);
            pump_gear_t current_gear = actuators_pump_get_gear();
            snprintf(buffer, sizeof(buffer),
                "{\"success\":true,\"gear\":%d,\"gear_name\":\"%s\",\"speed\":%d}",
                current_gear,
                actuators_pump_get_gear_name(current_gear),
                g_system_status.pump_speed);
            ESP_LOGI(TAG, "水泵档位: %d", gear);
        } else {
            snprintf(buffer, sizeof(buffer),
                "{\"success\":false,\"error\":\"invalid gear\"}");
        }
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, buffer, strlen(buffer));
        return ESP_OK;
    }

    // 处理开关和速度控制（保持向后兼容）
    int speed = atoi(param_speed);
    if (speed < 0) speed = 0;
    if (speed > 100) speed = 100;

    if (strcmp(param_action, "on") == 0) {
        actuators_motor_set_speed(speed);
        actuators_motor_start();

        snprintf(buffer, sizeof(buffer),
            "{\"success\":true,\"state\":\"on\",\"speed\":%d}", speed);
        ESP_LOGI(TAG, "水泵开启: %d%%", speed);

    } else if (strcmp(param_action, "off") == 0) {
        actuators_motor_stop();

        snprintf(buffer, sizeof(buffer),
            "{\"success\":true,\"state\":\"off\"}");
        ESP_LOGI(TAG, "水泵关闭");

    } else {
        snprintf(buffer, sizeof(buffer),
            "{\"success\":false,\"error\":\"invalid action\"}");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));

    return ESP_OK;
}

/**
 * @brief API: 舵机控制
 */
static esp_err_t api_servo_handler(httpd_req_t *req)
{
    char buffer[128];
    char param_angle[16] = {0};

    if (httpd_req_get_url_query_str(req, buffer, sizeof(buffer)) == ESP_OK) {
        httpd_query_key_value(buffer, "angle", param_angle, sizeof(param_angle));
    }

    int angle = atoi(param_angle);
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    actuators_servo_set_angle(angle);

    snprintf(buffer, sizeof(buffer), 
        "{\"success\":true,\"angle\":%d}", angle);
    ESP_LOGI(TAG, "舵机角度: %d", angle);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));

    return ESP_OK;
}

/**
 * @brief API: 获取配置信息
 */
static esp_err_t api_config_handler(httpd_req_t *req)
{
    char buffer[512];
    char ip_str[32];

    wifi_app_get_ip_string(ip_str, sizeof(ip_str));

    snprintf(buffer, sizeof(buffer),
        "{"
        "\"wifi_ssid\":\"%s\","
        "\"ip_address\":\"%s\","
        "\"server_version\":\"1.0\","
        "\"gpio_pwma\":%d,"
        "\"gpio_ain1\":%d,"
        "\"gpio_ain2\":%d"
        "}",
        WIFI_SSID,
        ip_str,
        MOTOR_PWMA_PIN,
        MOTOR_AIN1_PIN,
        MOTOR_AIN2_PIN
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));

    return ESP_OK;
}

/* TF卡文件管理 - 使用sdcard_web模块提供的处理函数 */

/**
 * @brief 静态文件: TF 卡文件管理器页面
 */
static esp_err_t sdcard_page_handler(httpd_req_t *req)
{
    return static_file_handler(req, WEB_BASE_PATH "sdcard.html");
}

/**
 * @brief 静态文件: TF 卡 CSS 样式
 */
static esp_err_t sdcard_css_handler(httpd_req_t *req)
{
    return static_file_handler(req, WEB_BASE_PATH "sdcard.css");
}

/**
 * @brief 静态文件: TF 卡 JS 脚本
 */
static esp_err_t sdcard_js_handler(httpd_req_t *req)
{
    return static_file_handler(req, WEB_BASE_PATH "sdcard.js");
}

/* HTTP 服务器 URI 路由表 */
static const httpd_uri_t uri_routes[] = {
    {.uri = "/",              .method = HTTP_GET, .handler = index_handler},
    {.uri = "/index.html",    .method = HTTP_GET, .handler = index_handler},
    {.uri = "/ping",          .method = HTTP_GET, .handler = ping_handler},
    {.uri = "/styles.css",    .method = HTTP_GET, .handler = css_handler},
    {.uri = "/app.js",        .method = HTTP_GET, .handler = js_handler},
    {.uri = "/api/status",    .method = HTTP_GET, .handler = api_status_handler},
    {.uri = "/api/network",   .method = HTTP_GET, .handler = api_network_handler},
    {.uri = "/api/pump",      .method = HTTP_GET, .handler = api_pump_handler},
    {.uri = "/api/servo",     .method = HTTP_GET, .handler = api_servo_handler},
    {.uri = "/api/config",    .method = HTTP_GET, .handler = api_config_handler},
    /* TF 卡文件浏览路由 - 使用 /fs 前缀避免路径冲突 */
    {.uri = "/fs",            .method = HTTP_GET, .handler = sdcard_page_handler},
    {.uri = "/fs/",           .method = HTTP_GET, .handler = sdcard_page_handler},
    {.uri = "/fs/sdcard.css", .method = HTTP_GET, .handler = sdcard_css_handler},
    {.uri = "/fs/sdcard.js",  .method = HTTP_GET, .handler = sdcard_js_handler},
    {.uri = "/fs/files",     .method = HTTP_GET, .handler = sdcard_web_download_handler},
    {.uri = "/fs/upload",    .method = HTTP_POST, .handler = sdcard_web_upload_handler},
    {.uri = "/api/sdcard/debug",  .method = HTTP_GET, .handler = sdcard_web_debug_handler},
    {.uri = "/api/sdcard/files",  .method = HTTP_GET, .handler = sdcard_web_files_handler},
    {.uri = "/api/sdcard/info",    .method = HTTP_GET, .handler = sdcard_web_info_handler},
    /* Captive Portal 路由 - 捕获常见重定向URL */
    {.uri = "/generate_204",  .method = HTTP_GET, .handler = captive_portal_handler},
    {.uri = "/fwlink/",       .method = HTTP_GET, .handler = captive_portal_handler},
    {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = captive_portal_handler},
    {.uri = "/connecttest.txt", .method = HTTP_GET, .handler = captive_portal_handler},
    {.uri = "/ncsi.txt",      .method = HTTP_GET, .handler = captive_portal_handler},
    {.uri = "/redirect",      .method = HTTP_GET, .handler = captive_portal_handler},
    /* iOS Captive Portal */
    {.uri = "/captive.apple.com", .method = HTTP_GET, .handler = captive_portal_handler},
    {.uri = "/www.apple.com/library/test/success.html", .method = HTTP_GET, .handler = captive_portal_handler},
    /* Windows Captive Portal */
    {.uri = "/www.msftconnecttest.com/redirect", .method = HTTP_GET, .handler = captive_portal_handler},
    {.uri = "/msftconnecttest.com/connecttest.txt", .method = HTTP_GET, .handler = captive_portal_handler},
    /* Android Captive Portal */
    {.uri = "/clients3.google.com/generate_204", .method = HTTP_GET, .handler = captive_portal_handler},
    {.uri = "/connectivitycheck.gstatic.com/generate_204", .method = HTTP_GET, .handler = captive_portal_handler},
    /* 通用 */
    {.uri = "/success.txt",   .method = HTTP_GET, .handler = captive_portal_handler},
    {.uri = "/httpok",        .method = HTTP_GET, .handler = captive_portal_handler},
};

/**
 * @brief HTTP 服务器初始化
 */
esp_err_t http_server_init(void)
{
    ESP_LOGI(TAG, "初始化 HTTP 服务器...");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_PORT;
    config.stack_size = 8192;  // 增加栈空间
    config.max_uri_handlers = 40;

    httpd_handle_t server = NULL;
    esp_err_t ret = httpd_start(&server, &config);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP 服务器启动失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 注册所有 URI 路由
    for (int i = 0; i < sizeof(uri_routes) / sizeof(uri_routes[0]); i++) {
        ret = httpd_register_uri_handler(server, &uri_routes[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "注册 URI 失败: %s", uri_routes[i].uri);
            return ret;
        }
    }

    ESP_LOGI(TAG, "HTTP 服务器启动成功 (端口 %d)", HTTP_PORT);
    ESP_LOGI(TAG, "访问 http://%d.%d.%d.%d:%d 控制水泵",
             WIFI_AP_IP_1, WIFI_AP_IP_2, WIFI_AP_IP_3, WIFI_AP_IP_4, HTTP_PORT);

    return ESP_OK;
}
