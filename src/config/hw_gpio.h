/**
 * @file hw_gpio.h
 * @brief GPIO 引脚配置
 */
#ifndef HW_GPIO_H
#define HW_GPIO_H

#include "driver/gpio.h"

/* ========================================
 * 电机驱动 (TB6612) 引脚
 * ======================================== */
/**
 * @brief TB6612 电机驱动引脚
 * - STBY 引脚已硬件连接到 3.3V
 */
#define GPIO_MOTOR_PWMA GPIO_NUM_NC
#define GPIO_MOTOR_AIN1 GPIO_NUM_NC
#define GPIO_MOTOR_AIN2 GPIO_NUM_NC

/* ========================================
 * 传感器引脚
 * ⚠️ 注意：原 GPIO4/5/7 与摄像头 SCCB/DVP 冲突，已重定位
 *    摄像头占用: GPIO4=SDA, GPIO5=SCL, GPIO6=VSYNC, GPIO7=HREF,
 *                GPIO8=D2, GPIO9=D1, GPIO10=D3, GPIO11=D0, GPIO12=D4,
 *                GPIO13=PCLK, GPIO15=XCLK, GPIO16=D7, GPIO17=D6, GPIO18=D5
 *    电机占用: GPIO1=PWMA, GPIO2=AIN1, GPIO42=AIN2
 *
 * ⚠️⚠️ 致命警告：ESP32-S3 八线 PSRAM 模式 (CONFIG_SPIRAM_MODE_OCT=y)
 *    占用 GPIO33-37，这些引脚绝对不能用于任何其他功能！
 *    GPIO33=SPIIO4, GPIO34=SPIIO5, GPIO35=SPIIO6, GPIO36=SPIIO7, GPIO37=SPIDQS
 *    曾错误地将 DHT11 分配到 GPIO33，导致 PSRAM 损坏，esp_netif_init() 挂死。
 * ======================================== */
/**
 * @brief DHT11 温湿度传感器 (单总线) - 不需要ADC
 * - 原 GPIO4 → 摄像头 SCCB SDA 冲突
 * - 原 GPIO33 → ❌ PSRAM 引脚冲突！(已修复)
 * - 现 GPIO45 → 空闲普通GPIO (strapping 引脚，启动后可用)
 *   注意：若硬件未连接 DHT11，读取会超时但不影响系统运行
 */
#define GPIO_DHT11 GPIO_NUM_NC

/**
 * @brief ADC 传感器
 * - 可用 ADC1 通道仅剩 GPIO3 (ADC1_CH2)
 * - 热敏电阻: 原 GPIO5 (SCL冲突) → 改为 GPIO3 (ADC1_CH2)  [与光敏只能二选一，建议硬件调整]
 * - 光敏电阻: 原 GPIO7 (HREF冲突) → 暂时复用 GPIO3，未接线则读取值无意义
 *   TODO: 如果实际硬件同时焊接了热敏和光敏，需要重设计引脚或使用ADC2
 */
#define GPIO_ADC_THERMISTOR GPIO_NUM_3  /**< 热敏电阻 - ADC1_CH2 */
#define GPIO_ADC_PHOTOSENSOR GPIO_NUM_3 /**< 光敏电阻 - 暂与热敏复用，接线以实际为准 */

/**
 * @brief 土壤湿度传感器 (LM393)
 * - 使用 GPIO3 (ADC1_CH2)，与热敏电阻共用引脚
 * - 湿度越高，ADC 值越低
 */
#define GPIO_SOILHUMIDITY GPIO_NUM_3 /**< 土壤湿度传感器 - ADC1_CH2 */

/* ========================================
 * 执行器引脚
 * ======================================== */
/**
 * @brief 舵机控制引脚 (SG90) - GPIO48
 * GPIO48 是可用的引脚
 */
#define GPIO_SERVO GPIO_NUM_NC

