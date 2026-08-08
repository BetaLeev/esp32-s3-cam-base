/**
 * @file hw_audio.h
 * @brief MAX98357 音频模块 GPIO 引脚配置
 *
 * ESP32-S3 的 I2S 引脚可以映射到多个 GPIO
 * 默认使用 GPIO14/15/16 作为 I2S 引脚
 */
#ifndef HW_AUDIO_H
#define HW_AUDIO_H

#include "driver/gpio.h"

/* ========================================
 * MAX98357 I2S 接口引脚配置
 * ⚠️ 注意：已从 GPIO6/7/8/9 移到空闲GPIO，避免与摄像头DVP/SCCB冲突
 *    (摄像头占用: GPIO4=SDA, GPIO5=SCL, GPIO6=VSYNC, GPIO7=HREF,
 *                 GPIO8=D2, GPIO9=D1, GPIO10=D3, GPIO11=D0, GPIO12=D4,
 *                 GPIO13=PCLK, GPIO15=XCLK, GPIO16=D7, GPIO17=D6, GPIO18=D5)
 * ======================================== */
/**
 * @brief MAX98357 I2S 接口引脚（重定位到空闲GPIO，避免与摄像头冲突）
 * - BCLK: GPIO14 (原GPIO6→VSYNC冲突)
 * - WS(LRC): GPIO20 (原GPIO7→HREF冲突，避免与GPIO3的ADC冲突)
 * - DIN: GPIO19    (原GPIO8→D2冲突)
 */
#define GPIO_AUDIO_BCLK       GPIO_NUM_14   /**< 位时钟引脚 - 移至GPIO14避免冲突 */
#define GPIO_AUDIO_WS         GPIO_NUM_20   /**< 字选择引脚 (LRC) - 移至GPIO20避免ADC冲突 */
#define GPIO_AUDIO_DIN        GPIO_NUM_19   /**< 串行数据输入引脚 - 移至GPIO19避免冲突 */

/* ========================================
 * MAX98357 控制引脚配置
 * ======================================== */
/**
 * @brief MAX98357 控制引脚（重定位到空闲GPIO）
 * - GAIN: GPIO21 (原GPIO9→D1冲突)
 * - SD:   GPIO47 (原GPIO33→DHT11冲突，移至GPIO47)
 */
#define GPIO_AUDIO_GAIN       GPIO_NUM_21   /**< 增益控制引脚 - 移至GPIO21避免冲突 */
#define GPIO_AUDIO_SD         GPIO_NUM_47   /**< 关闭控制引脚 - 移至GPIO47避免DHT11冲突 */

/* ========================================
 * 默认引脚映射
 * ======================================== */
/**
 * @brief 使用默认引脚时的宏定义
 */
#define AUDIO_DEFAULT_BCLK    GPIO_AUDIO_BCLK
#define AUDIO_DEFAULT_WS       GPIO_AUDIO_WS
#define AUDIO_DEFAULT_DIN     GPIO_AUDIO_DIN
#define AUDIO_DEFAULT_GAIN    GPIO_AUDIO_GAIN
#define AUDIO_DEFAULT_SD      GPIO_AUDIO_SD

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
