/**
 * @file audio_mp3.h
 * @brief MP3 解码接口
 */
#ifndef AUDIO_MP3_H
#define AUDIO_MP3_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief MP3 解码信息
 */
typedef struct {
    int sample_rate;     /**< 采样率 */
    int channels;        /**< 通道数 */
    int bitrate;         /**< 比特率 */
    uint32_t duration_ms;/**< 时长（毫秒） */
} audio_mp3_info_t;

/**
 * @brief MP3 解码回调函数
 * @param pcm_data PCM 数据
 * @param pcm_size 数据大小（字节）
 * @param user_data 用户数据
 */
typedef void (*audio_mp3_callback_t)(const uint8_t *pcm_data, size_t pcm_size, void *user_data);

/**
 * @brief 解码 MP3 文件并通过回调输出 PCM 数据
 * @param file_path MP3 文件路径
 * @param callback PCM 数据回调
 * @param user_data 用户数据
 * @param info 输出解码信息（可为 NULL）
 * @return ESP_OK 成功，其他失败
 */
esp_err_t audio_mp3_decode_file(const char *file_path, audio_mp3_callback_t callback, void *user_data, audio_mp3_info_t *info);

/**
 * @brief 解码 MP3 文件并返回 PCM 数据（同步方式）
 * @param file_path MP3 文件路径
 * @param pcm_data 输出 PCM 数据（需要调用 audio_mp3_free_pcm 释放）
 * @param pcm_size 输出 PCM 数据大小
 * @param info 输出解码信息（可为 NULL）
 * @return ESP_OK 成功，其他失败
 */
esp_err_t audio_mp3_decode_to_pcm(const char *file_path, uint8_t **pcm_data, size_t *pcm_size, audio_mp3_info_t *info);

/**
 * @brief 释放 PCM 数据内存
 */
void audio_mp3_free_pcm(uint8_t *pcm_data);

#endif /* AUDIO_MP3_H */
