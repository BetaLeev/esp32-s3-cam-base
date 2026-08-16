/**
 * @file http.c
 * @brief HTTP 服务器实现
 */
#include "http.h"
#include "../filesystem/filesystem.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "WEB_HTTP";
static httpd_handle_t s_http_handle = NULL;

/* 获取 HTTP 服务器句柄 */
httpd_handle_t web_http_get_handle(void) {
    return s_http_handle;
}

/* 静态文件处理 Handler */
static esp_err_t wildcard_handler(httpd_req_t *req) {
    // 1. 处理 API 请求未注册的情况
    if (strncmp(req->uri, "/api/", 5) == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND,
                            "{\"status\":\"error\",\"code\":404,\"message\":\"API Not Found\"}");
        return ESP_OK;
    }

    // 2. 拦截 iOS / Android 强制门户探活，防止 Safari 弹出 Captive 页面
    if (strstr(req->uri, "hotspot-detect.html") || strstr(req->uri, "canonical.html") ||
        strstr(req->uri, "generate_204")) {

        // 返回成功标识，告知 iOS 当前网络正常，无需弹窗
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>",
                        HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    char filepath[256] = {0};
    const char *uri = req->uri;

    // 剥离 Query 参数
    const char *quest = strchr(uri, '?');
    size_t uri_len = quest ? (size_t)(quest - uri) : strlen(uri);

    if (uri_len >= sizeof(filepath)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "URI Too Long");
        return ESP_FAIL;
    }

    if (uri_len == 1 && uri[0] == '/') {
        strncpy(filepath, "index.html", sizeof(filepath) - 1);
    } else if (uri[0] == '/') {
        strncpy(filepath, uri + 1, uri_len - 1);
        filepath[uri_len - 1] = '\0';
    } else {
        strncpy(filepath, uri, uri_len);
        filepath[uri_len] = '\0';
    }

    return web_filesystem_serve_file(req, filepath);
}
/**
 * @brief 初始化 HTTP 服务器
 */
esp_err_t web_http_init(void) {
    ESP_LOGI(TAG, "初始化 HTTP 服务器...");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    // ---- 优化 Socket 资源，解决 error (23) ----
    config.stack_size = 10240;
    config.max_open_sockets = 10;   // 适当增加 Socket 句柄（默认 7）
    config.lru_purge_enable = true; // 开启 LRU 机制：Socket 满时自动清理老旧闲置连接
    config.recv_wait_timeout = 5;   // 降低接收超时时间，加速 Socket 回收
    config.send_wait_timeout = 5;   // 降低发送超时时间
    config.max_uri_handlers = 64;
    config.uri_match_fn = httpd_uri_match_wildcard;

    esp_err_t ret = httpd_start(&s_http_handle, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动 HTTP 服务器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "HTTP 服务器已启动，开始注册 API 路由...");

/* 1. 注册所有模块的 API 路由 */
#include "wifi/wifi_web.h"
    wifi_web_register_routes(s_http_handle);

#include "video/video_web.h"
    video_web_register_routes(s_http_handle);

#include "sdcard/sdcard_web.h"
    sdcard_web_register_routes(s_http_handle);

#include "sensors/sensors_web.h"
    sensors_web_register_routes(s_http_handle);

#include "system/system_web.h"
    system_web_register_routes(s_http_handle);

#include "device/led/led_web.h"
    led_web_register_routes(s_http_handle);

#include "device/pulse/pulse_web.h"
    pulse_web_register_routes(s_http_handle);

#include "device/motor/motor.h"
#include "device/motor/motor_web.h"
#include "device/servo/servo.h"
#include "device/servo/servo_web.h"

    {
        httpd_uri_t uri = {.uri = "/api/pump",
                           .method = HTTP_GET,
                           .handler = motor_web_pump_handler,
                           .user_ctx = NULL};
        httpd_register_uri_handler(s_http_handle, &uri);

        uri.uri = "/api/servo";
        uri.handler = servo_web_handler;
        httpd_register_uri_handler(s_http_handle, &uri);

        uri.uri = "/api/motor";
        uri.handler = motor_web_handler;
        httpd_register_uri_handler(s_http_handle, &uri);
    }

#include "ai/ai_web.h"
    ai_web_register_routes(s_http_handle);

#include "ai/ai_ws.h"
    ai_ws_register_routes(s_http_handle);

    /* 2. 注册静态资源通配路由 (匹配所有未被上面 API 覆盖的 GET 请求) */
    httpd_uri_t static_uri = {
        .uri = "/*", .method = HTTP_GET, .handler = wildcard_handler, .user_ctx = NULL};
    ret = httpd_register_uri_handler(s_http_handle, &static_uri);
    ESP_LOGI(TAG, "静态文件/SPA 兜底路由注册: %s", ret == ESP_OK ? "OK" : "FAIL");

    ESP_LOGI(TAG, "HTTP 服务器启动成功");
    return ESP_OK;
}

/**
 * @brief 反初始化 HTTP 服务器
 */
esp_err_t web_http_deinit(void) {
    if (s_http_handle != NULL) {
        httpd_stop(s_http_handle);
        s_http_handle = NULL;
        ESP_LOGI(TAG, "HTTP 服务器已停止");
    }
    return ESP_OK;
}