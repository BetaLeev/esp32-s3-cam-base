# ESP32 动态配置规范

> 避免硬编码依赖，优先使用动态计算

---

## 1. 核心原则

### 问题本质

```
代码需要 N 个 → 硬编码写成 40 → 配置文件限制为 8
              → 冲突！
```

### 一句话原则

> **能用 sizeof/计算得到的结果，绝不硬编码。**

---

## 2. 规则：动态计算 > 硬编码

### 错误示例

```c
// ❌ 硬编码 - 问题根源
config.max_uri_handlers = 40;
config.stack_size = 8192;
buffer_size = 1024;
```

### 正确示例

```c
// ✅ 动态计算 - 自动适配
static const httpd_uri_t uri_routes[] = { /* ... */ };
config.max_uri_handlers = sizeof(uri_routes) / sizeof(uri_routes[0]);
```

---

## 3. 规则：单一数据源原则

### 错误示例

```c
// ❌ 两个地方定义同一件事
#define MAX_ITEMS 10
static const int MAX_ITEMS = 10;  // 又定义一次，容易不同步
```

### 正确示例

```c
// ✅ 只在一处定义
static const httpd_uri_t uri_routes[] = { /* N 个路由 */ };
#define URI_COUNT (sizeof(uri_routes) / sizeof(uri_routes[0]))

// ✅ 使用计算后的值
queue_create(URI_COUNT);
```

---

## 4. 规则：配置与代码的优先级

| 位置 | 作用 |
|------|------|
| `sdkconfig` | 定义系统支持的**上限**（最大值） |
| 代码 | 根据实际需求计算**实际值** |

### 正确示例

```c
// sdkconfig: 设置系统支持的最大值
CONFIG_HTTPD_MAX_HANDLERS=64

// code: 根据实际使用量动态设置
static const httpd_uri_t uri_routes[] = { /* N 个 */ };
config.max_uri_handlers = sizeof(uri_routes) / sizeof(uri_routes[0]);
```

---

## 5. 规则：添加编译时断言

```c
#define ACTUAL_COUNT (sizeof(array) / sizeof(array[0]))

// 编译时检查
#if defined(CONFIG_HTTPD_MAX_HANDLERS) && ACTUAL_COUNT > CONFIG_HTTPD_MAX_HANDLERS
#error "URI handlers count exceeds sdkconfig limit"
#endif
```

---

## 6. 常见配置项检查清单

### HTTP 服务器

| 配置项 | 正确做法 |
|--------|----------|
| `max_uri_handlers` | `sizeof(uri_routes) / sizeof(uri_routes[0])` |
| `stack_size` | 根据 handler 复杂度估算 |
| `server_port` | 使用宏定义 |

### Wi-Fi

| 配置项 | 值 |
|--------|---|
| `WIFI_SSID_MAX_LEN` | 32（标准） |
| `WIFI_PASSWORD_MAX_LEN` | 64（标准） |

### FreeRTOS

| 配置项 | 正确做法 |
|--------|----------|
| `task_stack` | 估算每个任务需求 |
| `queue_length` | 实际消息类型数 |
| `max_task_name_len` | 默认 16 |

---

## 7. 快速检查流程

添加新代码时，问自己：

1. **这个数字是从哪里来的？**
   - 如果是硬编码 → 改成计算
   - 如果是计算来的 → 检查公式是否正确

2. **有没有地方定义过类似的值？**
   - 如果有 → 复用，不要重复定义

3. **有没有对应的 sdkconfig 配置？**
   - 如果有 → 确保代码值 ≤ 配置值

---

## 8. 本项目示例

### HTTP 服务器初始化

```c
esp_err_t http_server_init(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_PORT;
    config.stack_size = 8192;
    
    // ✅ 动态计算，不硬编码
    config.max_uri_handlers = sizeof(uri_routes) / sizeof(uri_routes[0]);

    httpd_handle_t server = NULL;
    return httpd_start(&server, &config);
}
```
