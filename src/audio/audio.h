/**
 * @file audio.h
 * @brief MAX98357 音频模块接口定义
 *
 * 支持动态引脚配置、I2S音频播放、增益控制
 */
#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "../config/hw_audio.h"

/**
 * @brief 音频模块状态
 */
typedef enum {
    AUDIO_STATE_UNINIT = 0,  /**< 未初始化 */
    AUDIO_STATE_READY,       /**< 就绪 */
    AUDIO_STATE_PLAYING,     /**< 播放中 */
    AUDIO_STATE_PAUSED,      /**< 暂停 */
    AUDIO_STATE_ERROR        /**< 错误 */
} audio_state_t;

/**
 * @brief 音频格式定义
 */
typedef enum {
    AUDIO_FMT_16BIT = 0,     /**< 16位 */
    AUDIO_FMT_16BIT_STEREO,  /**< 16位立体声 */
    AUDIO_FMT_8BIT,          /**< 8位 */
} audio_format_t;

/**
 * @brief 音频采样率
 */
typedef enum {
    AUDIO_SAMPLE_RATE_8K = 8000,
    AUDIO_SAMPLE_RATE_16K = 16000,
    AUDIO_SAMPLE_RATE_22K = 22050,
    AUDIO_SAMPLE_RATE_44K = 44100,
    AUDIO_SAMPLE_RATE_48K = 48000,
} audio_sample_rate_t;

/**
 * @brief MAX98357 增益等级
 * GAIN引脚接法:
 * - 9dB: GAIN浮空
 * - 6dB: GAIN接GND
 * - 3dB: GAIN接SD
 * - 12dB: GAIN接VDD
 */
typedef enum {
    AUDIO_GAIN_3DB = 0,      /**< 3dB 增益 */
    AUDIO_GAIN_6DB = 1,      /**< 6dB 增益 */
    AUDIO_GAIN_9DB = 2,      /**< 9dB 增益 */
    AUDIO_GAIN_12DB = 3      /**< 12dB 增益 */
} audio_gain_t;

/**
 * @brief 音频引脚配置
 */
typedef struct {
    gpio_num_t bclk;         /**< 位时钟引脚 */
    gpio_num_t ws;           /**< 字选择引脚 (LRC) */
    gpio_num_t din;          /**< 串行数据输入引脚 (MAX98357 DIN) */
    gpio_num_t gain;         /**< 增益控制引脚 (可选) */
    gpio_num_t sd;           /**< 关闭控制引脚 (可选) */
    bool gain_enable;        /**< 是否启用软件增益控制 */
    bool sd_enable;          /**< 是否启用软件关闭控制 */
} audio_pin_config_t;

/**
 * @brief 音频播放配置
 */
typedef struct {
    audio_sample_rate_t sample_rate;  /**< 采样率 */
    audio_format_t format;            /**< 音频格式 */
    uint8_t channel;                 /**< 通道数: 1=单声道, 2=立体声 */
    audio_gain_t gain;               /**< 增益等级 */
} audio_play_config_t;

/**
 * @brief 音频文件信息
 */
typedef struct {
    char *filename;           /**< 文件名 */
    uint32_t size;            /**< 文件大小 */
    uint32_t duration_ms;     /**< 时长(毫秒) */
    audio_sample_rate_t sample_rate;  /**< 采样率 */
    uint32_t byte_rate;      /**< 字节速率 */
    uint8_t channels;        /**< 通道数 */
    uint8_t bits_per_sample;  /**< 位深度 */
} audio_file_info_t;

/* ========================================
 * 核心接口
 * ======================================== */

/**
 * @brief 初始化音频模块（使用默认引脚）
 */
esp_err_t audio_init(void);

/**
 * @brief 使用自定义引脚配置初始化音频模块
 * @param pin_config 引脚配置
 */
esp_err_t audio_init_with_pins(const audio_pin_config_t *pin_config);

/**
 * @brief 反初始化音频模块
 */
esp_err_t audio_deinit(void);

/**
 * @brief 获取音频模块状态
 */
audio_state_t audio_get_state(void);

