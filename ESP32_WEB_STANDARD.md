# ESP32 LittleFS + HTTP Server 标准化架构文档

> 本文档定义了 ESP32 项目使用 LittleFS 文件系统存储前端静态文件的标准化架构。
> 可作为未来 ESP32 Web 项目开发的模板和参考。

---

## 目录

1. [项目目录结构](#1-项目目录结构)
2. [核心文件说明](#2-核心文件说明)
3. [分区表配置](#3-分区表配置)
4. [文件系统初始化](#4-文件系统初始化)
5. [HTTP 服务器配置](#5-http-服务器配置)
6. [前端文件组织](#6-前端文件组织)
7. [编译和烧录](#7-编译和烧录)
8. [API 接口设计](#8-api-接口设计)
9. [关键注意事项](#9-关键注意事项)

---

## 1. 项目目录结构

```
esp32_project/
├── src/
│   ├── main.c                 # 主程序入口
│   ├── http_server.c          # HTTP 服务器实现
│   ├── http_server.h          # HTTP 服务器头文件
│   ├── spiffs_web.c           # SPIFFS 文件系统实现
│   ├── spiffs_web.h           # SPIFFS 文件系统头文件
│   ├── config.h               # 配置文件
│   ├── wifi_app.c             # Wi-Fi 应用
│   ├── motor_ctrl.c           # 电机控制
│   ├── sensor_*.c             # 传感器驱动
│   └── web/                   # 前端静态文件（重点）
│       ├── index.html         # 主页面
│       ├── styles.css         # 样式文件
│       └── app.js             # JavaScript
├── src/CMakeLists.txt         # 组件构建配置
├── default.csv                # 分区表
├── sdkconfig                  # ESP-IDF 配置
└── CMakeLists.txt             # 项目构建配置
```

---

## 2. 核心文件说明

### 2.1 spiffs_web.h（文件系统接口）

```c
#ifndef SPIFFS_WEB_H
#define SPIFFS_WEB_H

#include "esp_err.h"

/**
 * @brief Web 文件系统配置
 */
#define WEBFS_BASE_PATH "/spiffs"
#define WEBFS_PARTITION_LABEL "webfs"

/**
 * @brief 初始化 Web 文件系统
 */
esp_err_t spiffs_web_init(void);

/**
 * @brief 读取文件内容
 * @param path 文件路径（相对于 /spiffs）
 * @param out_buffer 输出缓冲区
 * @param out_len 输出长度
 */
esp_err_t spiffs_web_read_file(const char *path, char **out_buffer, size_t *out_len);

#endif
```

### 2.2 spiffs_web.c（文件系统实现）

- 使用 `esp_vfs_spiffs_register()` 注册 SPIFFS VFS
- 挂载点：`/spiffs`
- 分区标签：`webfs`
- 提供文件读取接口

### 2.3 http_server.c（HTTP 服务器）

- 处理静态文件请求（index.html, styles.css, app.js）
- 提供 REST API 接口
- 通过 `spiffs_web_read_file()` 读取前端文件

---

## 3. 分区表配置

### 3.1 分区表文件（default.csv）

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 1M,
webfs,    data, spiffs, 0x110000,0x40000,
```

### 3.2 分区说明

| 分区名称 | 类型 | 大小 | 说明 |
|---------|------|------|------|
| nvs | data | 24KB | NVS 存储（Wi-Fi 配置等） |
| phy_init | data | 4KB | 物理层初始化数据 |
| factory | app | 1MB | 应用程序 |
| webfs | data | 256KB | SPIFFS 文件系统（前端文件） |

---

## 4. 文件系统初始化

### 4.1 在 main.c 中初始化

```c
#include "spiffs_web.h"

// 在 system_init() 函数中添加
ESP_LOGI(TAG, ">>> [6/9] 初始化SPIFFS文件系统...");
ret = spiffs_web_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "    [失败] SPIFFS: %s", esp_err_to_name(ret));
    return ret;
}
ESP_LOGI(TAG, "    [成功] SPIFFS文件系统");
```

### 4.2 初始化顺序

```
1. NVS Flash
2. 硬件驱动（电机、传感器等）
3. Wi-Fi
4. SPIFFS 文件系统（必须在 HTTP 服务器之前）
5. HTTP 服务器
6. 创建任务
```

---

## 5. HTTP 服务器配置

### 5.1 路由配置

```c
static const httpd_uri_t uri_routes[] = {
    {.uri = "/",              .method = HTTP_GET, .handler = index_handler},
    {.uri = "/index.html",    .method = HTTP_GET, .handler = index_handler},
    {.uri = "/styles.css",    .method = HTTP_GET, .handler = css_handler},
    {.uri = "/app.js",        .method = HTTP_GET, .handler = js_handler},
    {.uri = "/api/status",    .method = HTTP_GET, .handler = api_status_handler},
    {.uri = "/api/control",   .method = HTTP_GET, .handler = api_control_handler},
};
```

### 5.2 文件处理 Handler 示例

```c
static esp_err_t index_handler(httpd_req_t *req)
{
    char *buffer = NULL;
    size_t len = 0;

    esp_err_t ret = spiffs_web_read_file("index.html", &buffer, &len);
    if (ret != ESP_OK) {
        const char *error_page = "<html><body><h1>404</h1></body></html>";
        httpd_resp_send(req, error_page, strlen(error_page));
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, buffer, len);
    free(buffer);

    return ESP_OK;
}
```

---

## 6. 前端文件组织

### 6.1 前端文件位置

所有前端文件必须放在 `src/web/` 目录下：

```
src/web/
├── index.html    # 主页面
├── styles.css    # 样式表
└── app.js        # JavaScript 逻辑
```

### 6.2 修改前端文件

**重要：修改前端文件后不需要修改任何 C 代码！**

前端文件通过 CMakeLists.txt 自动打包到 SPIFFS 分区。

### 6.3 CMakeLists.txt 配置

```cmake
# 指定要打包到 SPIFFS 的文件
set(spiffs_files 
    ${CMAKE_SOURCE_DIR}/src/web/index.html
    ${CMAKE_SOURCE_DIR}/src/web/styles.css
    ${CMAKE_SOURCE_DIR}/src/web/app.js
)

# 创建 SPIFFS 镜像生成命令
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/spiffs_image.bin
    COMMAND python ${IDF_PATH}/components/spiffs/spiffsgen.py
        0x40000  # 分区大小
        ${CMAKE_SOURCE_DIR}/src/web
        ${CMAKE_BINARY_DIR}/spiffs_image.bin
    DEPENDS ${spiffs_files}
    COMMENT "生成 SPIFFS 镜像..."
)

# 添加依赖
add_dependencies(app ${CMAKE_BINARY_DIR}/spiffs_image.bin)
```

---

## 7. 编译和烧录

### 7.1 编译步骤

```bash
# 清理并重新编译
idf.py clean
idf.py build

# 或者直接编译
idf.py build
```

### 7.2 烧录步骤

```bash
# 烧录所有分区
idf.py flash

# 烧录并监控
idf.py flash monitor
```

### 7.3 查看日志

```bash
idf.py monitor
```

---

## 8. API 接口设计

### 8.1 获取状态

```
GET /api/status

Response:
{
    "temperature": 25.5,
    "humidity": 60.0,
    "pump_state": 1,
    "pump_speed": 75
}
```

### 8.2 控制设备

```
GET /api/pump?action=on&speed=80

Response:
{
    "success": true,
    "state": "on",
    "speed": 80
}
```

---

## 9. 关键注意事项

### 9.1 初始化顺序

**必须先初始化 SPIFFS，再启动 HTTP 服务器！**

### 9.2 前端文件修改

- 修改前端文件后**不需要修改 C 代码**
- 重新编译会自动打包
- 使用 `idf.py clean && idf.py build` 确保更新

### 9.3 内存管理

- `spiffs_web_read_file()` 返回的缓冲区需要手动 `free()`
- HTTP handler 中读取文件后及时释放内存

### 9.4 分区大小

- 根据前端文件大小调整 `default.csv` 中的 `webfs` 分区大小
- 当前配置：256KB（对于大多数 Web UI 足够）

### 9.5 文件系统挂载

- 首次烧录或格式化失败时会自动格式化
- 挂载点：`/spiffs`
- 文件路径：`/spiffs/index.html`

---

## 10. 故障排查

### 10.1 文件读取失败

检查：
- SPIFFS 是否初始化成功
- 文件是否在 `src/web/` 目录下
- CMakeLists.txt 是否正确配置

### 10.2 浏览器显示 404

检查：
- HTTP 服务器是否启动成功
- 分区表是否正确烧录
- 文件路径是否正确

### 10.3 编译错误

检查：
- sdkconfig 是否包含 SPIFFS 配置
- 分区表文件是否存在
- CMakeLists.txt 语法是否正确

---

## 附录：快速开始模板

### A.1 新建项目步骤

1. 创建项目目录结构
2. 复制 `default.csv` 分区表
3. 配置 `CMakeLists.txt`（包含 SPIFFS 配置）
4. 实现 `spiffs_web.c/h`
5. 实现 `http_server.c`（包含文件 handler）
6. 在 `main.c` 中初始化 SPIFFS
7. 创建前端文件到 `src/web/`
8. 编译并烧录

### A.2 最小示例

参见本项目的实现：
- `src/spiffs_web.c/h` - 文件系统实现
- `src/http_server.c` - HTTP 服务器实现
- `src/main.c` - 初始化代码
- `src/web/` - 前端文件

---

**文档版本**: v1.0  
**最后更新**: 2026-07-07  
**适用版本**: ESP-IDF 6.0+
