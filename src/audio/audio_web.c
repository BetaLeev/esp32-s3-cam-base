/**
 * @file audio_web.c
 * @brief 音频模块Web接口实现
 *
 * 提供HTTP API接口用于Web控制和状态查询
 * 遵循规则：大缓冲区使用malloc，POSIX VFS接口，严格错误处理
 */
#include "audio_web.h"
#include "audio.h"
#include "audio_json.h"
#include "../http_server.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "AUDIO_WEB";

/* Web缓冲区大小 - 使用宏定义，便于调整 */
#define AUDIO_WEB_JSON_SIZE    512  /**< JSON响应缓冲区大小 */
#define AUDIO_WEB_REQ_SIZE      512  /**< 请求解析缓冲区大小 */

/* ========================================
 * 辅助函数
 * ======================================== */

/**
 * @brief 发送JSON响应
 */
static esp_err_t send_json_response(httpd_req_t *req, const char *json)
{
    if (json == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, strlen(json));
    return ret;
}

/**
 * @brief 发送错误响应
 */
static esp_err_t send_error_response(httpd_req_t *req, const char *message)
{
    /* 使用malloc分配而不是栈数组 */
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

/**
 * @brief 解析POST请求体
 */
static char* parse_post_body(httpd_req_t *req)
{
    /* 使用malloc分配而不是栈数组（避免栈溢出） */
    char *buffer = (char*)malloc(AUDIO_WEB_REQ_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return NULL;
    }

    /* 读取请求体 */
    int ret = httpd_req_recv(req, buffer, AUDIO_WEB_REQ_SIZE - 1);
    if (ret <= 0) {
        free(buffer);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "接收请求超时");
        } else {
            ESP_LOGE(TAG, "接收请求失败");
        }
        return NULL;
    }

    buffer[ret] = '\0';  /* 确保字符串结束 */
    ESP_LOGD(TAG, "收到请求: %s", buffer);

    return buffer;
}

/* ========================================
 * Web API 实现
 * ======================================== */

esp_err_t audio_web_api_get_status(httpd_req_t *req)
{
    /* 使用malloc分配JSON缓冲区 */
    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    audio_state_t state = audio_get_state();
    audio_gain_t gain = audio_get_gain();

    const char *state_str;
    switch (state) {
        case AUDIO_STATE_UNINIT:   state_str = "uninit"; break;
        case AUDIO_STATE_READY:    state_str = "ready"; break;
        case AUDIO_STATE_PLAYING:  state_str = "playing"; break;
        case AUDIO_STATE_PAUSED:   state_str = "paused"; break;
        case AUDIO_STATE_ERROR:    state_str = "error"; break;
        default:                    state_str = "unknown"; break;
    }

    int gain_db = (gain == AUDIO_GAIN_3DB) ? 3 :
                  (gain == AUDIO_GAIN_6DB) ? 6 :
                  (gain == AUDIO_GAIN_9DB) ? 9 : 12;

    int len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
        "{"
        "\"initialized\": %s,"
        "\"state\": \"%s\","
        "\"gain\": %d,"
        "\"gain_db\": %d"
        "}",
        audio_is_initialized() ? "true" : "false",
        state_str,
        gain,
        gain_db
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

    /* 解析请求体 */
    char *body = parse_post_body(req);
    if (body == NULL) {
        return send_error_response(req, "Invalid request body");
    }

    json_parse_result_t result;
    if (!json_parse_string(body, "file", &result) || result.str_val == NULL) {
        free(body);
        return send_error_response(req, "Missing 'file' parameter");
    }

    /* 复制文件路径 */
    char *file_path = (char*)malloc(result.str_len + 1);
    if (file_path == NULL) {
        free(body);
        return send_error_response(req, "Memory allocation failed");
    }
    memcpy(file_path, result.str_val, result.str_len);
    file_path[result.str_len] = '\0';

    ESP_LOGI(TAG, "播放请求: %s", file_path);
    free(body);

    /* 使用malloc分配响应缓冲区 */
    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        free(file_path);
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    /* 执行播放 */
    int32_t duration = audio_play_wav(file_path);
    free(file_path);

    int len;
    if (duration >= 0) {
        len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
            "{\"success\": true, \"duration_ms\": %d}", (int)duration);
    } else {
        len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
            "{\"success\": false, \"error\": \"Playback failed\"}");
    }

    esp_err_t ret = httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        ret = httpd_resp_send(req, json_str, len);
    }

    free(json_str);
    return ret;
}

