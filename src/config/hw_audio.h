/**
 * @file hw_audio.h
 * @brief MAX98357 音频模块 GPIO 引脚配置
 *
 * ESP32-S3 的 I2S 引脚可以映射到多个 GPIO
 * 默认使用 GPIO2/21/47 作为 I2S 引脚
 */
#ifndef HW_AUDIO_H
#define HW_AUDIO_H

#include "driver/gpio.h"

/* ========================================
 * MAX98357 I2S 接口引脚配置
 * ⚠️ 重新映射到完全空闲的GPIO，避免与所有外设冲突
 *    摄像头占用: GPIO4=SDA, GPIO5=SCL, GPIO6=VSYNC, GPIO7=HREF,
 *                GPIO8=D2, GPIO9=D1, GPIO10=D3, GPIO11=D0, GPIO12=D4,
 *                GPIO13=PCLK, GPIO15=XCLK, GPIO16=D7, GPIO17=D6, GPIO18=D5
 *    TF卡占用:   GPIO38=CMD, GPIO39=CLK, GPIO40=D0
 *    电机占用:   GPIO1=PWMA, GPIO2=AIN1, GPIO42=AIN2
 *    传感器占用: GPIO3=ADC, GPIO45=DHT11
 *    舵机占用:   GPIO22
 *    I2C占用:    GPIO41=SCL, GPIO46=SDA
 * ======================================== */
/**
 * @brief MAX98357 I2S 接口引脚（使用完全空闲的GPIO）
 * - BCLK: GPIO2
 * - WS(LRC): GPIO21
 * - DIN: GPIO47
 * ⚠️ GPIO35-37 是 PSRAM 引脚（不可用），GPIO38-40 是 TF 卡引脚（不可用）
 */
#define GPIO_AUDIO_BCLK       GPIO_NUM_2    /**< 位时钟引脚 - 空闲GPIO */
#define GPIO_AUDIO_WS         GPIO_NUM_21   /**< 字选择引脚 (LRC) - 空闲GPIO */
#define GPIO_AUDIO_DIN        GPIO_NUM_47   /**< 串行数据输入引脚 - 空闲GPIO */
#define GPIO_AUDIO_SD         GPIO_NUM_46   /**< SD (Shutdown) 引脚 - 必须拉高才能工作 */

/* ========================================
 * 默认引脚映射
 * ======================================== */
/**
 * @brief 使用默认引脚时的宏定义
 */
#define AUDIO_DEFAULT_BCLK    GPIO_AUDIO_BCLK
#define AUDIO_DEFAULT_WS      GPIO_AUDIO_WS
#define AUDIO_DEFAULT_DIN     GPIO_AUDIO_DIN
#define AUDIO_DEFAULT_GAIN    GPIO_NUM_45   /**< 增益控制引脚 - GPIO45 */
#define AUDIO_DEFAULT_SD      GPIO_AUDIO_SD /**< SD (Shutdown) 引脚 */

/* ========================================
 * I2S 配置参数
 * ======================================== */
/**
 * @brief I2S 默认采样率
 */
#define AUDIO_DEFAULT_SAMPLE_RATE  44100    /**< 44.1kHz CD音质 */

/**
 * @brief I2S 通道配置
 * 0 = I2S_NUM_0 (主I2S)
 * 1 = I2S_NUM_1 (从I2S)
 */
#define AUDIO_I2S_NUM             I2S_NUM_0

/**
 * @brief I2S 时钟配置
 */
#define AUDIO_I2S_MCK_MULT        256       /**< MCK = sample_rate * 256 */

#endif /* HW_AUDIO_H */
