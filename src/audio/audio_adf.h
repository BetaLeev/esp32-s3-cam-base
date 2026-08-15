/**
 * @file audio_adf.h
 * @brief ESP-ADF 音频封装层
 *
 * 基于 ESP-ADF Pipeline 架构的音频播放封装
 * 支持 MP3/AAC/WAV/FLAC 等多格式，后台 Task 运行
 *
 * 使用方法：
 *   audio_adf_init();
 *   audio_adf_play_file("/sdcard/music.mp3");  // 一行调用，不阻塞
 */
#ifndef AUDIO_ADF_H
#define AUDIO_ADF_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 播放状态
 */
typedef enum {
    AUDIO_ADF_STATE_IDLE = 0,
    AUDIO_ADF_STATE_PLAYING,
    AUDIO_ADF_STATE_PAUSED,
    AUDIO_ADF_STATE_STOPPED,
    AUDIO_ADF_STATE_ERROR
} audio_adf_state_t;

/**
 * @brief 音频信息
 */
typedef struct {
    audio_adf_state_t state;
    uint32_t duration_ms;
    uint32_t position_ms;
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t bitrate;
    char current_file[256];
} audio_adf_info_t;

/**
 * @brief 音频事件类型
 */
typedef enum {
    AUDIO_ADF_EVENT_NONE = 0,
    AUDIO_ADF_EVENT_PLAYING,
    AUDIO_ADF_EVENT_PAUSED,
    AUDIO_ADF_EVENT_STOPPED,
    AUDIO_ADF_EVENT_COMPLETE,
    AUDIO_ADF_EVENT_ERROR
} audio_adf_event_t;

/**
 * @brief 音频事件回调
 */
typedef void (*audio_adf_event_callback_t)(audio_adf_event_t event, void *user_data);

/**
 * @brief 初始化 ADF 音频模块
 *
 * @param bclk BCLK 引脚
 * @param ws   WS/LRC 引脚
 * @param dout DOUT 引脚
 * @return ESP_OK 成功
 */
esp_err_t audio_adf_init(gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout);

/**
 * @brief 使用默认引脚初始化
 */
esp_err_t audio_adf_init_default(void);

/**
 * @brief 反初始化
 */
esp_err_t audio_adf_deinit(void);

/**
 * @brief 播放音频文件（一行 API，不阻塞）
 *
 * 支持格式: MP3, AAC, WAV, FLAC, OGG, OPUS
 *
 * @param file_path 文件路径
 * @return ESP_OK 开始播放
 */
esp_err_t audio_adf_play_file(const char *file_path);

/**
 * @brief 播放 SD 卡文件
 */
esp_err_t audio_adf_play_sd(const char *path);

/**
 * @brief 播放网络流
 *
 * @param url 流地址
 * @return ESP_OK 成功
 */
esp_err_t audio_adf_play_url(const char *url);

/**
 * @brief 停止播放
 */
esp_err_t audio_adf_stop(void);

/**
 * @brief 暂停播放
 */
esp_err_t audio_adf_pause(void);

/**
 * @brief 恢复播放
 */
esp_err_t audio_adf_resume(void);

/**
 * @brief 设置音量 (0-100)
 */
void audio_adf_set_volume(uint8_t volume);

/**
 * @brief 获取音量
 */
uint8_t audio_adf_get_volume(void);

/**
 * @brief 检查是否正在播放
 */
bool audio_adf_is_playing(void);

/**
 * @brief 获取播放信息
 */
void audio_adf_get_info(audio_adf_info_t *info);

/**
 * @brief 设置事件回调
 */
void audio_adf_set_event_callback(audio_adf_event_callback_t callback, void *user_data);

/**
 * @brief 跳转播放位置
 */
esp_err_t audio_adf_seek(uint32_t position_ms);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ADF_H */
