# ESP32-S3 FatFS 中文文件名支持配置指南

## 问题描述

在使用 ESP32-S3 的 TF 卡文件管理功能时，SD 卡上存储的中文文件名在 Web 界面显示为下划线（如 `_____~1.MP3`、`????~1.MP3`），无法正常显示中文。

## 根本原因

这是 ESP-IDF 中 FatFS 模块默认配置导致的问题：

1. **未开启长文件名（LFN）支持** - FatFS 默认使用 SFN（短文件名，8.3 格式）
2. **未配置中文代码页** - 默认使用英文代码页，遇到非 ASCII 字符会替换为下划线

## 解决方案

在 `platformio.ini` 的 `build_flags` 中添加以下配置：

```ini
build_flags =
    ; ... 其他配置 ...

    ; FatFS 长文件名和中文编码支持
    -DCONFIG_FATFS_LFN_HEAP=y
    -DCONFIG_FATFS_MAX_LFN=255
    -DCONFIG_FATFS_API_ENCODING_UTF_8=y
    -DCONFIG_FATFS_CODEPAGE_936=y
```

## 配置项说明

| 配置项 | 说明 |
|--------|------|
| `CONFIG_FATFS_LFN_HEAP=y` | 开启长文件名支持，使用堆内存存储缓冲区（推荐，避免占用栈空间） |
| `CONFIG_FATFS_MAX_LFN=255` | 最大文件名长度设置为 255 字符（标准 LFN 长度） |
| `CONFIG_FATFS_API_ENCODING_UTF_8=y` | API 编码设置为 UTF-8，确保与 Web 前端兼容 |
| `CONFIG_FATFS_CODEPAGE_936=y` | 启用简体中文（GBK/GB2312）代码页支持 |

## ESP-IDF menuconfig 等效配置

如果使用 `idf.py menuconfig`，配置路径为：

```
Component config → FAT Filesystem support
├── Long filename support: Buffer on heap          ← 开启 LFN
├── Max long filename length: 255                  ← 最大长度
├── API Characters set: UTF-8                      ← UTF-8 编码
└── OEM code page: Simplified Chinese (CP936)      ← 中文代码页
```

对应的 sdkconfig 选项：
```ini
CONFIG_FATFS_LFN_HEAP=y
CONFIG_FATFS_MAX_LFN=255
CONFIG_FATFS_API_ENCODING_UTF_8=y
CONFIG_FATFS_CODEPAGE_936=y
```

## 注意事项

1. **重新编译** - 修改配置后必须重新编译并烧录固件
2. **SD 卡格式化** - 如果 SD 卡之前以短文件名格式存储了中文文件，可能需要重新复制文件
3. **代码页互斥** - `CONFIG_FATFS_CODEPAGE_936=y` 和 `CONFIG_FATFS_CODEPAGE_DYNAMIC=y` 不能同时开启

## 验证方法

1. 烧录新固件后，观察串口日志确认初始化正常
2. 在 Web 界面的文件管理器中查看中文文件名
3. 正常情况下应显示完整的中文文件名，而不是下划线

## 相关文件

- 配置文件：`platformio.ini`
- SD 卡处理：`src/sdcard/sdcard_web.c`
