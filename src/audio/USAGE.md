# 音频模块 - 使用指南

## 快速开始（推荐）

使用简化版 `audio_simple`，无需额外依赖，立即可用：

```c
#include "audio/audio_simple.h"

void app_main(void)
{
    // 1. 初始化
    audio_simple_init();

    // 2. 一行播放（不阻塞）
    audio_simple_play("/sdcard/music.mp3");

    // 3. 主循环可继续处理其他任务
    while (1) {
        vTaskDelay(1000);
        if (!audio_simple_is_playing()) {
            printf("播放完成\n");
        }
    }
}
```

## API 列表

| 函数 | 说明 |
|------|------|
| `audio_simple_init()` | 初始化（后台任务在 Core 1） |
| `audio_simple_deinit()` | 反初始化 |
| `audio_simple_play(path)` | 播放音频文件（一行调用） |
| `audio_simple_stop()` | 停止播放 |
| `audio_simple_set_volume(vol)` | 设置音量 (0-100) |
| `audio_simple_is_playing()` | 检查是否播放中 |
| `audio_simple_get_info(info)` | 获取播放信息 |
| `audio_simple_get_progress()` | 获取进度百分比 |

## 支持格式

| 格式 | 扩展名 |
|------|--------|
| MP3 | .mp3 |
| WAV | .wav |

## 架构

```
主程序 (Core 0)          后台任务 (Core 1)
┌─────────────────┐      ┌─────────────────┐
│ audio_simple_   │      │ audio_play_file │
│ play("/x.mp3") │ ──▶  │ (minimp3解码)  │
│    (不阻塞)     │ 队列 │     + I2S       │
└─────────────────┘      └─────────────────┘
```

## ESP-ADF 完整版（多格式支持）

如需支持 FLAC、AAC、OPUS 等格式，需要安装 ESP-ADF 框架：

### 手动安装步骤

```bash
# 1. 删除失败的目录
rm -rf components/esp-adf

# 2. 使用 Gitee 镜像克隆（国内推荐）
cd components
git clone --recursive https://gitee.com/EspressifSystems/esp-adf.git

# 3. 等待子模块下载完成
cd esp-adf
git submodule update --init --recursive

# 4. 使用 audio_adf 模块
audio_adf_init_default();
audio_adf_play_file("/sdcard/music.flac");  // 支持更多格式
```

### ESP-ADF 支持的格式

| 格式 | 扩展名 | 解码器 |
|------|--------|--------|
| MP3 | .mp3 | Helix |
| AAC | .aac, .m4a | AAC |
| WAV | .wav | WAV |
| FLAC | .flac | FLAC |
| OGG | .ogg | Vorbis |
| OPUS | .opus | Opus |

### ESP-ADF API

```c
// 初始化
audio_adf_init_default();

// 播放文件
audio_adf_play_file("/sdcard/music.flac");

// 播放网络流
audio_adf_play_url("http://stream.example.com/live.mp3");

// 控制
audio_adf_pause();
audio_adf_resume();
audio_adf_stop();
audio_adf_set_volume(80);

// 查询
audio_adf_is_playing();
audio_adf_get_info(&info);
```

## 文件列表

| 文件 | 说明 |
|------|------|
| `audio_simple.h/cpp` | 简化版（立即可用） |
| `audio_async.h/cpp` | 异步版（同简化版） |
| `audio_adf.h/cpp` | ESP-ADF 版（需安装 ADF） |
| `audio.h/c` | 底层 audio 模块 |
| `audio_mp3.h/c` | MP3 解码器 |