/**
 * @brief 检查音频模块是否已初始化
 */
bool audio_is_initialized(void);

/* ========================================
 * 引脚配置接口（支持动态更改）
 * ======================================== */

/**
 * @brief 动态更改引脚配置
 * @param pin_config 新的引脚配置
 * @note 如果正在播放，会先停止播放
 */
esp_err_t audio_reconfig_pins(const audio_pin_config_t *pin_config);

/**
 * @brief 获取当前引脚配置
 * @param pin_config 输出参数，存储引脚配置
 */
esp_err_t audio_get_pin_config(audio_pin_config_t *pin_config);

/* ========================================
 * 增益控制接口
 * ======================================== */

/**
 * @brief 设置增益等级
 * @param gain 增益等级
 */
esp_err_t audio_set_gain(audio_gain_t gain);

/**
 * @brief 获取当前增益等级
 */
audio_gain_t audio_get_gain(void);

/**
 * @brief 设置增益引脚电平（如果使用软件增益控制）
 * @param level 电平: 0=GND, 1=VDD, 其他=浮空(9dB)
 */
esp_err_t audio_set_gain_pin_level(uint8_t level);

/* ========================================
 * 播放控制接口
 * ======================================== */

/**
 * @brief 配置播放参数
 * @param config 播放配置
 */
esp_err_t audio_configure(const audio_play_config_t *config);

/**
 * @brief 播放WAV文件
 * @param file_path 文件路径（支持SPIFFS和SD卡路径）
 * @return 播放持续时间(毫秒)，错误时返回-1
 */
int32_t audio_play_wav(const char *file_path);

/**
 * @brief 播放音频数据
 * @param data 音频数据缓冲区
 * @param len 数据长度（字节）
 * @param config 播放配置
 * @return ESP_OK成功，其他失败
 */
esp_err_t audio_play_data(const uint8_t *data, size_t len, const audio_play_config_t *config);

/**
 * @brief 停止播放
 */
esp_err_t audio_stop(void);

/**
 * @brief 暂停播放
 */
esp_err_t audio_pause(void);

/**
 * @brief 恢复播放
 */
esp_err_t audio_resume(void);

/* ========================================
 * 音频文件操作接口（使用POSIX VFS）
 * ======================================== */

/**
 * @brief 获取WAV文件信息
 * @param file_path 文件路径
 * @param info 输出参数，存储文件信息
 * @note 内存使用malloc分配，符合栈空间限制
 */
esp_err_t audio_get_wav_info(const char *file_path, audio_file_info_t *info);

/**
 * @brief 释放音频文件信息内存
 * @param info 文件信息指针
 */
void audio_free_wav_info(audio_file_info_t *info);

/* ========================================
 * 硬件控制接口
 * ======================================== */

/**
 * @brief 使能音频输出（SD引脚拉高）
 */
esp_err_t audio_enable_output(void);

/**
 * @brief 禁用音频输出（SD引脚拉低）
 */
esp_err_t audio_disable_output(void);

/**
 * @brief 设置音量（通过增益实现）
 * @param volume 音量等级 0-100
 */
esp_err_t audio_set_volume(uint8_t volume);

/**
 * @brief 获取软件音量百分比
 * @return 音量值 0-100
 */
uint8_t audio_get_volume_percent(void);

/**
 * @brief 设置软件音量百分比（真正调节音量大小）
 * @param volume 音量 0-100
 */
esp_err_t audio_set_volume_percent(uint8_t volume);

/* ========================================
 * MP3 播放接口
 * ======================================== */

/**
 * @brief 播放音频文件（自动检测格式，支持 WAV 和 MP3）
 * @param file_path 文件路径（支持SPIFFS和SD卡路径）
 * @return 播放持续时间(毫秒)，错误时返回-1
 */
int32_t audio_play_file(const char *file_path);

/**
 * @brief 播放 MP3 文件
 * @param file_path MP3 文件路径
 * @return 播放持续时间(毫秒)，错误时返回-1
 */
int32_t audio_play_mp3(const char *file_path);

#endif /* AUDIO_H */
