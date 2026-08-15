/**
 * @file audio_async.h
 * @brief 异步音频播放封装（后台 Task）
 *
 * 将音频播放移至 Core 1 后台任务，主程序只需一行调用
 * 不阻塞主线程
 *
 * 支持: MP3, WAV
 */
#ifndef AUDIO_ASYNC_H
#define AUDIO_ASYNC_H

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
    AUDIO_ASYNC_STATE_IDLE = 0,
    AUDIO_ASYNC_STATE_PLAYING,
    AUDIO_ASYNC_STATE_PAUSED,
    AUDIO_ASYNC_STATE_STOPPED,
    AUDIO_ASYNC_STATE_ERROR
} audio_async_state_t;

/**
 * @brief 音频信息
 */
typedef struct {
    audio_async_state_t state;
    uint32_t duration_ms;
    uint32_t position_ms;
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t bitrate;
    char current_file[256];
} audio_async_info_t;

/**
 * @brief 初始化异步音频模块
 *
 * 必须在 app_main 中调用，音频播放将在 Core 1 的后台任务中运行
 *
 * @return ESP_OK 成功
 */
esp_err_t audio_async_init(void);

/**
 * @brief 反初始化
 */
esp_err_t audio_async_deinit(void);

/**
 * @brief 播放音频文件（一行 API，不阻塞）
 *
 * 只需一行调用，音频播放将在后台进行
 * 支持 MP3, WAV 格式
 *
 * @param file_path 文件路径（支持绝对路径或相对路径）
 * @return ESP_OK 开始播放
 */
esp_err_t audio_async_play(const char *file_path);

/**
 * @brief 播放 SD 卡文件
 * @param path 文件路径
 */
esp_err_t audio_async_play_sd(const char *path);

/**
 * @brief 停止播放
 */
esp_err_t audio_async_stop(void);

/**
 * @brief 暂停播放
 */
esp_err_t audio_async_pause(void);

/**
 * @brief 恢复播放
 */
esp_err_t audio_async_resume(void);

/**
 * @brief 设置音量 (0-100)
 */
void audio_async_set_volume(uint8_t volume);

/**
 * @brief 获取音量
 */
uint8_t audio_async_get_volume(void);

/**
 * @brief 检查是否正在播放
 */
bool audio_async_is_playing(void);

/**
 * @brief 获取播放信息
 */
void audio_async_get_info(audio_async_info_t *info);

/**
 * @brief 获取播放进度百分比
 * @return 0-100
 */
uint8_t audio_async_get_progress(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_ASYNC_H */
