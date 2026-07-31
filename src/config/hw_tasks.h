/**
 * @file hw_tasks.h
 * @brief FreeRTOS 任务配置
 */
#ifndef HW_TASKS_H
#define HW_TASKS_H

#include <stdint.h>

/* ========================================
 * 传感器任务配置
 * ======================================== */
#define TASK_SENSOR_STACK_SIZE    2048
#define TASK_SENSOR_PRIORITY      5
#define TASK_SENSOR_NAME          "sensor_task"
#define TASK_SENSOR_CORE          1           /**< 绑定到核心1 */

#define TASK_SENSOR_READ_INTERVAL_MS  2000    /**< 传感器读取间隔 */

/* ========================================
 * Wi-Fi 任务配置
 * ======================================== */
#define TASK_WIFI_STACK_SIZE      4096
#define TASK_WIFI_PRIORITY        4
#define TASK_WIFI_NAME            "wifi_task"
#define TASK_WIFI_CORE            1

/* ========================================
 * HTTP 服务器任务配置
 * ======================================== */
#define TASK_HTTP_STACK_SIZE      4096
#define TASK_HTTP_PRIORITY        5
#define TASK_HTTP_NAME            "http_server"
#define TASK_HTTP_CORE            0

/* ========================================
 * DNS 服务器任务配置
 * ======================================== */
#define TASK_DNS_STACK_SIZE       2048
#define TASK_DNS_PRIORITY         4
#define TASK_DNS_NAME             "dns_server"
#define TASK_DNS_CORE             0

/* ========================================
 * 状态监控任务配置 (预留)
 * ======================================== */
#define TASK_MONITOR_STACK_SIZE   2048
#define TASK_MONITOR_PRIORITY     3
#define TASK_MONITOR_NAME         "system_monitor"
#define TASK_MONITOR_CORE         0
#define TASK_MONITOR_INTERVAL_MS  1000

/* ========================================
 * OTA 更新任务配置 (预留)
 * ======================================== */
#define TASK_OTA_STACK_SIZE       8192
#define TASK_OTA_PRIORITY         6
#define TASK_OTA_NAME             "ota_update"
#define TASK_OTA_CORE             0

/* ========================================
 * 任务通知配置
 * ======================================== */
#define TASK_NOTIFY_INDEX_SENSOR  0x01
#define TASK_NOTIFY_INDEX_WIFI   0x02
#define TASK_NOTIFY_INDEX_HTTP    0x04

#endif /* HW_TASKS_H */
