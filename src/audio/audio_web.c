/**
 * @file audio_web.c
 * @brief 音频模块Web接口实现 (异步优化版)
 */
#include "audio_web.h"
#include "audio.h"
#include "audio_json.h"
#include "audio_simple.h"
#include "../web/http/http.h"
#include "../utils/path_utils.h"
#include "config.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "AUDIO_WEB";

#define AUDIO_WEB_JSON_SIZE    512  
#define AUDIO_WEB_REQ_SIZE      512  

static esp_err_t send_error_response(httpd_req_t *req, const char *message)
{
    char *json = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int len = snprintf(json, AUDIO_WEB_JSON_SIZE,
                       "{\"error\": \"%s\"}", message ? message : "Unknown error");

    esp_err_t ret = httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        ret = httpd_resp_send(req, json, len);
    }

    free(json);
    return ret;
}

static char* parse_post_body(httpd_req_t *req)
{
    char *buffer = (char*)malloc(AUDIO_WEB_REQ_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return NULL;
    }

    int ret = httpd_req_recv(req, buffer, AUDIO_WEB_REQ_SIZE - 1);
    if (ret <= 0) {
        free(buffer);
        return NULL;
    }

    buffer[ret] = '\0';
    return buffer;
}

esp_err_t audio_web_api_get_status(httpd_req_t *req)
{
    AUDIO_LOGI(TAG, "get_status 请求开始");
    
    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    audio_gain_t gain = audio_get_gain();
    uint8_t volume = audio_get_volume_percent();
    bool is_playing = audio_simple_is_playing();

    // 转换增益值到 dB
    int gain_db = (gain == AUDIO_GAIN_3DB) ? 3 :
                  (gain == AUDIO_GAIN_6DB) ? 6 :
                  (gain == AUDIO_GAIN_9DB) ? 9 : 12;

    // 根据播放状态确定 state
    const char *state = audio_is_initialized() ? 
                        (is_playing ? "playing" : "ready") : "uninit";

    int len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
        "{"
        "\"initialized\": %s,"
        "\"playing\": %s,"
        "\"state\": \"%s\","
        "\"gain\": %d,"
        "\"gain_db\": %d,"
        "\"volume\": %d"
        "}",
        audio_is_initialized() ? "true" : "false",
        is_playing ? "true" : "false",
        state,
        gain,
        gain_db,
        volume
    );

    esp_err_t ret = httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        ret = httpd_resp_send(req, json_str, len);
    }

    free(json_str);
    return ret;
}

esp_err_t audio_web_api_play(httpd_req_t *req)
{
    if (!audio_is_initialized()) {
        return send_error_response(req, "Audio module not initialized");
    }

    char *body = parse_post_body(req);
    if (body == NULL) {
        return send_error_response(req, "Invalid request body");
    }

    json_parse_result_t result;
    if (!json_parse_string(body, "file", &result) || result.str_val == NULL) {
        free(body);
        return send_error_response(req, "Missing 'file' parameter");
    }

    char *file_path = (char*)malloc(result.str_len + 1);
    if (file_path == NULL) {
        free(body);
        return send_error_response(req, "Memory allocation failed");
    }
    memcpy(file_path, result.str_val, result.str_len);
    file_path[result.str_len] = '\0';

    char decoded_path[512];
    path_url_decode(file_path, decoded_path, sizeof(decoded_path));
    free(file_path);

    char full_path[512];
    if (decoded_path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "%s", decoded_path);
    } else {
        snprintf(full_path, sizeof(full_path), "/sdcard/%s", decoded_path);
    }

    ESP_LOGI(TAG, "异步播放请求: %s", full_path);
    free(body);

    esp_err_t play_ret = audio_simple_play(full_path);

    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
        "{\"success\": %s, \"message\": \"Playback queued in background\"}",
        play_ret == ESP_OK ? "true" : "false");

    esp_err_t ret = httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        ret = httpd_resp_send(req, json_str, len);
    }

    free(json_str);
    return ret;
}

esp_err_t audio_web_api_test(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        return send_error_response(req, "Only POST supported");
    }

    AUDIO_LOGI(TAG, "播放测试音调 (C大调三和弦)");
    // 异步播放：发送 3 个音调到队列，每个间隔 100ms
    audio_simple_play_tone(262, 300);
    vTaskDelay(pdMS_TO_TICKS(100));
    audio_simple_play_tone(330, 300);
    vTaskDelay(pdMS_TO_TICKS(100));
    audio_simple_play_tone(392, 500);

    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int len = snprintf(json_str, AUDIO_WEB_JSON_SIZE, 
        "{\"success\": true, \"message\": \"Playing C major chord (C4+E4+G4)\"}");

    esp_err_t ret = httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        ret = httpd_resp_send(req, json_str, len);
    }

    free(json_str);
    return ret;
}

