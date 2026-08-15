/**
 * @file audio_i2s_example.c
 * @brief audioI2S 使用示例
 *
 * 展示如何在主程序中使用 audioI2S 播放音频
 */
#include "audio_i2s.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "AUDIO_EXAMPLE";

/**
 * @brief 音频播放任务示例
 *
 * 演示如何在后台播放音频而不阻塞主线程
 */
void audio_example_task(void *param)
{
    (void)param;

    ESP_LOGI(TAG, "=== audioI2S 示例开始 ===");

    // 1. 初始化（使用默认引脚）
    ESP_LOGI(TAG, "初始化 audioI2S...");
    esp_err_t ret = audio_i2s_init_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化失败: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    // 2. 设置音量
    audio_i2s_set_volume(18);  // 音量范围 0-21

    // 3. 播放 SD 卡中的 MP3 文件
    ESP_LOGI(TAG, "播放 /sdcard/music/test.mp3");
    audio_i2s_play_file("/sdcard/music/test.mp3");

    // 4. 主循环（处理音频任务，不阻塞）
    while (1) {
        // 等待一段时间
        vTaskDelay(pdMS_TO_TICKS(2000));

        // 检查播放状态
        if (audio_i2s_is_playing()) {
            uint8_t progress = audio_i2s_get_progress();
            ESP_LOGI(TAG, "播放进度: %d%%", progress);
        } else {
            audio_i2s_info_t info;
            audio_i2s_get_info(&info);

            if (info.state == AUDIO_I2S_STATE_STOPPED) {
                ESP_LOGI(TAG, "播放完成，播放下一首...");
                audio_i2s_play_file("/sdcard/music/next.mp3");
            }
        }
    }
}

/**
 * @brief HTTP API 触发播放
 *
 * 在 Web handler 中调用，一行代码即可
 */
esp_err_t http_play_audio_handler(const char *file_path)
{
    // 只需一行调用，播放将在后台进行
    return audio_i2s_play_file(file_path);
}

/**
 * @brief HTTP API 停止播放
 */
esp_err_t http_stop_audio_handler(void)
{
    return audio_i2s_stop();
}

/**
 * @brief HTTP API 获取播放状态
 */
esp_err_t http_get_audio_status_handler(audio_i2s_info_t *info)
{
    audio_i2s_get_info(info);
    return ESP_OK;
}
