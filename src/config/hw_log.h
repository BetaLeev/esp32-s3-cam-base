/**
 * @file hw_log.h
 * @brief 日志配置 - 各模块日志级别开关
 *
 * 使用方法：
 * 1. 设置模块的日志级别为 LOG_LEVEL_NONE 可禁用该模块日志
 * 2. 设置模块的日志级别为 LOG_LEVEL_DEBUG/INFO/WARN/ERROR 可启用对应级别日志
 *
 * 示例：
 *   #define LOG_LEVEL_SDCARD    LOG_LEVEL_DEBUG  // 启用TF卡DEBUG级别日志
 *   #define LOG_LEVEL_SENSORS   LOG_LEVEL_NONE   // 禁用传感器日志
 */

#ifndef HW_LOG_H
#define HW_LOG_H

#include "esp_log.h"

/* ========================================
 * 日志级别定义
 * 注意：必须使用 #define 而不是 enum，因为 C 预处理器 #if 无法识别枚举类型
 * ======================================== */
#define LOG_LEVEL_NONE  0    /**< 禁用日志 */
#define LOG_LEVEL_ERROR 1    /**< 仅错误 */
#define LOG_LEVEL_WARN  2    /**< 警告 + 错误 */
#define LOG_LEVEL_INFO  3    /**< 信息 + 警告 + 错误 */
#define LOG_LEVEL_DEBUG 4    /**< 调试 + 信息 + 警告 + 错误 */

/* ========================================
 * 各模块日志级别配置
 * ======================================== *
 * 将不需要的模块设为 LOG_LEVEL_NONE 即可禁用其日志
 *
 * 调试指南：
 *   - 调试TF卡: 设置 LOG_LEVEL_SDCARD = LOG_LEVEL_DEBUG
 *   - 调试传感器: 设置 LOG_LEVEL_SENSORS = LOG_LEVEL_INFO
 */

/** TF卡模块日志级别 */
#define LOG_LEVEL_SDCARD    LOG_LEVEL_INFO

/** 传感器模块日志级别 */
#define LOG_LEVEL_SENSORS   LOG_LEVEL_INFO

/** 执行器模块日志级别 */
#define LOG_LEVEL_ACTUATORS LOG_LEVEL_INFO

/** 音频模块日志级别 */
#define LOG_LEVEL_AUDIO     LOG_LEVEL_INFO

/** 视频模块日志级别 - 调试中 */
#define LOG_LEVEL_VIDEO     LOG_LEVEL_DEBUG

/** 视频WebSocket模块日志级别 */
#define LOG_LEVEL_VIDEO_WS  LOG_LEVEL_DEBUG

/** WiFi模块日志级别 */
#define LOG_LEVEL_WIFI      LOG_LEVEL_DEBUG

/** HTTP服务器日志级别 - 调试中 */
#define LOG_LEVEL_HTTP      LOG_LEVEL_DEBUG

/** Web模块日志级别 */
#define LOG_LEVEL_WEB       LOG_LEVEL_INFO

/** 主模块日志级别 - 调试中 */
#define LOG_LEVEL_MAIN      LOG_LEVEL_DEBUG

/** DNS模块日志级别 */
#define LOG_LEVEL_DNS       LOG_LEVEL_INFO

/** SPIFFS模块日志级别 */
#define LOG_LEVEL_SPIFFS    LOG_LEVEL_INFO

/** 脉冲控制模块日志级别 */
#define LOG_LEVEL_PULSE     LOG_LEVEL_INFO

/* ========================================
 * 日志宏定义 - 根据级别自动启用/禁用
 * ======================================== */

/* SDCARD 日志宏 */
#if LOG_LEVEL_SDCARD >= LOG_LEVEL_ERROR
    #define SDCARD_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define SDCARD_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_SDCARD >= LOG_LEVEL_WARN
    #define SDCARD_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define SDCARD_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_SDCARD >= LOG_LEVEL_INFO
    #define SDCARD_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define SDCARD_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_SDCARD >= LOG_LEVEL_DEBUG
    #define SDCARD_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define SDCARD_LOGD(tag, ...) do {} while(0)
#endif

/* SENSORS 日志宏 */
#if LOG_LEVEL_SENSORS >= LOG_LEVEL_ERROR
    #define SENSORS_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define SENSORS_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_SENSORS >= LOG_LEVEL_WARN
    #define SENSORS_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define SENSORS_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_SENSORS >= LOG_LEVEL_INFO
    #define SENSORS_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define SENSORS_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_SENSORS >= LOG_LEVEL_DEBUG
    #define SENSORS_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define SENSORS_LOGD(tag, ...) do {} while(0)
#endif

/* ACTUATORS 日志宏 */
#if LOG_LEVEL_ACTUATORS >= LOG_LEVEL_ERROR
    #define ACTUATORS_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define ACTUATORS_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_ACTUATORS >= LOG_LEVEL_WARN
    #define ACTUATORS_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define ACTUATORS_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_ACTUATORS >= LOG_LEVEL_INFO
    #define ACTUATORS_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define ACTUATORS_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_ACTUATORS >= LOG_LEVEL_DEBUG
    #define ACTUATORS_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define ACTUATORS_LOGD(tag, ...) do {} while(0)
#endif

/* AUDIO 日志宏 */
#if LOG_LEVEL_AUDIO >= LOG_LEVEL_ERROR
    #define AUDIO_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define AUDIO_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_AUDIO >= LOG_LEVEL_WARN
    #define AUDIO_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define AUDIO_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_AUDIO >= LOG_LEVEL_INFO
    #define AUDIO_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define AUDIO_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_AUDIO >= LOG_LEVEL_DEBUG
    #define AUDIO_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define AUDIO_LOGD(tag, ...) do {} while(0)