esp_err_t audio_web_api_stop(httpd_req_t *req)
{
    audio_simple_stop();

    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int len = snprintf(json_str, AUDIO_WEB_JSON_SIZE, "{\"success\": true}");

    esp_err_t ret = httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        ret = httpd_resp_send(req, json_str, len);
    }

    free(json_str);
    return ret;
}

esp_err_t audio_web_api_set_volume(httpd_req_t *req)
{
    char *body = parse_post_body(req);
    if (body == NULL) {
        return send_error_response(req, "Invalid request body");
    }

    json_parse_result_t result;
    if (!json_parse_int(body, "volume", &result) || !result.is_valid) {
        free(body);
        return send_error_response(req, "Missing 'volume' parameter");
    }

    int volume = result.int_val;
    if (volume < 0 || volume > 100) {
        free(body);
        return send_error_response(req, "Volume must be 0-100");
    }

    free(body);

    audio_simple_set_volume((uint8_t)volume);

    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
        "{\"success\": true, \"volume\": %d}", volume);

    esp_err_t ret = httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        ret = httpd_resp_send(req, json_str, len);
    }

    free(json_str);
    return ret;
}

static const httpd_uri_t audio_status_uri = {
    .uri = "/api/audio/status", .method = HTTP_GET, .handler = audio_web_api_get_status, .user_ctx = NULL
};
static const httpd_uri_t audio_play_uri = {
    .uri = "/api/audio/play", .method = HTTP_POST, .handler = audio_web_api_play, .user_ctx = NULL
};
static const httpd_uri_t audio_stop_uri = {
    .uri = "/api/audio/stop", .method = HTTP_POST, .handler = audio_web_api_stop, .user_ctx = NULL
};
static const httpd_uri_t audio_volume_uri = {
    .uri = "/api/audio/volume", .method = HTTP_POST, .handler = audio_web_api_set_volume, .user_ctx = NULL
};
static const httpd_uri_t audio_test_uri = {
    .uri = "/api/audio/test", .method = HTTP_POST, .handler = audio_web_api_test, .user_ctx = NULL
};

esp_err_t audio_web_api_set_gain(httpd_req_t *req)
{
    char *body = parse_post_body(req);
    if (body == NULL) {
        return send_error_response(req, "Invalid request body");
    }

    json_parse_result_t result;
    if (!json_parse_int(body, "gain", &result) || !result.is_valid) {
        free(body);
        return send_error_response(req, "Missing 'gain' parameter");
    }

    int gain = result.int_val;
    if (gain < 0 || gain > 3) {
        free(body);
        return send_error_response(req, "Gain must be 0-3");
    }

    esp_err_t ret = audio_set_gain((audio_gain_t)gain);
    free(body);

    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
        "{\"success\": %s, \"gain\": %d}", 
        ret == ESP_OK ? "true" : "false", gain);

    esp_err_t http_ret = httpd_resp_set_type(req, "application/json");
    if (http_ret == ESP_OK) {
        http_ret = httpd_resp_send(req, json_str, len);
    }

    free(json_str);
    return http_ret;
}

static const httpd_uri_t audio_gain_uri = {
    .uri = "/api/audio/gain", .method = HTTP_POST, .handler = audio_web_api_set_gain, .user_ctx = NULL
};

esp_err_t audio_web_register_routes(void)
{
    httpd_handle_t server = web_http_get_handle();
    if (server == NULL) return ESP_ERR_INVALID_STATE;

    httpd_register_uri_handler(server, &audio_status_uri);
    httpd_register_uri_handler(server, &audio_play_uri);
    httpd_register_uri_handler(server, &audio_stop_uri);
    httpd_register_uri_handler(server, &audio_volume_uri);
    httpd_register_uri_handler(server, &audio_test_uri);
    httpd_register_uri_handler(server, &audio_gain_uri);

    return ESP_OK;
}

esp_err_t audio_web_init(void) { return audio_web_register_routes(); }
esp_err_t audio_web_deinit(void) { return ESP_OK; }