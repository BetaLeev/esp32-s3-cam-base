# ESP32 传感器模块规范

## 概述

传感器模块采用**独立子模块**架构，每个传感器有独立的目录和文件，支持**引脚可配置**。

## 目录结构

```
src/sensors/
├── CMakeLists.txt           # 组件编译配置
├── sensors.h               # 统一入口头文件
├── sensors.c               # 统一入口实现
├── sensors_web.h           # Web API 头文件
├── sensors_web.c           # Web API 实现
├── thermistor/             # 热敏电阻子模块
│   ├── thermistor.h       # 头文件
│   └── thermistor.c       # 实现
├── photosensor/            # 光敏电阻子模块
│   ├── photosensor.h      # 头文件
│   └── photosensor.c      # 实现
└── dht11/                  # DHT11 温湿度子模块
    ├── dht11.h            # 头文件
    └── dht11.c            # 实现
```

## 子模块规范

### 1. 文件命名

| 类型 | 规则 | 示例 |
|------|------|------|
| 子模块目录 | 英文小写 | `thermistor/` |
| 子模块头文件 | `xxx.h` | `thermistor.h` |
| 子模块实现 | `xxx.c` | `thermistor.c` |

### 2. 头文件结构 (.h)

每个传感器子模块头文件必须包含：

```c
/**
 * @file xxx.h
 * @brief 传感器名称 传感器模块
 *
 * 简短描述传感器特性和使用方式
 */
#ifndef XXX_H
#define XXX_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

/* ========================================
 * 传感器配置 (默认引脚和参数)
 * ======================================== */

/**
 * @brief 默认引脚配置
 */
#ifndef XXX_DEFAULT_GPIO
#define XXX_DEFAULT_GPIO  GPIO_NUM_X
#endif

// ... 其他配置参数 ...

/* ========================================
 * 数据结构
 * ======================================== */

/**
 * @brief 传感器数据
 */
typedef struct {
    type1 field1;     /**< 字段1说明 */
    type2 field2;     /**< 字段2说明 */
} xxx_data_t;

/* ========================================
 * 接口函数
 * ======================================== */

/**
 * @brief 初始化传感器
 * @param gpio 引脚号
 * @return ESP_OK 成功
 */
esp_err_t xxx_init(gpio_num_t gpio);

/**
 * @brief 设置引脚
 * @param gpio GPIO 引脚号
 */
void xxx_set_gpio(gpio_num_t gpio);

/**
 * @brief 获取当前引脚
 * @return GPIO 引脚号
 */
gpio_num_t xxx_get_gpio(void);

/**
 * @brief 读取传感器数据
 * @param data 数据结构指针
 * @return ESP_OK 成功
 */
esp_err_t xxx_read(xxx_data_t *data);

/**
 * @brief 读取原始值
 * @return 原始 ADC 值或其他原始数据
 */
uint32_t xxx_read_raw(void);

/**
 * @brief 根据原始值计算物理量
 * @param raw_value 原始值
 * @return 物理量值
 */
float xxx_calculate(uint32_t raw_value);

#endif /* XXX_H */
```

### 3. 实现文件结构 (.c)

```c
/**
 * @file xxx.c
 * @brief 传感器名称 传感器实现
 */

#include "xxx.h"
#include "../../config.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"  // 或其他驱动
#include <math.h>

static const char *TAG = "XXX";

/* ========================================
 * 内部变量
 * ======================================== */

static adc_oneshot_unit_handle_t s_adc_handle = NULL;  // ADC 句柄
static adc_channel_t s_adc_channel = ADC_CHANNEL_X;    // ADC 通道
static gpio_num_t s_gpio = XXX_DEFAULT_GPIO;            // GPIO 引脚
static bool s_initialized = false;                     // 初始化标志

/* ========================================
 * 接口实现
 * ======================================== */

esp_err_t xxx_init(gpio_num_t gpio)
{
    s_gpio = gpio;
    // GPIO 到通道映射
    // ...

    esp_err_t ret = adc_init();
    if (ret == ESP_OK) {
        s_initialized = true;
        ESP_LOGI(TAG, "传感器初始化完成, GPIO%d", s_gpio);
    }
    return ret;
}

void xxx_set_gpio(gpio_num_t gpio)
{
    if (gpio == s_gpio) return;
    s_gpio = gpio;
    s_adc_handle = NULL;  // 重置句柄
    xxx_init(gpio);
}

gpio_num_t xxx_get_gpio(void)
{
    return s_gpio;
}

uint32_t xxx_read_raw(void)
{
    if (!s_initialized) {
        adc_init();
    }
    int raw_value = 0;
    esp_err_t ret = adc_oneshot_read(s_adc_handle, s_adc_channel, &raw_value);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC 读取失败: %s", esp_err_to_name(ret));
        return 0;
    }
    return (uint32_t)raw_value;
}

esp_err_t xxx_read(xxx_data_t *data)
{
    if (data == NULL) return ESP_ERR_INVALID_ARG;
    data->raw = xxx_read_raw();
    data->value = xxx_calculate(data->raw);
    return ESP_OK;
}

float xxx_calculate(uint32_t raw_value)
{
    // 传感器-specific 计算公式
}
```

