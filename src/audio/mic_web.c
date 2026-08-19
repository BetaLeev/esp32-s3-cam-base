#include "audio/mic.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "web/http/http.h"
#include "web/web.h"
#include "audio_loopback.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "MIC_WEB";

// 原有接口：获取麦克风状态
static esp_err_t mic_web_api_get_status(httpd_req_t *req) {
    char json[128];
    int level = mic_get_sound_level();
    bool testing = mic_is_testing();

    snprintf(json, sizeof(json),
             "{\"initialized\": true, \"testing\": %s, \"sound_level\": %d}",
             testing ? "true" : "false", level);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// 原有接口：开始麦克风测试
static esp_err_t mic_web_api_start(httpd_req_t *req) {
    esp_err_t err = mic_init();
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Mic initialization failed");
        return ESP_FAIL;
    }

    mic_set_testing(true);
    audio_loopback_set_enabled(true);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\": true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// 原有接口：停止麦克风测试
static esp_err_t mic_web_api_stop(httpd_req_t *req) {
    mic_set_testing(false);
    audio_loopback_set_enabled(false);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\": true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// 原有接口：获取麦克风高级调参
static esp_err_t mic_web_api_get_config(httpd_req_t *req) {
    int sr, shift;
    float scale;
    mic_get_params(&sr, &shift, &scale);

    char json[128];
    snprintf(json, sizeof(json), "{\"sample_rate\":%d, \"shift_bits\":%d, \"volume_scale\":%.2f}",
             sr, shift, scale);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// 原有接口：设置麦克风高级调参
static esp_err_t mic_web_api_set_config(httpd_req_t *req) {
    char content[256];
    int recv_size = MIN(req->content_len, sizeof(content) - 1);
    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive content");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *root = cJSON_Parse(content);
    if (root) {
        cJSON *sr = cJSON_GetObjectItem(root, "sample_rate");
        cJSON *shift = cJSON_GetObjectItem(root, "shift_bits");
        cJSON *scale = cJSON_GetObjectItem(root, "volume_scale");

        int cur_sr, cur_shift;
        float cur_scale;
        mic_get_params(&cur_sr, &cur_shift, &cur_scale);

        mic_set_params(
            sr ? sr->valueint : cur_sr,
            shift ? shift->valueint : cur_shift,
            scale ? (float)scale->valuedouble : cur_scale
        );
        cJSON_Delete(root);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ============================================================
// 新增接口：获取设备离线命令词识别结果
// 路径：/api/speech/command
// ============================================================
static esp_err_t mic_web_api_get_command(httpd_req_t *req) {
    const char *cmd = "";

    char json[128];
    snprintf(json, sizeof(json), "{\"text\":\"%s\"}", cmd ? cmd : "");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// URI 定义
static const httpd_uri_t mic_status_uri = {
    .uri = "/api/mic/status",
    .method = HTTP_GET,
    .handler = mic_web_api_get_status,
    .user_ctx = NULL
};

static const httpd_uri_t mic_start_uri = {
    .uri = "/api/mic/start",
    .method = HTTP_POST,
    .handler = mic_web_api_start,
    .user_ctx = NULL
};

static const httpd_uri_t mic_stop_uri = {
    .uri = "/api/mic/stop",
    .method = HTTP_POST,
    .handler = mic_web_api_stop,
    .user_ctx = NULL
};

static const httpd_uri_t mic_config_get_uri = {
    .uri = "/api/mic/config",
    .method = HTTP_GET,
    .handler = mic_web_api_get_config,
    .user_ctx = NULL
};

static const httpd_uri_t mic_config_post_uri = {
    .uri = "/api/mic/config",
    .method = HTTP_POST,
    .handler = mic_web_api_set_config,
    .user_ctx = NULL
};

// 新增路由
static const httpd_uri_t speech_command_uri = {
    .uri = "/api/speech/command",
    .method = HTTP_GET,
    .handler = mic_web_api_get_command,
    .user_ctx = NULL
};

esp_err_t mic_web_init(void) {
    httpd_handle_t server = web_http_get_handle();
    if (server == NULL) {
        ESP_LOGE(TAG, "HTTP 服务器尚未启动，无法注册麦克风路由！");
        return ESP_ERR_INVALID_STATE;
    }

    httpd_register_uri_handler(server, &mic_status_uri);
    httpd_register_uri_handler(server, &mic_start_uri);
    httpd_register_uri_handler(server, &mic_stop_uri);
    httpd_register_uri_handler(server, &mic_config_get_uri);
    httpd_register_uri_handler(server, &mic_config_post_uri);
    httpd_register_uri_handler(server, &speech_command_uri);   // 新增

    ESP_LOGI(TAG, "麦克风 Web API 及调参路由注册完成");
    return ESP_OK;
}