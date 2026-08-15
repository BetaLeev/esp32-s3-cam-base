/**
 * @file audio.h
 * @brief MAX98357 音频模块接口定义
 */
#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "../config/hw_audio.h"

/* 核心修复：必须加上 extern "C"，否则 C++ 文件调用 C 函数会发生链接错误 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 音频模块状态
 */
typedef enum {
    AUDIO_STATE_UNINIT = 0,  
    AUDIO_STATE_READY,       
    AUDIO_STATE_PLAYING,     
    AUDIO_STATE_PAUSED,      
    AUDIO_STATE_ERROR        
} audio_state_t;

typedef enum {
    AUDIO_FMT_16BIT = 0,     
    AUDIO_FMT_16BIT_STEREO,  
    AUDIO_FMT_8BIT,          
} audio_format_t;

typedef enum {
    AUDIO_SAMPLE_RATE_8K = 8000,
    AUDIO_SAMPLE_RATE_16K = 16000,
    AUDIO_SAMPLE_RATE_22K = 22050,
    AUDIO_SAMPLE_RATE_44K = 44100,
    AUDIO_SAMPLE_RATE_48K = 48000,
} audio_sample_rate_t;

typedef enum {
    AUDIO_GAIN_3DB = 0,      
    AUDIO_GAIN_6DB = 1,      
    AUDIO_GAIN_9DB = 2,      
    AUDIO_GAIN_12DB = 3      
} audio_gain_t;

typedef struct {
    gpio_num_t bclk;         
    gpio_num_t ws;           
    gpio_num_t din;          
    gpio_num_t gain;         
    gpio_num_t sd;           
    bool gain_enable;        
    bool sd_enable;          
} audio_pin_config_t;

typedef struct {
    audio_sample_rate_t sample_rate;  
    audio_format_t format;            
    uint8_t channel;                 
    audio_gain_t gain;               
} audio_play_config_t;

typedef struct {
    char *filename;           
    uint32_t size;            
    uint32_t duration_ms;     
    audio_sample_rate_t sample_rate;  
    uint32_t byte_rate;      
    uint8_t channels;        
    uint8_t bits_per_sample;  
} audio_file_info_t;

/* ========================================
 * 核心接口声明
 * ======================================== */
esp_err_t audio_init(void);
esp_err_t audio_init_with_pins(const audio_pin_config_t *pin_config);
esp_err_t audio_deinit(void);
audio_state_t audio_get_state(void);
bool audio_is_initialized(void);

esp_err_t audio_reconfig_pins(const audio_pin_config_t *pin_config);
esp_err_t audio_get_pin_config(audio_pin_config_t *pin_config);

esp_err_t audio_set_gain(audio_gain_t gain);
audio_gain_t audio_get_gain(void);

int32_t audio_play_wav(const char *file_path);
int32_t audio_play_mp3(const char *file_path);
int32_t audio_play_file(const char *file_path);
esp_err_t audio_play_data(const uint8_t *data, size_t len, const audio_play_config_t *config);
esp_err_t audio_play_wav_data(const int16_t *data, size_t size);

esp_err_t audio_stop(void);
esp_err_t audio_pause(void);
esp_err_t audio_resume(void);

esp_err_t audio_get_wav_info(const char *file_path, audio_file_info_t *info);
void audio_free_wav_info(audio_file_info_t *info);

esp_err_t audio_enable_output(void);
esp_err_t audio_disable_output(void);
esp_err_t audio_set_volume(uint8_t volume);
uint8_t audio_get_volume_percent(void);
esp_err_t audio_set_volume_percent(uint8_t volume);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_H */