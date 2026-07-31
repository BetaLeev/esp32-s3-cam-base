/**
 * @file hw_system.h
 * @brief 系统级参数配置
 */
#ifndef HW_SYSTEM_H
#define HW_SYSTEM_H

#include <stdint.h>

/* ========================================
 * 默认启动参数
 * ======================================== */
#define DEFAULT_PUMP_GEAR        0           /**< 水泵默认档位 0=关闭 */
#define DEFAULT_SERVO_ANGLE       90          /**< 舵机默认角度 */
#define DEFAULT_SENSOR_INTERVAL_MS 2000       /**< 传感器读取间隔 */

/* ========================================
 * 执行器范围限制
 * ======================================== */
#define PUMP_SPEED_MIN           20
#define PUMP_SPEED_MAX           100         /**< 水泵速度 0-100% */

#define SERVO_ANGLE_MIN          0
#define SERVO_ANGLE_MAX          180         /**< 舵机角度 0-180° */

/* ========================================
 * DHT11 配置
 * ======================================== */
#define DHT11_RETRY_COUNT        15          /**< 最大重试次数 */
#define DHT11_RETRY_INTERVAL_MS  100         /**< 重试间隔 */
#define DHT11_TIMEOUT_US         10000       /**< 超时时间 */

/* ========================================
 * 设备标识
 * ======================================== */
#define DEVICE_NAME              "ESP32-Sensor"
#define DEVICE_MODEL             "ESP32-S3-CAM"
#define DEVICE_MANUFACTURER      "DIY"

#define DEVICE_FIRMWARE_VERSION  "1.0.0"
#define DEVICE_HARDWARE_VERSION  "1.0"

/* ========================================
 * 看门狗配置
 * ======================================== */
#define WDT_TIMEOUT_S            10          /**< 看门狗超时 10秒 */

/* ========================================
 * 内存管理
 * ======================================== */
#define HEAP_MIN_FREE_BYTES      8192        /**< 最小可用内存警告阈值 */

/* ========================================
 * 调试配置
 * ======================================== */
#define DEBUG_ENABLED            1
#define DEBUG_LOG_LEVEL          3           /**< 0=无 1=错误 2=警告 3=信息 4=调试 */

#endif /* HW_SYSTEM_H */