esp_err_t audio_web_api_stop(httpd_req_t *req)
{
    esp_err_t ret = audio_stop();

    /* 使用malloc分配响应缓冲区 */
    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
        "{\"success\": %s}", ret == ESP_OK ? "true" : "false");

    ret = httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        ret = httpd_resp_send(req, json_str, len);
    }

    free(json_str);
    return ret;
}

esp_err_t audio_web_api_set_pins(httpd_req_t *req)
{
    /* 解析请求体 */
    char *body = parse_post_body(req);
    if (body == NULL) {
        return send_error_response(req, "Invalid request body");
    }

    /* 提取引脚配置 */
    audio_pin_config_t pin_config = {0};
    json_parse_result_t result;
    bool has_error = false;

    if (json_parse_int(body, "bclk", &result) && result.is_valid) {
        pin_config.bclk = (gpio_num_t)result.int_val;
    } else {
        has_error = true;
    }

    if (json_parse_int(body, "ws", &result) && result.is_valid) {
        pin_config.ws = (gpio_num_t)result.int_val;
    } else {
        has_error = true;
    }

    if (json_parse_int(body, "din", &result) && result.is_valid) {
        pin_config.din = (gpio_num_t)result.int_val;
    } else {
        has_error = true;
    }

    if (json_parse_int(body, "gain", &result) && result.is_valid) {
        pin_config.gain = (gpio_num_t)result.int_val;
        pin_config.gain_enable = true;
    } else {
        pin_config.gain_enable = false;
    }

    if (json_parse_int(body, "sd", &result) && result.is_valid) {
        pin_config.sd = (gpio_num_t)result.int_val;
        pin_config.sd_enable = true;
    } else {
        pin_config.sd_enable = false;
    }

    free(body);

    if (has_error) {
        return send_error_response(req, "Missing required pin configuration");
    }

    /* 应用新配置 */
    esp_err_t ret = audio_reconfig_pins(&pin_config);

    /* 使用malloc分配响应缓冲区 */
    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int len;
    if (ret == ESP_OK) {
        len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
            "{\"success\": true, \"message\": \"Pins reconfigured\", "
            "\"bclk\": %d, \"ws\": %d, \"din\": %d}",
            pin_config.bclk, pin_config.ws, pin_config.din);
    } else {
        len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
            "{\"success\": false, \"error\": \"Reconfiguration failed\"}");
    }

    ret = httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        ret = httpd_resp_send(req, json_str, len);
    }

    free(json_str);
    return ret;
}

esp_err_t audio_web_api_get_pins(httpd_req_t *req)
{
    audio_pin_config_t pin_config;
    esp_err_t ret = audio_get_pin_config(&pin_config);

    /* 使用malloc分配响应缓冲区 */
    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int len;
    if (ret == ESP_OK) {
        len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
            "{"
            "\"success\": true,"
            "\"bclk\": %d,"
            "\"ws\": %d,"
            "\"din\": %d,"
            "\"gain\": %d,"
            "\"gain_enable\": %s,"
            "\"sd\": %d,"
            "\"sd_enable\": %s"
            "}",
            pin_config.bclk,
            pin_config.ws,
            pin_config.din,
            pin_config.gain,
            pin_config.gain_enable ? "true" : "false",
            pin_config.sd,
            pin_config.sd_enable ? "true" : "false"
        );
    } else {
        len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
            "{\"success\": false, \"error\": \"Failed to get pin config\"}");
    }

    ret = httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        ret = httpd_resp_send(req, json_str, len);
    }

    free(json_str);
    return ret;
}

esp_err_t audio_web_api_set_gain(httpd_req_t *req)
{
    /* 解析请求体 */
    char *body = parse_post_body(req);
    if (body == NULL) {
        return send_error_response(req, "Invalid request body");
    }

    json_parse_result_t result;
    if (!json_parse_int(body, "gain", &result) || !result.is_valid) {
        free(body);
        return send_error_response(req, "Missing 'gain' parameter");
    }

    int gain_value = result.int_val;
    audio_gain_t gain;

    switch (gain_value) {
        case 3:  gain = AUDIO_GAIN_3DB; break;
        case 6:  gain = AUDIO_GAIN_6DB; break;
        case 9:  gain = AUDIO_GAIN_9DB; break;
        case 12: gain = AUDIO_GAIN_12DB; break;
        default:
            free(body);
            return send_error_response(req, "Invalid gain value (3, 6, 9, 12 allowed)");
    }

    free(body);

    esp_err_t ret = audio_set_gain(gain);

    /* 使用malloc分配响应缓冲区 */
    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
        "{\"success\": %s, \"gain\": %d}",
        ret == ESP_OK ? "true" : "false",
        gain_value);

    ret = httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        ret = httpd_resp_send(req, json_str, len);
    }

    free(json_str);
    return ret;
}

