/**
 * @file audio_mp3.c
 * @brief MP3 流式解码实现 - 指针安全与防止踩内存版
 */
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "audio_mp3.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "AUDIO_MP3";

#define MAX_MP3_FRAME_SIZE 4096

static mp3dec_t *s_mp3dec = NULL;

static void init_decoder(void)
{
    if (s_mp3dec == NULL) {
        /* 从堆空间分配解码器结构体，防止局部/全局内存踩爆 */
        s_mp3dec = (mp3dec_t *)calloc(1, sizeof(mp3dec_t));
        if (s_mp3dec) {
            mp3dec_init(s_mp3dec);
            ESP_LOGI(TAG, "minimp3 解码器堆内存初始化成功");
        }
    }
}

static int skip_id3v2_tag(FILE *fp)
{
    uint8_t header[10];
    long pos = ftell(fp);
    
    if (fread(header, 1, 10, fp) != 10) {
        fseek(fp, pos, SEEK_SET);
        return -1;
    }
    
    if (header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
        int tag_size = ((header[6] & 0x7F) << 21) |
                       ((header[7] & 0x7F) << 14) |
                       ((header[8] & 0x7F) << 7) |
                       (header[9] & 0x7F);
        fseek(fp, pos + 10 + tag_size, SEEK_SET);
        ESP_LOGI(TAG, "自动跳过 ID3v2 标签: %d 字节", tag_size);
        return tag_size;
    }
    
    fseek(fp, pos, SEEK_SET);
    return 0;
}

static int find_sync(FILE *fp, uint8_t *buffer, int buf_size)
{
    long pos = ftell(fp);
    int bytes_read = fread(buffer, 1, buf_size, fp);
    if (bytes_read < 4) {
        return -1;
    }
    
    for (int i = 0; i < bytes_read - 3; i++) {
        if (buffer[i] == 0xFF && (buffer[i + 1] & 0xE0) == 0xE0) {
            fseek(fp, pos + i, SEEK_SET);
            return 1;
        }
    }
    
    return 0;
}

esp_err_t audio_mp3_decode_file(const char *file_path, audio_mp3_callback_t callback, void *user_data, audio_mp3_info_t *info)
{
    if (file_path == NULL || callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "无法打开 MP3 文件: %s", file_path);
        return ESP_FAIL;
    }

    init_decoder();
    if (s_mp3dec == NULL) {
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }

    skip_id3v2_tag(fp);

    /* 动态分配帧与 PCM 缓冲区 */
    uint8_t *frame_buffer = (uint8_t *)malloc(MAX_MP3_FRAME_SIZE);
    int16_t *pcm_buffer = (int16_t *)malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(int16_t));

    if (frame_buffer == NULL || pcm_buffer == NULL) {
        ESP_LOGE(TAG, "解码缓冲区内存分配失败");
        if (frame_buffer) free(frame_buffer);
        if (pcm_buffer) free(pcm_buffer);
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }

    int total_samples = 0;
    int frame_count = 0;
    mp3dec_frame_info_t frame_info;
    memset(&frame_info, 0, sizeof(frame_info));

    while (1) {
        int sync_found = find_sync(fp, frame_buffer, MAX_MP3_FRAME_SIZE);
        if (sync_found <= 0) {
            break;
        }

        size_t read_bytes = fread(frame_buffer, 1, MAX_MP3_FRAME_SIZE, fp);
        if (read_bytes == 0) {
            break;
        }

        /* 进行安全解码校验 */
        int samples = mp3dec_decode_frame(s_mp3dec, frame_buffer, read_bytes, pcm_buffer, &frame_info);
        
        if (samples > 0 && frame_info.frame_bytes > 0) {
            frame_count++;
            total_samples += samples;

            if (info && frame_count == 1) {
                info->sample_rate = frame_info.hz;
                info->channels = frame_info.channels;
                info->bitrate = frame_info.bitrate_kbps * 1000;
            }

            fseek(fp, -(read_bytes - frame_info.frame_bytes), SEEK_CUR);

            /* 将 PCM 回调抛出 */
            callback((const uint8_t *)pcm_buffer, samples * frame_info.channels * sizeof(int16_t), user_data);
        } else {
            /* 跳过非法或无法解码的单字节 */
            fseek(fp, -(read_bytes - 1), SEEK_CUR);
        }
    }

    if (info && info->sample_rate > 0 && info->channels > 0) {
        info->duration_ms = (uint32_t)(((int64_t)total_samples * 1000) / (info->sample_rate));
    }

    free(pcm_buffer);
    free(frame_buffer);
    fclose(fp);

    return (frame_count > 0) ? ESP_OK : ESP_FAIL;
}