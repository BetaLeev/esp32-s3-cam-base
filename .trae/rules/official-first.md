# 官方优先原则

## 核心原则

**遇到任何技术问题，首先查阅官方文档。找不到方案时，必须请示，绝不自己手写。**

---

## 一、代码来源优先级

### 1.1 技术方案来源排序

| 优先级 | 来源 | 说明 |
|-------|------|------|
| 1 | ESP-IDF 官方文档 | https://docs.espressif.com/projects/esp-idf/ |
| 2 | ESP-IDF 示例代码 | `$IDF_PATH/examples` |
| 3 | 芯片数据手册 | 芯片规格书、寄存器手册 |
| 4 | 组件官方仓库 | GitHubESP 官方仓库 |
| 5 | 成熟开源库 | 广泛验证的第三方库 |
| 6 | 其他来源 | 须经确认后才可使用 |

### 1.2 禁止的行为

```c
// ❌ 禁止：自己手写协议实现
uint8_t calculate_crc(uint8_t *data, int len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];  // 自己写的CRC算法，可能有bug
    }
    return crc;
}

// ✅ 正确：使用官方驱动/库
#include "driver/spi_master.h"
// ESP-IDF 官方SPI驱动，成熟稳定
```

```c
// ❌ 禁止：复制网上的不确定代码
// 从CSDN/博客复制来的文件系统代码，可能有安全漏洞

// ✅ 正确：使用ESP-IDF官方VFS接口
#include "esp_vfs_fat.h"
esp_vfs_fat_sdmmc_mount();
```

---

## 二、工作流程

### 2.1 遇到问题时的标准流程

```
发现需要实现的功能
    ↓
查阅 ESP-IDF 官方文档
    ↓
查找官方示例代码
    ↓
有 → 复制官方示例，适配项目
    ↓
无 → 查找官方驱动/库
    ↓
有 → 使用官方驱动/库
    ↓
无 → 请示确认后再决定
```

### 2.2 禁止擅自处理的情况

- 任何协议实现（I2C、SPI、UART等）
- 文件系统操作
- 网络通信（WiFi、BLE、以太网）
- 外设驱动（GPIO、ADC、PWM等）
- 安全相关（加密、认证、TLS）
- 内存管理
- 任务调度/中断处理

### 2.3 必须请示的场景

| 场景 | 处理方式 |
|------|---------|
| 官方文档找不到对应功能 | 请示后再决定 |
| 需要自定义协议 | 请示后再决定 |
| 需要修改官方示例 | 请示后再决定 |
| 需要使用第三方库 | 请示后再决定 |
| 遇到官方库的bug | 请示后再决定 |

---

## 三、官方文档查阅清单

### 3.1 ESP-IDF 必须查阅的章节

| 功能 | 官方文档路径 |
|------|------------|
| WiFi | `wifi-guide` |
| HTTP Server | `esp-http-server` |
| FAT文件系统 | `fatfs` |
| SD卡 | `sd-card` |
| ADC | `adc` |
| GPIO | `gpio` |
| I2C | `i2c` |
| SPI | `spi` |
| PWM/LEDC | `ledc` |
| FreeRTOS | `freertos` |

### 3.2 查阅顺序

1. **API Reference** - 函数接口说明
2. **Programming Guide** - 使用指南
3. **Examples** - 示例代码
4. **Hardware Reference** - 硬件说明

---

## 四、示例代码使用规范

### 4.1 官方示例优先级

```
官方 examples 文件夹中的示例 > 自己写的代码
```

### 4.2 使用官方示例的正确方式

```c
// ✅ 正确：基于官方示例修改
// 参考：$IDF_PATH/examples/storage/sd_card
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

// 在此基础上适配项目需求
```

### 4.3 禁止的模式

```c
// ❌ 禁止：看完网上教程自己实现
void my_sdcard_init(void) {
    // 根据博客文章自己写的初始化代码
    // 可能不符合ESP-IDF的最佳实践
}

// ❌ 禁止：复制部分代码拼凑
gpio_set_direction(GPIO_NUM_1, GPIO_MODE_OUTPUT);  // 从某处复制
sdmmc_host_init();  // 从另一处复制
// 不知道它们之间的正确调用顺序
```

---

## 五、例外情况

### 5.1 可以自行实现的情况

- 简单的数据转换函数（如结构体序列化）
- 业务逻辑代码（与硬件无关）
- UI/显示相关（使用官方图形库）
- 字符串处理（使用标准C库）

### 5.2 需要明确确认的

| 功能 | 是否可以使用第三方 |
|------|------------------|
| JSON解析 | ✅ cJSON（官方推荐） |
| 文件压缩 | ✅ miniz（广泛使用） |
| HTTP客户端 | ✅ esp-idf内置 |
| MQTT | ✅ esp-mqtt（官方） |
| OTA | ✅ esp_https_ota（官方） |

---

## 六、违反此规则的代价

1. **代码质量不可控** - 自写代码可能有隐藏bug
2. **安全风险** - 自写协议/加密可能被攻击
3. **维护困难** - 官方代码有社区支持
4. **兼容性问题** - 自写代码可能与官方库冲突

---

## 七、参考资源

### 7.1 必收藏链接

| 资源 | 链接 |
|------|------|
| ESP-IDF 编程指南 | https://docs.espressif.com/projects/esp-idf/ |
| ESP-IDF API参考 | https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/ |
| ESP-IDF 示例 | `$IDF_PATH/examples` |
| Espressif GitHub | https://github.com/espressif |
| ESP32技术参考手册 | 芯片数据手册 |

### 7.2 遇到问题时

1. 首先查阅官方文档
2. 在官方GitHub issues搜索类似问题
3. 查看ESP-IDF示例代码
4. **最后**才考虑其他方案
5. 任何不确定的情况 → **必须请示**
