/**
 * @file hw_network.h
 * @brief 网络/HTTP 服务器配置
 */
#ifndef HW_NETWORK_H
#define HW_NETWORK_H

#include <stdint.h>

/* ========================================
 * HTTP 服务器配置
 * ======================================== */
#define HTTP_SERVER_PORT       80
#define HTTP_SERVER_STACK_SIZE 4096
#define HTTP_SERVER_PRIORITY   5
#define HTTP_SERVER_TASK_NAME  "http_server"

/* ========================================
 * DNS 服务器配置 (Captive Portal)
 * ======================================== */
#define DNS_SERVER_PORT        53
#define DNS_SERVER_STACK_SIZE  2048
#define DNS_SERVER_PRIORITY    4
#define DNS_SERVER_TASK_NAME   "dns_server"

/* ========================================
 * HTTP 请求超时
 * ======================================== */
#define HTTP_TIMEOUT_MS        5000
#define HTTP_KEEPALIVE_TIMEOUT 180

/* ========================================
 * CORS 配置
 * ======================================== */
#define HTTP_CORS_ENABLED      1
#define HTTP_CORS_ORIGIN        "*"

/* ========================================
 * API 版本
 * ======================================== */
#define API_VERSION            "v1.0.0"
#define API_BASE_PATH          "/api"

/* ========================================
 * OTA 更新配置 (预留)
 * ======================================== */
#define OTA_UPDATE_ENABLED     1
#define OTA_CHECK_URL         "http://update.example.com/check"
#define OTA_FIRMWARE_URL      "http://update.example.com/firmware.bin"

/* ========================================
 * MQTT 配置 (预留)
 * ======================================== */
#define MQTT_ENABLED           0
#define MQTT_BROKER_URL        "mqtt://broker.example.com:1883"
#define MQTT_CLIENT_ID         "esp32_sensor"
#define MQTT_USERNAME          ""
#define MQTT_PASSWORD          ""
#define MQTT_TOPIC_PREFIX      "esp32/"

/* ========================================
 * NTP 时间同步 (预留)
 * ======================================== */
#define NTP_ENABLED            0
#define NTP_SERVER_1          "pool.ntp.org"
#define NTP_SERVER_2          "time.nist.gov"
#define NTP_TIMEZONE_OFFSET     8           /**< UTC+8 北京时间 */
#define NTP_UPDATE_INTERVAL_MS 3600000    /**< 1小时同步一次 */

#endif /* HW_NETWORK_H */
