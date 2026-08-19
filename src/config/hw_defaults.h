/**
 * @file hw_defaults.h
 * @brief 默认值宏定义 - 所有配置的统一默认值来源
 *
 * 当配置文件缺失或无效时，使用此文件中的默认值
 */
#ifndef HW_DEFAULTS_H
#define HW_DEFAULTS_H

/* ========================================
 * Wi-Fi 默认值
 * ======================================== */
#define DEFAULT_AP_SSID          "myEsp"
#define DEFAULT_AP_PASSWORD      "88888888"
#define DEFAULT_AP_CHANNEL       1
#define DEFAULT_STA_SSID         ""
#define DEFAULT_STA_PASSWORD     ""
#define DEFAULT_STA_TIMEOUT_MS   30000

/* ========================================
 * GPIO 默认值
 * ======================================== */
#define DEFAULT_GPIO_MOTOR_PWMA      1
#define DEFAULT_GPIO_MOTOR_AIN1      -1
#define DEFAULT_GPIO_MOTOR_AIN2      -1
#define DEFAULT_GPIO_DHT11           4
#define DEFAULT_GPIO_SERVO           48
#define DEFAULT_GPIO_THERMISTOR      5
#define DEFAULT_GPIO_PHOTOSENSOR     7
#define DEFAULT_GPIO_SD_CLK          39
#define DEFAULT_GPIO_SD_CMD          38
#define DEFAULT_GPIO_SD_D0           40

/* ========================================
 * 系统默认值
 * ======================================== */
#define DEFAULT_PUMP_GEAR            0
#define DEFAULT_SERVO_ANGLE          90
#define DEFAULT_SENSOR_INTERVAL_MS   2000

/* ========================================
 * PWM 默认值
 * ======================================== */
#define DEFAULT_LEDC_FREQUENCY       1000
#define DEFAULT_LEDC_DUTY_RES        10          /**< 10位 = 0-1023 */
#define DEFAULT_SERVO_FREQUENCY      50

/* ========================================
 * 网络默认值
 * ======================================== */
#define DEFAULT_HTTP_PORT            80
#define DEFAULT_DNS_PORT             53
#define DEFAULT_HTTP_TIMEOUT_MS      5000

/* ========================================
 * 任务默认值
 * ======================================== */
#define DEFAULT_TASK_STACK_SIZE      2048
#define DEFAULT_TASK_PRIORITY        5

#endif /* HW_DEFAULTS_H */