## 引脚配置规范

### 默认引脚定义

所有默认引脚定义在 `src/config/hw_gpio.h` 中：

```c
#define GPIO_XXX_SENSOR     GPIO_NUM_X
```

### 配置优先级

1. **NVS 存储** (最高优先级) - 从 Flash 读取用户配置
2. **运行时设置** - 通过 API 动态设置 `xxx_set_gpio()`
3. **默认值** (最低优先级) - 使用头文件中的 `XXX_DEFAULT_GPIO`

### 引脚映射表

| 传感器 | 默认 GPIO | ADC 通道 | 说明 |
|--------|----------|----------|------|
| 热敏电阻 | GPIO5 | ADC_CHANNEL_4 | NTC 10K |
| 光敏电阻 | GPIO3 | ADC_CHANNEL_2 | LDR |
| DHT11 | GPIO4 | 无 | 单总线 |

### GPIO 与 ADC 通道映射

```
GPIO1  -> ADC_CHANNEL_0
GPIO2  -> ADC_CHANNEL_1
GPIO3  -> ADC_CHANNEL_2
GPIO4  -> ADC_CHANNEL_3
GPIO5  -> ADC_CHANNEL_4
GPIO6  -> ADC_CHANNEL_5
GPIO7  -> ADC_CHANNEL_6
GPIO8  -> ADC_CHANNEL_7
GPIO9  -> ADC_CHANNEL_8
GPIO10 -> ADC_CHANNEL_9
```

## Web API 规范

### 传感器数据 JSON 格式

```json
{
    "thermistor": {
        "gpio": 5,
        "raw": 2048,
        "temperature": 25.0
    },
    "photosensor": {
        "gpio": 3,
        "raw": 1500,
        "light": 350.0
    },
    "dht11": {
        "gpio": 4,
        "temperature": 25.0,
        "humidity": 60.0,
        "valid": true
    }
}
```

### API 端点

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/sensors/data` | 获取传感器数据 |
| GET | `/api/sensors/config` | 获取传感器配置 |
| POST | `/api/sensors/config` | 设置传感器引脚 |

## 添加新传感器

### 步骤 1: 创建目录和文件

```bash
mkdir -p src/sensors/new_sensor
touch src/sensors/new_sensor/new_sensor.h
touch src/sensors/new_sensor/new_sensor.c
```

### 步骤 2: 实现头文件

参考上述「头文件结构」编写 `new_sensor.h`

### 步骤 3: 实现源文件

参考上述「实现文件结构」编写 `new_sensor.c`

### 步骤 4: 添加默认引脚

在 `src/config/hw_gpio.h` 中添加：

```c
#define GPIO_NEW_SENSOR     GPIO_NUM_X
```

### 步骤 5: 更新 CMakeLists.txt

```cmake
idf_component_register(
    SRCS "sensors.c" "sensors_web.c"
        "thermistor/thermistor.c"
        "photosensor/photosensor.c"
        "dht11/dht11.c"
        "new_sensor/new_sensor.c"    # 添加这一行
    INCLUDE_DIRS "."
    REQUIRES esp_http_server driver freertos esp_adc
)
```

### 步骤 6: 更新统一入口

在 `sensors.h` 中添加：

```c
#include "new_sensor/new_sensor.h"
```

在 `sensors.c` 的 `sensors_init()` 中添加初始化调用。

## 错误处理

所有传感器函数必须遵循以下错误处理规范：

| 错误类型 | 返回值 | 说明 |
|----------|--------|------|
| 参数错误 | `ESP_ERR_INVALID_ARG` | NULL 指针或无效参数 |
| 超时 | `ESP_ERR_TIMEOUT` | 读取超时 |
| 校验失败 | `ESP_ERR_INVALID_CRC` | CRC 校验失败 |
| 初始化失败 | `ESP_FAIL` | 初始化错误 |

## 注意事项

1. **内存限制**: FreeRTOS Task 栈空间有限，禁止分配 > 512 字节的局部数组
2. **互斥访问**: 共享资源（如 ADC）需要使用互斥锁
3. **初始化检查**: 所有读取操作前检查初始化状态
4. **错误日志**: 使用 `ESP_LOGW` 记录可恢复错误，避免日志刷屏
5. **线程安全**: 传感器读取任务定期更新全局状态，确保数据一致性
