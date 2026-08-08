# ESP32 ESP-IDF 组件依赖规则

## 问题症状

新增 C 源文件后编译报错：
```
undefined reference to `esp_wifi_get_ap_num'
undefined reference to `esp_wifi_get_ap_list'
```

## 根本原因

ESP-IDF 组件必须显式声明依赖，未声明会导致链接器找不到符号。

## ⚠️ PlatformIO ESP-IDF 项目配置位置

**不是** `src/CMakeLists.txt`（那是原生 ESP-IDF 项目用的）

**正确位置是** `platformio.ini` 的 `board_build.esp-idf.components`：

```ini
board_build.esp-idf.components = esp_wifi esp_netif lwip ...
```

## 依赖检查流程

新增 `.c` 文件时：

1. 检查文件中的 `#include` 头文件
2. 确认头文件对应的 ESP-IDF 组件
3. 在 `platformio.ini` 添加组件到 `board_build.esp-idf.components`

## 头文件 → 组件对照表

| 头文件 | 组件 | 用途 |
|---------|------|------|
| `esp_wifi.h` | `esp_wifi` | WiFi 扫描/连接 |
| `nvs_flash.h`, `nvs.h` | `nvs_flash` | NVS 存储 |
| `esp_http_server.h` | `esp_http_server` | HTTP 服务器 |
| `driver/gpio.h`, `driver/ledc.h` | `driver` | GPIO/PWM/I2C/SPI |
| `fatfs.h` | `fatfs` | FAT 文件系统 |
| `esp_spiffs.h` | `spiffs` | SPIFFS 文件系统 |
| `esp_adc/*.h` | `esp_adc` | ADC 模数转换 |
| `esp_netif.h` | `esp_netif` | 网络接口 |

## 当前项目配置

`platformio.ini` 中已声明：
```ini
board_build.esp-idf.components = esp_wifi esp_netif lwip nvs_flash esp_http_server esp_timer freertos fatfs spiffs esp_adc driver mbedtls
```

## 修复示例

**新增 `wifi_config.c` 包含**：
```c
#include "esp_wifi.h"
#include "nvs_flash.h"
```

**必须执行**：在 `platformio.ini` 添加：
```ini
board_build.esp-idf.components = ... esp_wifi nvs_flash
```

## 快速检查命令

```bash
# 检查新增文件用了哪些头文件
grep "#include" src/xxx.c

# 检查 platformio.ini 是否包含某组件
grep "esp_wifi" platformio.ini
```