esp_err_t audio_web_api_set_volume(httpd_req_t *req)
{
    /* 解析请求体 */
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

    esp_err_t ret = audio_set_volume((uint8_t)volume);

    /* 使用malloc分配响应缓冲区 */
    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    audio_gain_t current_gain = audio_get_gain();
    int gain_db = (current_gain == AUDIO_GAIN_3DB) ? 3 :
                  (current_gain == AUDIO_GAIN_6DB) ? 6 :
                  (current_gain == AUDIO_GAIN_9DB) ? 9 : 12;

    int len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
        "{\"success\": %s, \"volume\": %d, \"gain_db\": %d}",
        ret == ESP_OK ? "true" : "false",
        volume,
        gain_db);

    ret = httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        ret = httpd_resp_send(req, json_str, len);
    }

    free(json_str);
    return ret;
}

esp_err_t audio_web_api_test(httpd_req_t *req)
{
    if (!audio_is_initialized()) {
        return send_error_response(req, "Audio module not initialized");
    }

    /* 生成测试音调 - 1kHz正弦波，1秒 */
    /* 44.1kHz采样率，16位单声道 */
    /* 使用malloc分配缓冲区 */
    uint32_t sample_rate = 44100;
    uint32_t duration_sec = 1;
    uint32_t num_samples = sample_rate * duration_sec;
    size_t buffer_size = num_samples * sizeof(int16_t);

    int16_t *tone_buffer = (int16_t*)malloc(buffer_size);
    if (tone_buffer == NULL) {
        return send_error_response(req, "Memory allocation failed");
    }

    /* 生成1kHz正弦波 */
    ESP_LOGI(TAG, "生成1kHz测试音调...");
    float frequency = 1000.0f;
    for (uint32_t i = 0; i < num_samples; i++) {
        float sample = sinf(2.0f * 3.1415926f * frequency * i / sample_rate);
        tone_buffer[i] = (int16_t)(sample * 16000);  /* 约50%幅度 */
    }

    /* 配置播放参数 */
    audio_play_config_t config = {
        .sample_rate = AUDIO_SAMPLE_RATE_44K,
        .format = AUDIO_FMT_16BIT,
        .channel = 1,
        .gain = AUDIO_GAIN_9DB
    };

    /* 播放 */
    esp_err_t ret = audio_play_data((const uint8_t*)tone_buffer, buffer_size, &config);

    free(tone_buffer);

    /* 使用malloc分配响应缓冲区 */
    char *json_str = (char*)malloc(AUDIO_WEB_JSON_SIZE);
    if (json_str == NULL) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int len = snprintf(json_str, AUDIO_WEB_JSON_SIZE,
        "{\"success\": %s, \"message\": \"%s\", \"frequency\": 1000}",
        ret == ESP_OK ? "true" : "false",
        ret == ESP_OK ? "Playing 1kHz test tone" : "Playback failed");

    ret = httpd_resp_set_type(req, "application/json");
    if (ret == ESP_OK) {
        ret = httpd_resp_send(req, json_str, len);
    }

    free(json_str);
    return ret;
}

/* ========================================
 * 路由注册
 * ======================================== */

/* URI处理器定义 */
static const httpd_uri_t audio_status_uri = {
    .uri = "/api/audio/status",
    .method = HTTP_GET,
    .handler = audio_web_api_get_status,
    .user_ctx = NULL
};

static const httpd_uri_t audio_play_uri = {
    .uri = "/api/audio/play",
    .method = HTTP_POST,
    .handler = audio_web_api_play,
    .user_ctx = NULL
};

static const httpd_uri_t audio_stop_uri = {
    .uri = "/api/audio/stop",
    .method = HTTP_POST,
    .handler = audio_web_api_stop,
    .user_ctx = NULL
};

static const httpd_uri_t audio_pins_set_uri = {
    .uri = "/api/audio/pins",
    .method = HTTP_POST,
    .handler = audio_web_api_set_pins,
    .user_ctx = NULL
};

static const httpd_uri_t audio_pins_get_uri = {
    .uri = "/api/audio/pins",
    .method = HTTP_GET,
    .handler = audio_web_api_get_pins,
    .user_ctx = NULL
};