#endif

/* VIDEO 日志宏 */
#if LOG_LEVEL_VIDEO >= LOG_LEVEL_ERROR
    #define VIDEO_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define VIDEO_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_VIDEO >= LOG_LEVEL_WARN
    #define VIDEO_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define VIDEO_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_VIDEO >= LOG_LEVEL_INFO
    #define VIDEO_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define VIDEO_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_VIDEO >= LOG_LEVEL_DEBUG
    #define VIDEO_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define VIDEO_LOGD(tag, ...) do {} while(0)
#endif

/* VIDEO_WS 日志宏 */
#if LOG_LEVEL_VIDEO_WS >= LOG_LEVEL_ERROR
    #define VIDEO_WS_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define VIDEO_WS_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_VIDEO_WS >= LOG_LEVEL_WARN
    #define VIDEO_WS_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define VIDEO_WS_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_VIDEO_WS >= LOG_LEVEL_INFO
    #define VIDEO_WS_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define VIDEO_WS_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_VIDEO_WS >= LOG_LEVEL_DEBUG
    #define VIDEO_WS_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define VIDEO_WS_LOGD(tag, ...) do {} while(0)
#endif

/* WIFI 日志宏 */
#if LOG_LEVEL_WIFI >= LOG_LEVEL_ERROR
    #define WIFI_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define WIFI_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_WIFI >= LOG_LEVEL_WARN
    #define WIFI_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define WIFI_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_WIFI >= LOG_LEVEL_INFO
    #define WIFI_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define WIFI_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_WIFI >= LOG_LEVEL_DEBUG
    #define WIFI_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define WIFI_LOGD(tag, ...) do {} while(0)
#endif

/* HTTP 日志宏 */
#if LOG_LEVEL_HTTP >= LOG_LEVEL_ERROR
    #define HTTP_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define HTTP_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_HTTP >= LOG_LEVEL_WARN
    #define HTTP_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define HTTP_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_HTTP >= LOG_LEVEL_INFO
    #define HTTP_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define HTTP_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_HTTP >= LOG_LEVEL_DEBUG
    #define HTTP_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define HTTP_LOGD(tag, ...) do {} while(0)
#endif

/* WEB 日志宏 */
#if LOG_LEVEL_WEB >= LOG_LEVEL_ERROR
    #define WEB_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define WEB_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_WEB >= LOG_LEVEL_WARN
    #define WEB_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define WEB_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_WEB >= LOG_LEVEL_INFO
    #define WEB_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define WEB_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_WEB >= LOG_LEVEL_DEBUG
    #define WEB_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define WEB_LOGD(tag, ...) do {} while(0)
#endif

/* MAIN 日志宏 */
#if LOG_LEVEL_MAIN >= LOG_LEVEL_ERROR
    #define MAIN_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define MAIN_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_MAIN >= LOG_LEVEL_WARN
    #define MAIN_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define MAIN_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_MAIN >= LOG_LEVEL_INFO
    #define MAIN_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define MAIN_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_MAIN >= LOG_LEVEL_DEBUG
    #define MAIN_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define MAIN_LOGD(tag, ...) do {} while(0)
#endif

/* DNS 日志宏 */
#if LOG_LEVEL_DNS >= LOG_LEVEL_ERROR
    #define DNS_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define DNS_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_DNS >= LOG_LEVEL_WARN
    #define DNS_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define DNS_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_DNS >= LOG_LEVEL_INFO
    #define DNS_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define DNS_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_DNS >= LOG_LEVEL_DEBUG
    #define DNS_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define DNS_LOGD(tag, ...) do {} while(0)
#endif

/* SPIFFS 日志宏 */
#if LOG_LEVEL_SPIFFS >= LOG_LEVEL_ERROR
    #define SPIFFS_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define SPIFFS_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_SPIFFS >= LOG_LEVEL_WARN
    #define SPIFFS_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define SPIFFS_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_SPIFFS >= LOG_LEVEL_INFO
    #define SPIFFS_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define SPIFFS_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_SPIFFS >= LOG_LEVEL_DEBUG
    #define SPIFFS_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define SPIFFS_LOGD(tag, ...) do {} while(0)
#endif

/* SDCARD_WEB 日志宏 - 复用SDCARD的日志级别 */
#if LOG_LEVEL_SDCARD >= LOG_LEVEL_ERROR
    #define SDCARD_WEB_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define SDCARD_WEB_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_SDCARD >= LOG_LEVEL_WARN
    #define SDCARD_WEB_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define SDCARD_WEB_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_SDCARD >= LOG_LEVEL_INFO
    #define SDCARD_WEB_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define SDCARD_WEB_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_SDCARD >= LOG_LEVEL_DEBUG
    #define SDCARD_WEB_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define SDCARD_WEB_LOGD(tag, ...) do {} while(0)
#endif

/* PULSE 日志宏 */
#if LOG_LEVEL_PULSE >= LOG_LEVEL_ERROR
    #define PULSE_LOGE(tag, ...) ESP_LOGE(tag, __VA_ARGS__)
#else
    #define PULSE_LOGE(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_PULSE >= LOG_LEVEL_WARN
    #define PULSE_LOGW(tag, ...) ESP_LOGW(tag, __VA_ARGS__)
#else
    #define PULSE_LOGW(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_PULSE >= LOG_LEVEL_INFO
    #define PULSE_LOGI(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#else
    #define PULSE_LOGI(tag, ...) do {} while(0)
#endif

#if LOG_LEVEL_PULSE >= LOG_LEVEL_DEBUG
    #define PULSE_LOGD(tag, ...) ESP_LOGD(tag, __VA_ARGS__)
#else
    #define PULSE_LOGD(tag, ...) do {} while(0)
#endif

#endif /* HW_LOG_H */
