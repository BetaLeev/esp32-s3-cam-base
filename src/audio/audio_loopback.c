#include "audio_loopback.h"
#include "audio.h"
#include "audio/mic.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "LOOPBACK";
static bool s_loopback_enabled = false;

extern esp_err_t audio_write_raw_data(const uint8_t *data, size_t size);

// 扩展双声道缓冲区
static int16_t s_stereo_out[1024];

extern void mic_get_params(int *sample_rate, int *shift_bits, float *volume_scale);

static void mic_stream_callback(const uint8_t *data, size_t size) {
    if (!s_loopback_enabled)
        return;

    if (!audio_is_initialized() || audio_get_state() == AUDIO_STATE_PLAYING) {
        return;
    }

    const int16_t *mono_in = (const int16_t *)data;
    size_t mono_count = size / sizeof(int16_t);

    if (mono_count * 2 > (sizeof(s_stereo_out) / sizeof(int16_t))) {
        mono_count = (sizeof(s_stereo_out) / sizeof(int16_t)) / 2;
    }

    // 获取前端动态设置的缩放比例 (默认给个安全的 0.2f)
    float vol_scale = 0.2f;
    // 如果你有动态调整函数可以在这里读，没有就写固定安全值

    for (size_t i = 0; i < mono_count; i++) {
        // 加上防溢出限幅保护 (Clamping)，绝对防止爆音杂音
        int32_t temp = (int32_t)(mono_in[i] * vol_scale);
        if (temp > 32767)
            temp = 32767;
        if (temp < -32768)
            temp = -32768;

        int16_t sample = (int16_t)temp;
        s_stereo_out[i * 2] = sample;     // 左声道
        s_stereo_out[i * 2 + 1] = sample; // 右声道
    }

    audio_write_raw_data((const uint8_t *)s_stereo_out, mono_count * 2 * sizeof(int16_t));
}
void audio_loopback_set_enabled(bool enable) {
    if (s_loopback_enabled == enable)
        return;
    s_loopback_enabled = enable;

    if (enable) {
        ESP_LOGI(TAG, ">>> 开启麦克风实时扩音直通 <<<");
        audio_init();
        audio_enable_output();
        audio_set_volume_percent(50);

        mic_init();
        mic_set_testing(true);
        mic_register_callback(mic_stream_callback);
    } else {
        ESP_LOGI(TAG, ">>> 关闭麦克风实时扩音直通 <<<");
        mic_register_callback(NULL);
        mic_set_testing(false);
    }
}

bool audio_loopback_is_enabled(void) {
    return s_loopback_enabled;
}