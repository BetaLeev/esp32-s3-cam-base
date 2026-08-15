/**
 * @file audio_i2s.h
 * @brief ESP32-audioI2S 封装层 (ESP-IDF 版本)
 *
 * 将 ESP32-audioI2S 封装为后台 Task 运行在 Core 1，
 * 只需调用 audio_i2s_play_file() 即可播放音频，不阻塞主线程
 */
#ifndef AUDIO_I2S_H
#define AUDIO_I2S_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "../config/hw_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 播放状态
 */
typedef enum {
    AUDIO_I2S_STATE_IDLE = 0,      /**< 空闲 */
    AUDIO_I2S_STATE_PLAYING,        /**< 播放中 */
    AUDIO_I2S_STATE_PAUSED,         /**< 暂停 */
    AUDIO_I2S_STATE_STOPPED,        /**< 已停止 */
    AUDIO_I2S_STATE_ERROR           /**< 错误 */
} audio_i2s_state_t;

/**
 * @brief 播放信息
 */
typedef struct {
    audio_i2s_state_t state;       /**< 当前状态 */
    uint32_t duration_ms;           /**< 总时长(毫秒) */
    uint32_t position_ms;          /**< 当前进度(毫秒) */
    uint32_t sample_rate;          /**< 采样率 */
    uint8_t channels;             /**< 通道数 */
    uint32_t bitrate;             /**< 比特率 */
    char current_file[256];       /**< 当前播放文件 */
} audio_i2s_info_t;

/**
 * @brief 初始化 audioI2S 后台任务
 *
 * 必须在 Core 0 初始化（在 app_main 中调用）
 * 音频解码和 I2S 喂数将在 Core 1 的后台任务中运行
 *
 * @param bclk   位时钟引脚
 * @param ws     字选择引脚 (LRC)
 * @param dout   数据输出引脚
 * @return ESP_OK 成功
 */
esp_err_t audio_i2s_init(gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout);

/**
 * @brief 使用默认引脚初始化
 *
 * 使用 hw_audio.h 中的配置初始化
 */
esp_err_t audio_i2s_init_default(void);

/**
 * @brief 反初始化
 */
esp_err_t audio_i2s_deinit(void);

/**
 * @brief 播放音频文件（一行API，不阻塞）
 *
 * 只需一行调用，音频播放将在后台进行
 * 支持 MP3, AAC, M4A, FLAC, OPUS, VORBIS, WAV 格式
 *
 * @param file_path 文件路径（支持绝对路径或相对SD卡路径）
 *                  例如: "/music/test.mp3" 或 "0:/sdcard/music/test.mp3"
 * @return ESP_OK 已开始播放（后台异步）
 *         ESP_ERR_INVALID_STATE 未初始化或前一首未停止
 */
esp_err_t audio_i2s_play_file(const char *file_path);

/**
 * @brief 播放 SD 卡音频文件
 *
 * @param path    文件路径
 * @return ESP_OK 成功
 */
esp_err_t audio_i2s_play_sd(const char *path);

/**
 * @brief 播放网络流
 *
 * @param url 流地址（如 "http://stream.example.com/live.mp3"）
 * @return ESP_OK 成功
 */
esp_err_t audio_i2s_play_url(const char *url);

/**
 * @brief 停止播放
 */
esp_err_t audio_i2s_stop(void);

/**
 * @brief 暂停/恢复播放
 * @return true 暂停成功, false 恢复成功
 */
bool audio_i2s_pause_resume(void);

/**
 * @brief 设置音量
 * @param vol 音量 0-21（默认21）
 */
void audio_i2s_set_volume(uint8_t vol);

/**
 * @brief 获取当前播放信息
 * @param info 输出参数
 */
void audio_i2s_get_info(audio_i2s_info_t *info);

/**
 * @brief 检查是否正在播放
 */
bool audio_i2s_is_playing(void);

/**
 * @brief 设置播放位置（秒）
 * @param sec 秒数
 * @return true 成功
 */
bool audio_i2s_set_position(uint16_t sec);

/**
 * @brief 跳过指定秒数
 * @param sec 正数快进，负数快退
 */
void audio_i2s_skip(int16_t sec);

/**
 * @brief 获取播放进度百分比
 * @return 0-100
 */
uint8_t audio_i2s_get_progress(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_I2S_H */