/**
 * @brief 执行器可用GPIO列表（供动态配置使用）
 * 注意：LED/脉冲/水泵需要选择空闲GPIO
 * 已占用: 1(电机PWMA), 2(电机AIN1), 3(ADC), 4-18(摄像头), 38-42(TF卡/电机), 45(DHT11), 46-48(其他)
 * 可用: 0, 19,  22
 */
#define ACTUATOR_AVAILABLE_GPIO {0, 19, 22}

/* ========================================
 * TF 卡 (SD卡) 引脚 - SDMMC 1-bit 模式
 *
 * ⚠️ 重要：这些是板载标准引脚，请勿随意修改！
 * ESP32-S3-CAM 板载TF卡使用以下引脚：
 *   - CLK:  GPIO39  (时钟信号)
 *   - CMD:  GPIO38  (命令/响应信号)
 *   - D0:   GPIO40  (数据信号)
 *
 * 如果修改这些引脚，TF卡将无法正常工作！
 * ======================================== */
#define GPIO_SD_CLK GPIO_NUM_39
#define GPIO_SD_CMD GPIO_NUM_38
#define GPIO_SD_D0 GPIO_NUM_40

/* ========================================
 * I2C 引脚 (预留 - OLED, IMU 等)
 * ⚠️ 原 GPIO42 与电机 AIN2 冲突，已改为 GPIO46
 * ======================================== */
#define GPIO_I2C_SCL GPIO_NUM_NC
#define GPIO_I2C_SDA GPIO_NUM_46

/* ========================================
 * SPI 引脚 (预留 - LCD, 射频模块等)
 * ⚠️⚠️ 原 GPIO34-37 与八线 PSRAM 冲突，已全部禁用！
 *    ESP32-S3 PSRAM 八线模式占用 GPIO33-37，不可用于 SPI。
 *    如需使用 SPI，请选择其他空闲 GPIO (如 GPIO0, GPIO14, GPIO19, GPIO20)
 *    注意 GPIO14/19/20 已被音频模块占用。
 * ======================================== */
/* #define GPIO_SPI_CLK           GPIO_NUM_36 */ /* ❌ PSRAM 冲突 */
/* #define GPIO_SPI_MOSI          GPIO_NUM_35 */ /* ❌ PSRAM 冲突 */
/* #define GPIO_SPI_MISO          GPIO_NUM_37 */ /* ❌ PSRAM 冲突 */
/* #define GPIO_SPI_CS            GPIO_NUM_34 */ /* ❌ PSRAM 冲突 */

/* ========================================
 * 扩展引脚 (预留)
 * ⚠️ 原 GPIO6/7/8/9 已被摄像头 DVP 总线占用，不可作为扩展引脚使用
 *    GPIO6=VSYNC, GPIO7=HREF, GPIO8=D2, GPIO9=D1
 *    这些引脚如需扩展，请使用其他空闲 GPIO (如 GPIO0, GPIO45, GPIO46, GPIO47)
 * ======================================== */
#define GPIO_EXT_1 GPIO_NUM_6 /**< ⚠️ 已被摄像头 VSYNC 占用 */
#define GPIO_EXT_2 GPIO_NUM_7 /**< ⚠️ 已被摄像头 HREF 占用 */
#define GPIO_EXT_3 GPIO_NUM_8 /**< ⚠️ 已被摄像头 D2 占用 */
#define GPIO_EXT_4 GPIO_NUM_9 /**< ⚠️ 已被摄像头 D1 占用 */


/* ========================================
 * OLED 屏幕 I2C 引脚 (采用右侧同侧相邻安全引脚)
 * ======================================== */
#define GPIO_OLED_SCL GPIO_NUM_47 /**< I2C 时钟线 SCL */
#define GPIO_OLED_SDA GPIO_NUM_21 /**< I2C 数据线 SDA */
#define OLED_I2C_ADDRESS 0x3C     /**< 常见 0.96 OLED I2C 默认地址 */

#endif /* HW_GPIO_H */
