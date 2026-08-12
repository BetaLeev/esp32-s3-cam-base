/**
 * @file led_web.c
 * @brief LED Web API实现
 */

#include "led_web.h"
#include "led.h"
#include "../config.h"
#include "web_module.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "LED_WEB";

/* ========================================
 * 工具函数
 * ======================================== */

/**
 * @brief 获取字符串参数
 */
static esp_err_t get_string_param(httpd_req_t *req, const char *name, char *buf, size_t buf_len)
{
    char query_buf[256];
    size_t query_len = httpd_req_get_url_query_len(req) + 1;

    if (query_len > sizeof(query_buf)) {
        char *query = malloc(query_len);
        if (query == NULL) return ESP_ERR_NO_MEM;

        if (httpd_req_get_url_query_str(req, query, query_len) == ESP_OK) {
            if (httpd_query_key_value(query, name, buf, buf_len) == ESP_OK) {
                free(query);
                return ESP_OK;
            }
        }
        free(query);
    } else {
        if (httpd_req_get_url_query_str(req, query_buf, query_len) == ESP_OK) {
            if (httpd_query_key_value(query_buf, name, buf, buf_len) == ESP_OK) {
                return ESP_OK;
            }
        }
    }

    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief 获取整数参数
 */
static esp_err_t get_int_param(httpd_req_t *req, const char *name, int *out_value)
{
    char buf[32];
    if (get_string_param(req, name, buf, sizeof(buf)) == ESP_OK) {
        *out_value = atoi(buf);
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

/**
 * @brief 获取浮点数参数
 */
static esp_err_t get_float_param(httpd_req_t *req, const char *name, float *out_value)
{
    char buf[32];
    if (get_string_param(req, name, buf, sizeof(buf)) == ESP_OK) {
        *out_value = atof(buf);
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

/* ========================================
 * Web API Handler
 * ======================================== */

esp_err_t led_web_handler(httpd_req_t *req)
{
    char action_buf[16] = {0};

    // 获取action参数
    get_string_param(req, "action", action_buf, sizeof(action_buf));

    // 处理stop动作
    if (strcmp(action_buf, "stop") == 0) {
        esp_err_t ret = led_stop();
        if (ret == ESP_OK) {
            return send_success(req, NULL, "LED已停止");
        } else {
            return send_error(req, "停止LED失败", HTTPD_500_INTERNAL_SERVER_ERROR);
        }
    }

    // 解析配置参数
    led_config_t config;
    memset(&config, 0, sizeof(config));

    // 引脚
    int pin = -1;
    if (get_int_param(req, "pin", &pin) == ESP_OK && pin >= 0 && pin <= 39) {
        config.pin = (gpio_num_t)pin;
    } else {
        // 使用当前配置的引脚
        led_get_config(&config);
    }

    // 触发模式
    char mode_buf[16] = {0};
    if (get_string_param(req, "trigger_mode", mode_buf, sizeof(mode_buf)) == ESP_OK) {
        if (strcmp(mode_buf, "static") == 0) {
            config.mode = LED_MODE_STATIC;
        } else {
            config.mode = LED_MODE_BLINK;
        }
    } else {
        config.mode = LED_MODE_BLINK;
    }

    // 初始电平
    int initial_level = 1;
    if (get_int_param(req, "initial_level", &initial_level) == ESP_OK) {
        config.initial_level = (initial_level != 0) ? 1 : 0;
    } else {
        config.initial_level = 1;
    }

    // 高电平时长
    float high_duration = 1.0f;
    get_float_param(req, "high_duration", &high_duration);
    config.high_duration = high_duration > 0 ? high_duration : 1.0f;

    // 低电平时长
    float low_duration = 1.0f;
    get_float_param(req, "low_duration", &low_duration);
    config.low_duration = low_duration > 0 ? low_duration : 1.0f;

    // 重复次数
    int repeat_count = 3;
    get_int_param(req, "repeat_count", &repeat_count);
    config.repeat_count = repeat_count;

    // 处理config动作
    if (strcmp(action_buf, "config") == 0) {
        esp_err_t ret = led_configure(&config);
        if (ret == ESP_OK) {
            return send_success(req, NULL, "LED配置已保存");
        } else {
            return send_error(req, "配置LED失败", HTTPD_500_INTERNAL_SERVER_ERROR);
        }
    }

    // 处理start动作（或无action参数）
    esp_err_t ret = led_start(&config);
    if (ret != ESP_OK) {
        return send_error(req, "启动LED失败", HTTPD_500_INTERNAL_SERVER_ERROR);
    }

    // 获取状态并返回
    led_status_t status;
    led_get_status(&status);
    led_get_config(&config);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "pin", config.pin);
    cJSON_AddBoolToObject(data, "enabled", status.enabled);
    cJSON_AddStringToObject(data, "trigger_mode",
                            config.mode == LED_MODE_STATIC ? "static" : "blink");
    cJSON_AddNumberToObject(data, "initial_level", config.initial_level);
    cJSON_AddNumberToObject(data, "high_duration", config.high_duration);
    cJSON_AddNumberToObject(data, "low_duration", config.low_duration);
    cJSON_AddNumberToObject(data, "repeat_count", config.repeat_count);
    cJSON_AddNumberToObject(data, "current_level", status.current_level);
    cJSON_AddNumberToObject(data, "executed_count", status.executed_count);
    cJSON_AddNumberToObject(data, "total_count", status.total_count);
    cJSON_AddNumberToObject(data, "elapsed_time", status.elapsed_time);
    cJSON_AddNumberToObject(data, "remaining_time", status.remaining_time);

    return send_success(req, data, "LED控制成功");
}

esp_err_t gpio_used_web_handler(httpd_req_t *req)
{
    int pins[32];
    int count = led_get_used_pins(pins, 32);

    cJSON *data = cJSON_CreateObject();
    cJSON *pins_array = cJSON_CreateIntArray(pins, count);
    cJSON_AddItemToObject(data, "pins", pins_array);

    return send_success(req, data, "获取成功");
}

/* ========================================
 * 路由注册
 * ======================================== */

void led_web_register_routes(httpd_handle_t server)
{
    httpd_uri_t led_uri = {
        .uri = "/api/led",
        .method = HTTP_GET,
        .handler = led_web_handler,
        .user_ctx = NULL
    };

    httpd_uri_t gpio_used_uri = {
        .uri = "/api/gpio/used",
        .method = HTTP_GET,
        .handler = gpio_used_web_handler,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(server, &led_uri);
    httpd_register_uri_handler(server, &gpio_used_uri);

    ESP_LOGI(TAG, "LED路由已注册: /api/led, /api/gpio/used");
}
