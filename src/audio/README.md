# 音频模块使用指南

## 快速开始

### 1. 初始化（在 app_main 中）

```c
#include "audio/audio_async.h"

void app_main(void)
{
    // ... 其他初始化 ...

    // 初始化异步音频模块
    ESP_ERROR_CHECK(audio_async_init());

    // 设置音量 (0-100)
    audio_async_set_volume(80);
}
```

### 2. 播放音频（一行调用，不阻塞）

```c
// 播放 SD 卡中的 MP3 文件
audio_async_play("/sdcard/music/River Flows in You.mp3");

// 或使用相对路径
audio_async_play("music/test.mp3");

// 播放完成前可以继续处理其他任务
```

### 3. 控制播放

```c
// 停止
audio_async_stop();

// 暂停/恢复
audio_async_pause();
audio_async_resume();

// 设置音量
audio_async_set_volume(60);
```

### 4. 查询状态

```c
// 检查是否播放中
if (audio_async_is_playing()) {
    // 获取播放信息
    audio_async_info_t info;
    audio_async_get_info(&info);
    printf("正在播放: %s\n", info.current_file);
}

// 获取进度百分比
uint8_t progress = audio_async_get_progress();
printf("进度: %d%%\n", progress);
```

## 完整示例

```c
#include "audio/audio_async.h"
#include "esp_log.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "应用启动");

    // 初始化异步音频
    ESP_ERROR_CHECK(audio_async_init());
    audio_async_set_volume(80);

    // 播放音乐
    ESP_LOGI(TAG, "播放 /sdcard/music.mp3");
    audio_async_play("/sdcard/music/River Flows in You.mp3");

    // 主循环
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (audio_async_is_playing()) {
            ESP_LOGI(TAG, "进度: %d%%", audio_async_get_progress());
        } else {
            audio_async_info_t info;
            audio_async_get_info(&info);
            if (info.state == AUDIO_ASYNC_STATE_STOPPED) {
                ESP_LOGI(TAG, "播放完成，播放下一首");
                audio_async_play("/sdcard/music/next.mp3");
            }
        }
    }
}
```

## API 列表

| 函数 | 说明 |
|------|------|
| `audio_async_init()` | 初始化（后台任务在 Core 1） |
| `audio_async_deinit()` | 反初始化 |
| `audio_async_play(path)` | 播放音频文件（一行调用） |
| `audio_async_play_sd(path)` | 播放 SD 卡文件 |
| `audio_async_stop()` | 停止播放 |
| `audio_async_pause()` | 暂停 |
| `audio_async_resume()` | 继续播放 |
| `audio_async_set_volume(vol)` | 设置音量 (0-100) |
| `audio_async_get_volume()` | 获取音量 |
| `audio_async_is_playing()` | 检查是否播放中 |
| `audio_async_get_info(info)` | 获取播放信息 |
| `audio_async_get_progress()` | 获取进度百分比 |

## 支持格式

| 格式 | 扩展名 |
|------|--------|
| MP3 | .mp3 |
| WAV | .wav |

## 架构说明

```
┌─────────────────────────────────────────────────────────────┐
│                       主程序 (Core 0)                        │
│                                                              │
│   audio_async_play("/sdcard/music.mp3");  // 一行调用        │
│                         │                                    │
│                         ▼ 发送命令到队列（不阻塞）           │
└─────────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│                   后台任务 (Core 1)                          │
│                                                              │
│   ┌──────────┐    ┌──────────┐    ┌──────────┐          │
│   │ minimp3  │ -> │ 音量调节  │ -> │   I2S    │ -> DAC   │
│   │  解码器  │    │          │    │   输出    │          │
│   └──────────┘    └──────────┘    └──────────┘          │
│                                                              │
│   解码 + I2S 喂数 全部在 Core 1 运行，不占主线程            │
└─────────────────────────────────────────────────────────────┘
```

## 高级用法：ESP-ADF 框架

如需支持更多音频格式（FLAC、AAC、OPUS 等），可安装 ESP-ADF 框架：

```bash
# 克隆 ESP-ADF
git clone --recursive https://github.com/espressif/esp-adf.git ~/esp/esp-adf

# 设置环境变量
. ~/esp/esp-adf/export.sh

# 使用 audio_adf 模块
audio_adf_play_file("/sdcard/music.flac");
```

详细说明见：[ESP-ADF-INTEGRATION.md](ESP-ADF-INTEGRATION.md)
