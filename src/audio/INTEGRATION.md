# ESP32-audioI2S 集成指南

## 概述

本目录提供了 ESP32-audioI2S 库的封装层，将复杂的多格式音频解码简化为一行 API 调用：

```c
audio_i2s_play_file("/sdcard/music/test.mp3");
```

## 硬件要求

- **芯片**: ESP32, ESP32-S3, ESP32-P4 (多核)
- **PSRAM**: 最少 2MB
- **DAC**: MAX98357A, PCM5102A, CS4344, UDA1334A 等 I2S DAC

你的项目配置：
- ✅ ESP32-S3 (多核)
- ✅ PSRAM 已启用
- ✅ MAX98357A 音频放大器

## 快速使用

### 1. 初始化（app_main 中调用）

```c
#include "audio/audio_i2s.h"

// 使用默认引脚初始化（在 Core 1 运行后台任务）
esp_err_t ret = audio_i2s_init_default();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "audioI2S 初始化失败");
}
```

### 2. 播放音频（一行调用）

```c
// 播放 SD 卡文件
audio_i2s_play_file("/sdcard/music/River Flows in You.mp3");

// 播放网络流
audio_i2s_play_url("http://stream.example.com/live.mp3");

// 设置音量 (0-21)
audio_i2s_set_volume(18);

// 暂停/继续
bool is_paused = audio_i2s_pause_resume();

// 停止
audio_i2s_stop();
```

### 3. 查询播放状态

```c
audio_i2s_info_t info;
audio_i2s_get_info(&info);

printf("状态: %d\n", info.state);
printf("进度: %u/%u ms\n", info.position_ms, info.duration_ms);
printf("文件: %s\n", info.current_file);

// 获取进度百分比
uint8_t progress = audio_i2s_get_progress();
```

## 支持的音频格式

| 格式 | 扩展名 | 解码器 |
|------|--------|--------|
| MP3 | .mp3 | Helix |
| AAC | .aac, .m4a | Helix |
| FLAC | .flac | FLAC |
| OPUS | .opus | libopus |
| Vorbis | .ogg | libvorbis |
| WAV | .wav | 内置 |

## 完整示例

```c
// main.c
#include "audio/audio_i2s.h"

void app_main(void)
{
    // ... 其他初始化 ...

    // 初始化 audioI2S（后台任务将自动在 Core 1 运行）
    ESP_ERROR_CHECK(audio_i2s_init_default());

    // 播放音乐
    ESP_LOGI(TAG, "开始播放音乐...");
    audio_i2s_play_file("/sdcard/music/test.mp3");

    // 主循环中可以继续处理其他任务
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 定期检查播放状态
        if (audio_i2s_is_playing()) {
            printf("播放进度: %d%%\n", audio_i2s_get_progress());
        }
    }
}
```

## 添加 ESP32-audioI2S 库依赖

### 方法一：idf.py 添加组件

```bash
idf.py add-dependency "schreibfaul1/esp32-audioi2s"
```

### 方法二：手动下载到 components

```bash
cd /path/to/esp32_demo
git clone https://github.com/schreibfaul1/ESP32-audioI2S.git components/audioI2S
```

## API 参考

| 函数 | 说明 |
|------|------|
| `audio_i2s_init()` | 使用自定义引脚初始化 |
| `audio_i2s_init_default()` | 使用默认引脚初始化 |
| `audio_i2s_deinit()` | 反初始化 |
| `audio_i2s_play_file()` | 播放本地文件 |
| `audio_i2s_play_fs()` | 播放指定文件系统 |
| `audio_i2s_play_url()` | 播放网络流 |
| `audio_i2s_stop()` | 停止播放 |
| `audio_i2s_pause_resume()` | 暂停/继续 |
| `audio_i2s_set_volume()` | 设置音量 |
| `audio_i2s_get_info()` | 获取播放信息 |
| `audio_i2s_is_playing()` | 检查是否播放中 |
| `audio_i2s_set_position()` | 跳转位置 |
| `audio_i2s_skip()` | 跳过秒数 |
| `audio_i2s_get_progress()` | 获取进度百分比 |

## 与现有 audio.c 的关系

- **audio.c**: 使用 minimp3 解码器，播放 MP3（同步方式）
- **audio_i2s.c**: 使用 ESP32-audioI2S 库，支持多格式（异步后台任务）

建议：
- 简单 MP3 播放 → 使用 `audio_play_mp3()`
- 多格式/流媒体 → 使用 `audio_i2s_*`

## 故障排除

### 编译错误：找不到 Audio.h

需要添加 ESP32-audioI2S 库依赖：

```bash
idf.py add-dependency "schreibfaul1/esp32-audioi2s"
idf.py reconfigure
idf.py build
```

### 运行时崩溃

检查：
1. 是否启用 PSRAM (`CONFIG_SPIRAM=y`)
2. 芯片是否是 ESP32/ESP32-S3/ESP32-P4
3. 是否有足够的堆内存（解码器需要约 40KB）

### 播放无声音

1. 检查 I2S 引脚接线
2. 检查 DAC 芯片是否正常
3. 确认音量 > 0
4. 检查 SD 卡文件是否存在
