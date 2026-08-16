# LittleFS 链接冲突修复计划

## 问题分析

编译时出现链接错误，原因是同时链接了两个 LittleFS 库：
1. ESP-IDF 内置的 LittleFS: `.pio/build/esp32-s3-cam/esp-idf/littlefs/liblittlefs.a`
2. 手动添加的 LittleFS 组件: `components/littlefs/`

这导致符号重复定义和链接冲突。

## 解决方案

### 步骤 1: 删除手动添加的 LittleFS 组件

```bash
rm -rf /Users/v/Desktop/esp32_demo/components/littlefs
```

### 步骤 2: 确保 ESP-IDF 内置 LittleFS 正确配置

在 `sdkconfig.defaults` 中已添加 LittleFS 配置，确保包含：
```
CONFIG_LITTLEFS=y
CONFIG_LITTLEFS_PARTITION_LABEL="webfs"
CONFIG_LITTLEFS_PAGE_SIZE=256
CONFIG_LITTLEFS_OBJ_NAME_LEN=64
CONFIG_LITTLEFS_READ_SIZE=256
CONFIG_LITTLEFS_WRITE_SIZE=256
CONFIG_LITTLEFS_LOOKAHEAD_SIZE=128
CONFIG_LITTLEFS_CACHE_SIZE=512
CONFIG_LITTLEFS_BLOCK_CYCLES=512
```

### 步骤 3: 清理 PlatformIO 构建缓存

在 PlatformIO 中执行 "Clean" 然后重新编译。

## 需要修改的文件

无（仅需删除目录和清理缓存）

## 验证步骤

1. 重新编译项目
2. 观察串口日志确认 LittleFS 挂载成功
3. 访问 IP 地址测试前端页面加载
