#include "http_server.h"
#include "config.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "sdcard/sdcard_web.h"
#include "sensors/sensors_web.h"
#include "spiffs_web.h"
#include "system/system_web.h"
#include "video/video_web.h"
#include "wifi/wifi_web.h"
#include "device/led/led_web.h"
#include "device/pulse/pulse_web.h"
#include "device/motor/motor.h"
#include "device/motor/motor_web.h"
#include "device/servo/servo.h"
#include "device/servo/servo_web.h"
#include "ai/ai_web.h"
#include "ai/ai_ws.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "HTTP_SERVER";
static httpd_handle_t s_http_server_handle = NULL;

httpd_handle_t get_httpd_handle(void) {
    return s_http_server_handle;
}

// 静态文件通配符处理 Handler
static esp_err_t wildcard_static_handler(httpd_req_t *req) {
    // API 请求未匹配任何路由，返回 404
    if (strncmp(req->uri, "/api/", 5) == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"status\":\"error\",\"code\":404,\"message\":\"Not Found\"}", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    char *filepath = malloc(512);
    if (filepath == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (strcmp(req->uri, "/") == 0) {
        snprintf(filepath, 512, "web/index.html");
    } else {
        // 限制路径长度，避免截断警告
        size_t uri_len = strlen(req->uri);
        if (uri_len > 500) {
            free(filepath);
            return ESP_ERR_INVALID_SIZE;
        }
        snprintf(filepath, 512, "web%s", req->uri);
    }

    esp_err_t ret = spiffs_web_file_handler(req, filepath);
    free(filepath);
    return ret;
}

esp_err_t http_server_init(void) {
    HTTP_LOGI(TAG, "初始化 HTTP 服务器...");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 10240;
    config.max_open_sockets = 7;
    config.max_uri_handlers = 64; // ← 添加这一行！
    config.lru_purge_enable = true;
    config.send_wait_timeout = 10;
    config.recv_wait_timeout = 300;

    esp_err_t ret = httpd_start(&s_http_server_handle, &config);
    if (ret != ESP_OK) {
        HTTP_LOGE(TAG, "启动 HTTP 服务器失败: %s", esp_err_to_name(ret));
        return ret;
    }

    HTTP_LOGI(TAG, "HTTP 服务器已启动，开始注册路由...");

    // 注册所有模块 API 路由（精确匹配优先）
    wifi_web_register_routes(s_http_server_handle);
    HTTP_LOGI(TAG, "WiFi 路由已注册");

    video_web_register_routes(s_http_server_handle);
    HTTP_LOGI(TAG, "Video 路由已注册");

    sdcard_web_register_routes(s_http_server_handle);
    HTTP_LOGI(TAG, "SDCard 路由已注册");

    sensors_web_register_routes(s_http_server_handle);
    HTTP_LOGI(TAG, "Sensors 路由已注册");

    system_web_register_routes(s_http_server_handle);
    HTTP_LOGI(TAG, "System 路由已注册");

    led_web_register_routes(s_http_server_handle);
    HTTP_LOGI(TAG, "LED 路由已注册");

    pulse_web_register_routes(s_http_server_handle);
    HTTP_LOGI(TAG, "Pulse 路由已注册");

    // 注册电机 API
    {
        httpd_uri_t pump_uri = {
            .uri = "/api/pump",
            .method = HTTP_GET,
            .handler = motor_web_pump_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(s_http_server_handle, &pump_uri);
        HTTP_LOGI(TAG, "水泵路由已注册");

        httpd_uri_t servo_uri = {
            .uri = "/api/servo",
            .method = HTTP_GET,
            .handler = servo_web_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(s_http_server_handle, &servo_uri);
        HTTP_LOGI(TAG, "舵机路由已注册");

        httpd_uri_t motor_uri = {
            .uri = "/api/motor",
            .method = HTTP_GET,
            .handler = motor_web_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(s_http_server_handle, &motor_uri);
        HTTP_LOGI(TAG, "电机路由已注册");
    }

    ai_web_register_routes(s_http_server_handle);
    HTTP_LOGI(TAG, "AI HTTP 路由已注册");

    ai_ws_register_routes(s_http_server_handle);
    HTTP_LOGI(TAG, "AI WebSocket 路由已注册");

    // 通配符路由放最后，作为静态文件兜底
    httpd_uri_t static_uri = {
        .uri = "/*", .method = HTTP_GET, .handler = wildcard_static_handler, .user_ctx = NULL};
    ret = httpd_register_uri_handler(s_http_server_handle, &static_uri);
    HTTP_LOGI(TAG, "通配符路由注册: %s", ret == ESP_OK ? "OK" : "FAIL");

    HTTP_LOGI(TAG, "HTTP 服务器启动成功");
    return ESP_OK;
}
