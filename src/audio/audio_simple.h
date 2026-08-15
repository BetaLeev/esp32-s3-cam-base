/**
 * @file audio_simple.h
 * @brief 简化音频播放（不依赖 ESP-ADF）
 *
 * 基于现有 audio.c 模块，提供简洁的异步播放接口
 * 零依赖，直接可用
 */
#ifndef AUDIO_SIMPLE_H
#define AUDIO_SIMPLE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 播放状态
 */
typedef enum {
    AUDIO_SIMPLE_STATE_IDLE = 0,
    AUDIO_SIMPLE_STATE_PLAYING,
    AUDIO_SIMPLE_STATE_STOPPED,
} audio_simple_state_t;

/**
 * @brief 音频信息
 */
typedef struct {
    audio_simple_state_t state;
    uint32_t duration_ms;
    uint32_t position_ms;
    char current_file[128];
} audio_simple_info_t;

/**
 * @brief 初始化音频模块
 * @return ESP_OK 成功
 */
esp_err_t audio_simple_init(void);

/**
 * @brief 反初始化
 */
esp_err_t audio_simple_deinit(void);

/**
 * @brief 播放音频文件（异步，不阻塞）
 * @param file_path 文件路径
 * @return ESP_OK 开始播放
 */
esp_err_t audio_simple_play(const char *file_path);

/**
 * @brief 停止播放
 */
void audio_simple_stop(void);

/**
 * @brief 设置音量 (0-100)
 */
void audio_simple_set_volume(uint8_t volume);

/**
 * @brief 检查是否播放中
 */
bool audio_simple_is_playing(void);

/**
 * @brief 获取播放信息
 */
void audio_simple_get_info(audio_simple_info_t *info);

/**
 * @brief 获取进度百分比
 */
uint8_t audio_simple_get_progress(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_SIMPLE_H */
