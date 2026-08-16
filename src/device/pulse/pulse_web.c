/**
 * @file pulse_web.c
 * @brief 脉冲控制 Web API 实现
 */

#include "pulse_web.h"
#include "pulse.h"
#include "../../config.h"
#include "../../web/web_module.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "PULSE_WEB";

/**
 * @brief 安全获取查询参数中的字符串
 */
static bool get_query_param(httpd_req_t *req, const char *key, char *buf, size_t buf_len)
{
    char query_buf[512];
    char *query = query_buf;
    size_t query_len = httpd_req_get_url_query_len(req) + 1;

    if (query_len > sizeof(query_buf)) {
        query = malloc(query_len);
        if (query == NULL) return false;
    }

    bool found = false;
    if (httpd_req_get_url_query_str(req, query, query_len) == ESP_OK) {
        if (httpd_query_key_value(query, key, buf, buf_len) == ESP_OK) {
            found = true;
        }
    }

    if (query != query_buf) {
        free(query);
    }

    return found;
}

/**
 * @brief 安全获取查询参数中的整数值
 */
static bool get_query_int(httpd_req_t *req, const char *key, int *out_value)
{
    char buf[32];
    if (get_query_param(req, key, buf, sizeof(buf))) {
        *out_value = atoi(buf);
        return true;
    }
    return false;
}

esp_err_t pulse_web_handler(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    // 获取参数
    char action_buf[16] = {0};
    int pin = -1;
    int intensity = -1;
    int frequency = -1;
    int pulse_width = -1;
    char mode_buf[16] = {0};

    get_query_param(req, "action", action_buf, sizeof(action_buf));
    get_query_int(req, "pin", &pin);
    get_query_int(req, "intensity", &intensity);
    get_query_int(req, "frequency", &frequency);
    get_query_int(req, "pulse_width", &pulse_width);
    get_query_param(req, "mode", mode_buf, sizeof(mode_buf));

    // 处理动作
    if (action_buf[0] != '\0') {
        if (strcmp(action_buf, "stop") == 0) {
            esp_err_t ret = pulse_stop();
            if (ret != ESP_OK) {
                return send_error(req, "停止脉冲失败", HTTP_INTERNAL_ERROR);
            }
        } else if (strcmp(action_buf, "start") == 0) {
            // 验证必需参数
            if (pin < 0) {
                return send_bad_request(req, "缺少必需参数: pin");
            }

            // 设置默认值
            if (intensity < 0 || intensity > 100) {
                intensity = 50;
            }
            if (frequency < 1 || frequency > 1000) {
                frequency = 10;
            }
            if (pulse_width < 1 || pulse_width > 1000) {
                pulse_width = 100;
            }

            // 解析模式
            pulse_mode_t mode = PULSE_MODE_SINGLE;
            if (mode_buf[0] != '\0') {
                if (strcmp(mode_buf, "continuous") == 0) {
                    mode = PULSE_MODE_CONTINUOUS;
                }
            }

            esp_err_t ret = pulse_start(pin, mode, (uint8_t)intensity,
                                        (uint32_t)frequency, (uint32_t)pulse_width);
            if (ret != ESP_OK) {
                return send_error(req, "启动脉冲失败", HTTP_INTERNAL_ERROR);
            }
        }
    }

    // 构建响应数据
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return send_internal_error(req, "创建响应数据失败");
    }

    // 配置信息
    const pulse_config_t *config = pulse_get_config();
    cJSON_AddNumberToObject(data, "pin", config->pin);
    cJSON_AddBoolToObject(data, "enabled", config->enabled);
    cJSON_AddStringToObject(data, "mode", config->mode == PULSE_MODE_CONTINUOUS ? "continuous" : "single");
    cJSON_AddNumberToObject(data, "intensity", config->intensity);
    cJSON_AddNumberToObject(data, "frequency", config->frequency);
    cJSON_AddNumberToObject(data, "pulse_width", config->pulse_width);

    // 实时状态
    pulse_status_t status;
    pulse_get_status(&status);
    cJSON_AddNumberToObject(data, "current_intensity", status.current_intensity);
    cJSON_AddNumberToObject(data, "pulse_count", status.pulse_count);
    cJSON_AddNumberToObject(data, "elapsed_time", status.elapsed_time);
    cJSON_AddNumberToObject(data, "pin_level", status.pin_level);

    return send_success(req, data, "操作成功");
}

void pulse_web_register_routes(httpd_handle_t server)
{
    httpd_uri_t route = {
        .uri = "/api/pulse",
        .method = HTTP_GET,
        .handler = pulse_web_handler
    };

    esp_err_t ret = httpd_register_uri_handler(server, &route);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "注册路由失败: /api/pulse");
    } else {
        ESP_LOGI(TAG, "已注册路由: /api/pulse");
    }
}
