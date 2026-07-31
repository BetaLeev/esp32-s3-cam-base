/**
 * @file actuators_web.c
 * @brief 执行器Web API实现
 */

#include "actuators_web.h"
#include "../config.h"
#include "../actuators/actuators.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ACTUATORS_WEB";

/**
 * @brief 解析URL查询参数
 */
static esp_err_t parse_query_param(httpd_req_t *req, const char *param_name, int *out_value)
{
    char query_buf[128];
    char *query = query_buf;
    size_t query_len = httpd_req_get_url_query_len(req) + 1;

    if (query_len > sizeof(query_buf)) {
        query = malloc(query_len);
        if (query == NULL) return ESP_ERR_NO_MEM;
    }

    if (httpd_req_get_url_query_str(req, query, query_len) == ESP_OK) {
        char param_buf[64];
        if (httpd_query_key_value(query, param_name, param_buf, sizeof(param_buf)) == ESP_OK) {
            *out_value = atoi(param_buf);
            if (query != query_buf) free(query);
            return ESP_OK;
        }
    }

    if (query != query_buf) free(query);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t actuators_web_servo_handler(httpd_req_t *req)
{
    char response[128];
    int angle = -1;

    if (parse_query_param(req, "angle", &angle) == ESP_OK) {
        if (angle >= 0 && angle <= 180) {
            esp_err_t ret = actuators_servo_set_angle((uint8_t)angle);
            if (ret == ESP_OK) {
                snprintf(response, sizeof(response), "{\"success\":true,\"angle\":%d}", angle);
            } else {
                snprintf(response, sizeof(response), "{\"success\":false,\"error\":\"set failed\"}");
            }
        } else {
            snprintf(response, sizeof(response), "{\"success\":false,\"error\":\"invalid angle (0-180)\"}");
        }
    } else {
        angle = actuators_servo_get_angle();
        snprintf(response, sizeof(response), "{\"success\":true,\"angle\":%d}", angle);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

esp_err_t actuators_web_motor_handler(httpd_req_t *req)
{
    char response[128];
    char query_buf[256];
    char *query = query_buf;
    size_t query_len = httpd_req_get_url_query_len(req) + 1;

    if (query_len > sizeof(query_buf)) {
        query = malloc(query_len);
        if (query == NULL) {
            snprintf(response, sizeof(response), "{\"success\":false,\"error\":\"memory error\"}");
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
            return ESP_ERR_NO_MEM;
        }
    }

    esp_err_t ret = httpd_req_get_url_query_str(req, query, query_len);
    if (ret == ESP_OK) {
        char cmd_buf[16];
        if (httpd_query_key_value(query, "cmd", cmd_buf, sizeof(cmd_buf)) == ESP_OK) {
            if (strcmp(cmd_buf, "start") == 0) {
                int speed = -1;
                if (parse_query_param(req, "speed", &speed) == ESP_OK && speed >= 0 && speed <= 100) {
                    actuators_motor_set_speed((uint8_t)speed);
                }
                ret = actuators_motor_start();
            } else if (strcmp(cmd_buf, "stop") == 0) {
                ret = actuators_motor_stop();
            } else if (strcmp(cmd_buf, "speed") == 0) {
                int speed = -1;
                if (parse_query_param(req, "speed", &speed) == ESP_OK && speed >= 0 && speed <= 100) {
                    ret = actuators_motor_set_speed((uint8_t)speed);
                } else {
                    speed = actuators_motor_get_speed();
                    snprintf(response, sizeof(response),
                        "{\"success\":true,\"state\":%s,\"speed\":%d}",
                        actuators_motor_get_state() ? "true" : "false", speed);
                    httpd_resp_set_type(req, "application/json");
                    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
                    if (query != query_buf) free(query);
                    return ESP_OK;
                }
            } else {
                ret = ESP_ERR_INVALID_ARG;
            }

            if (ret == ESP_OK) {
                snprintf(response, sizeof(response),
                    "{\"success\":true,\"state\":%s,\"speed\":%d}",
                    actuators_motor_get_state() ? "true" : "false",
                    actuators_motor_get_speed());
            } else {
                snprintf(response, sizeof(response), "{\"success\":false,\"error\":\"command failed\"}");
            }
        } else {
            snprintf(response, sizeof(response),
                "{\"success\":true,\"state\":%s,\"speed\":%d}",
                actuators_motor_get_state() ? "true" : "false",
                actuators_motor_get_speed());
        }
    } else {
        snprintf(response, sizeof(response),
            "{\"success\":true,\"state\":%s,\"speed\":%d}",
            actuators_motor_get_state() ? "true" : "false",
            actuators_motor_get_speed());
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    if (query != query_buf) free(query);

    return ESP_OK;
}
