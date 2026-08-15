# ESP-ADF 集成指南

## 概述

ESP-ADF (Audio Development Framework) 是乐鑫官方的音频开发框架，提供完整的 Pipeline 架构，支持多格式音频解码和网络流播放。

## 硬件要求

- **芯片**: ESP32, ESP32-S3, ESP32-P4 (多核)
- **PSRAM**: 推荐启用
- **DAC**: MAX98357A, ES8388, ES8311 等

你的项目已满足所有要求：
- ✅ ESP32-S3 (多核)
- ✅ PSRAM 已启用
- ✅ MAX98357A

## 快速使用

### 1. 初始化

```c
#include "audio/audio_adf.h"

// 在 app_main 中调用
esp_err_t ret = audio_adf_init_default();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ADF 初始化失败");
}
```

### 2. 播放音频（一行 API）

```c
// 播放 SD 卡文件
audio_adf_play_file("/sdcard/music/test.mp3");

// 播放网络流
audio_adf_play_url("http://stream.example.com/live.mp3");

// 设置音量
audio_adf_set_volume(80);

// 暂停/恢复
audio_adf_pause();
audio_adf_resume();

// 停止
audio_adf_stop();
```

### 3. 查询状态

```c
audio_adf_info_t info;
audio_adf_get_info(&info);
printf("状态: %d, 进度: %d%%\n", info.state, progress);

// 检查播放状态
if (audio_adf_is_playing()) {
    // 正在播放
}
```

## ESP-ADF 架构

```
┌─────────────────────────────────────────────────────────────┐
│                      Audio Pipeline                          │
│                                                             │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐            │
│  │ FATFS/   │ -> │ Decoder  │ -> │ I2S      │ -> DAC     │
│  │ HTTP     │    │ MP3/AAC  │    │ Stream   │            │
│  └──────────┘    └──────────┘    └──────────┘            │
│                                                             │
│  文件/网络      解码              I2S输出                  │
└─────────────────────────────────────────────────────────────┘
```

## 支持的音频格式

| 格式 | 扩展名 | 解码器 |
|------|--------|--------|
| MP3 | .mp3 | Helix MP3 |
| AAC | .aac, .m4a | AAC decoder |
| WAV | .wav | WAV decoder |
| FLAC | .flac | FLAC decoder |
| OGG | .ogg | Vorbis decoder |
| OPUS | .opus | Opus decoder |
| AMR | .amr | AMR decoder |

## ESP-ADF 安装步骤

### 方案一：独立安装 ADF（推荐）

```bash
# 1. 克隆 ESP-ADF 仓库
cd ~/esp
git clone --recursive https://github.com/espressif/esp-adf.git

# 2. 设置环境变量
. ~/esp/esp-adf/export.sh

# 3. 验证安装
idf.py -C ~/esp/esp-adf/examples/get-started/play_mp3 build
```

### 方案二：作为组件集成到项目

```bash
# 在项目根目录
cd /Users/v/Desktop/esp32_demo

# 添加 ADF 子模块
git submodule add https://github.com/espressif/esp-adf.git components/esp-adf
git submodule update --init --recursive

# 或者使用 idf.py
idf.py add-dependency "espressif/esp-adf"
```

## 更新 CMakeLists.txt

集成 ESP-ADF 后，更新组件的 CMakeLists.txt：

```cmake
idf_component_register(
    SRCS "audio_adf.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_http_server
        driver
        freertos
        esp_netif
        esp_timer
        nvs_flash
        audio_pipeline      # ADF 核心组件
        audio_stream
        fatfs_stream       # 文件系统流
        i2s_stream        # I2S 输出流
        mp3_decoder       # MP3 解码器
        aac_decoder       # AAC 解码器
        wav_decoder       # WAV 解码器
        esp_codec_dev     # 编解码设备
)
```

## 依赖组件说明

| 组件 | 说明 |
|------|------|
| audio_pipeline | Pipeline 核心管理 |
| audio_stream | 音频流基础 |
| fatfs_stream | FATFS 文件系统流 |
| http_stream | HTTP 网络流 |
| i2s_stream | I2S 输出流 |
| mp3_decoder | MP3 解码器 |
| aac_decoder | AAC 解码器 |
| esp_codec_dev | 编解码设备抽象层 |

## 完整示例

```c
// main.c
#include "audio/audio_adf.h"
#include "esp_log.h"

static const char *TAG = "MAIN";

// 事件回调
void audio_event_handler(audio_adf_event_t event, void *user_data) {
    switch (event) {
        case AUDIO_ADF_EVENT_PLAYING:
            ESP_LOGI(TAG, "开始播放");
            break;
        case AUDIO_ADF_EVENT_COMPLETE:
            ESP_LOGI(TAG, "播放完成");
            break;
        case AUDIO_ADF_EVENT_ERROR:
            ESP_LOGE(TAG, "播放错误");
            break;
        default:
            break;
    }
}

void app_main(void)
{
    // ... 其他初始化 ...

    // 初始化 ADF 音频模块
    ESP_ERROR_CHECK(audio_adf_init_default());

    // 设置事件回调
    audio_adf_set_event_callback(audio_event_handler, NULL);

    // 播放音乐
    ESP_LOGI(TAG, "播放 /sdcard/music.mp3");
    audio_adf_play_file("/sdcard/music/River Flows in You.mp3");

    // 主循环
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        // 定期检查播放状态
        if (audio_adf_is_playing()) {
            audio_adf_info_t info;
            audio_adf_get_info(&info);
            ESP_LOGI(TAG, "播放中: %s", info.current_file);
        }
    }
}
```

## ESP-IDF 版本兼容性

| ESP-ADF 版本 | 支持的 ESP-IDF |
|-------------|----------------|
| v2.8 | v5.4, v5.5, v5.5.3 |
| v2.7 | v5.0, v5.1, v5.2, v5.3 |
| v2.6 | v4.4, v5.0, v5.1 |

检查你的 ESP-IDF 版本：

```bash
idf.py --version
```

## 故障排除

### 编译错误：找不到 audio_pipeline.h

```bash
# 确保已安装 ESP-ADF
. $ADF_PATH/export.sh

# 或者在 CMakeLists.txt 中添加路径
set(EXTRA_COMPONENT_DIRS $ENV{ADF_PATH}/components)
```

### 运行时崩溃

1. 检查 PSRAM 是否启用
2. 增加任务栈大小
3. 检查 I2S 引脚配置

### 播放无声音

1. 确认 I2S 引脚接线正确
2. 检查 DAC 芯片是否支持
3. 验证音频文件格式

## 相关资源

- [ESP-ADF 官方文档](https://docs.espressif.com/projects/esp-adf/en/latest/)
- [ESP-ADF GitHub](https://github.com/espressif/esp-adf)
- [Pipeline 示例](https://github.com/espressif/esp-adf/tree/release/v2.x/examples/player)