static const httpd_uri_t audio_gain_uri = {
    .uri = "/api/audio/gain",
    .method = HTTP_POST,
    .handler = audio_web_api_set_gain,
    .user_ctx = NULL
};

static const httpd_uri_t audio_volume_uri = {
    .uri = "/api/audio/volume",
    .method = HTTP_POST,
    .handler = audio_web_api_set_volume,
    .user_ctx = NULL
};

static const httpd_uri_t audio_test_uri = {
    .uri = "/api/audio/test",
    .method = HTTP_POST,
    .handler = audio_web_api_test,
    .user_ctx = NULL
};

esp_err_t audio_web_register_routes(void)
{
    httpd_handle_t server = get_httpd_handle();
    if (server == NULL) {
        ESP_LOGE(TAG, "HTTP服务器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    /* 注册所有路由 */
    esp_err_t ret;

    ret = httpd_register_uri_handler(server, &audio_status_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 /api/audio/status 失败");
        return ret;
    }

    ret = httpd_register_uri_handler(server, &audio_play_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 /api/audio/play 失败");
        return ret;
    }

    ret = httpd_register_uri_handler(server, &audio_stop_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 /api/audio/stop 失败");
        return ret;
    }

    ret = httpd_register_uri_handler(server, &audio_pins_set_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 /api/audio/pins (POST) 失败");
        return ret;
    }

    ret = httpd_register_uri_handler(server, &audio_pins_get_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 /api/audio/pins (GET) 失败");
        return ret;
    }

    ret = httpd_register_uri_handler(server, &audio_gain_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 /api/audio/gain 失败");
        return ret;
    }

    ret = httpd_register_uri_handler(server, &audio_volume_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 /api/audio/volume 失败");
        return ret;
    }

    ret = httpd_register_uri_handler(server, &audio_test_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册 /api/audio/test 失败");
        return ret;
    }

    ESP_LOGI(TAG, "音频Web路由注册完成");
    ESP_LOGI(TAG, "可用API:");
    ESP_LOGI(TAG, "  GET  /api/audio/status  - 获取状态");
    ESP_LOGI(TAG, "  POST /api/audio/play    - 播放WAV文件");
    ESP_LOGI(TAG, "  POST /api/audio/stop    - 停止播放");
    ESP_LOGI(TAG, "  GET  /api/audio/pins   - 获取引脚配置");
    ESP_LOGI(TAG, "  POST /api/audio/pins   - 设置引脚配置");
    ESP_LOGI(TAG, "  POST /api/audio/gain    - 设置增益");
    ESP_LOGI(TAG, "  POST /api/audio/volume  - 设置音量");
    ESP_LOGI(TAG, "  POST /api/audio/test    - 播放测试音调");

    return ESP_OK;
}

/* ========================================
 * 初始化/反初始化
 * ======================================== */

esp_err_t audio_web_init(void)
{
    ESP_LOGI(TAG, "音频Web模块初始化");
    return audio_web_register_routes();
}

esp_err_t audio_web_deinit(void)
{
    ESP_LOGI(TAG, "音频Web模块反初始化");
    return ESP_OK;
}

esp_err_t audio_web_get_status_json(char *json_str, size_t max_len)
{
    if (json_str == NULL || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    audio_state_t state = audio_get_state();
    audio_gain_t gain = audio_get_gain();

    const char *state_str;
    switch (state) {
        case AUDIO_STATE_UNINIT:   state_str = "uninit"; break;
        case AUDIO_STATE_READY:    state_str = "ready"; break;
        case AUDIO_STATE_PLAYING:  state_str = "playing"; break;
        case AUDIO_STATE_PAUSED:   state_str = "paused"; break;
        case AUDIO_STATE_ERROR:    state_str = "error"; break;
        default:                    state_str = "unknown"; break;
    }

    int gain_db = (gain == AUDIO_GAIN_3DB) ? 3 :
                  (gain == AUDIO_GAIN_6DB) ? 6 :
                  (gain == AUDIO_GAIN_9DB) ? 9 : 12;

    int len = snprintf(json_str, max_len,
        "{"
        "\"initialized\": %s,"
        "\"state\": \"%s\","
        "\"gain\": %d,"
        "\"gain_db\": %d"
        "}",
        audio_is_initialized() ? "true" : "false",
        state_str,
        gain,
        gain_db
    );

    return (len > 0 && (size_t)len < max_len) ? ESP_OK : ESP_ERR_NO_MEM;
}
