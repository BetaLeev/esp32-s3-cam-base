# ESP32 Web API JSON 处理规范

## 概述

本规范定义 ESP32 HTTP 服务器中 Web API 的 JSON 响应格式和处理方式，确保所有 API 具有一致的行为和可预测的错误处理。

## 目录结构

```
src/
├── web_module.h       # JSON 响应工具接口
├── web_module.c       # JSON 响应工具实现
├── cJSON/             # JSON 解析库 (components/cjson/)
│   ├── cJSON.c
│   ├── cJSON.h
│   └── CMakeLists.txt
└── xxx_web.c          # 各模块 Web API 实现
```

## 统一响应格式

### 成功响应

```json
{
    "status": "success",
    "code": 200,
    "message": "操作描述信息",
    "data": { ... }
}
```

### 错误响应

```json
{
    "status": "error",
    "code": 400,
    "message": "错误描述信息",
    "data": null
}
```

### 警告响应

```json
{
    "status": "warning",
    "code": 200,
    "message": "警告描述信息",
    "data": { ... }
}
```

## HTTP 状态码使用

| HTTP 状态码 | 含义 | 使用场景 |
|-------------|------|----------|
| 200 OK | 成功 | 正常响应 |
| 201 Created | 创建成功 | 资源创建成功 |
| 400 Bad Request | 请求错误 | 参数错误、无效 JSON |
| 404 Not Found | 资源不存在 | 找不到指定资源 |
| 500 Internal Server Error | 服务器错误 | 内部处理失败 |

## API 设计原则

### 1. 使用 cJSON 构建响应

**禁止使用 snprintf 拼接 JSON**，应使用 cJSON 库：

```c
// ❌ 错误：snprintf 拼接
char buf[256];
snprintf(buf, sizeof(buf), "{\"value\":%d}", value);
httpd_resp_send(req, buf, strlen(buf));

// ✅ 正确：使用 cJSON
cJSON *data = cJSON_CreateObject();
cJSON_AddNumberToObject(data, "value", value);
send_success(req, data, "获取成功");
```

### 2. 统一错误处理

```c
esp_err_t xxx_web_handler(httpd_req_t *req)
{
    // 1. 检查请求方法
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    // 2. 解析参数
    cJSON *json = parse_request_json(req);
    if (!json) {
        return send_bad_request(req, "无效的 JSON 格式");
    }

    // 3. 业务逻辑
    esp_err_t ret = do_something(json);
    if (ret != ESP_OK) {
        cJSON_Delete(json);
        return send_internal_error(req, "处理失败");
    }

    // 4. 成功响应
    cJSON *data = cJSON_CreateObject();
    // ... 添加数据 ...
    cJSON_Delete(json);

    return send_success(req, data, "操作成功");
}
```

### 3. 内存管理

```c
// 正确：cJSON 对象必须释放
cJSON *json = cJSON_Parse(buf);
if (json) {
    // 使用 json ...
    cJSON_Delete(json);  // 必须释放
}

// 错误示例
cJSON *json = cJSON_Parse(buf);
if (json) {
    cJSON_AddNumberToObject(obj, "key", 1);
    // 未释放 json
}
```

### 4. 参数验证

```c
// 检查必需参数
cJSON *param = cJSON_GetObjectItem(json, "required_param");
if (!cJSON_IsNumber(param)) {
    cJSON_Delete(json);
    return send_bad_request(req, "缺少必需参数: required_param");
}

// 验证参数范围
int gpio = param->valueint;
if (gpio < 0 || gpio > 48) {
    cJSON_Delete(json);
    return send_bad_request(req, "GPIO 编号超出范围 (0-48)");
}
```

## 常用工具函数

### 响应函数

| 函数 | 说明 |
|------|------|
| `send_success(req, data, msg)` | 发送成功响应 |
| `send_success_data(req, data)` | 发送成功响应（带默认消息） |
| `send_success_msg(req, msg)` | 发送成功响应（无数据） |
| `send_error(req, msg, code)` | 发送错误响应 |
| `send_bad_request(req, msg)` | 发送 400 错误 |
| `send_not_found(req, msg)` | 发送 404 错误 |
| `send_internal_error(req, msg)` | 发送 500 错误 |

### 解析函数

| 函数 | 说明 |
|------|------|
| `parse_request_json(req)` | 解析请求体 JSON |
| `get_query_string(req, key, buf, len)` | 获取 URL 查询参数 |

### JSON 辅助函数

| 函数 | 说明 |
|------|------|
| `json_add_int(obj, key, value)` | 添加整数字段 |
| `json_add_float(obj, key, value, decimals)` | 添加浮点数字段 |
| `json_add_bool(obj, key, value)` | 添加布尔字段 |
| `json_add_string(obj, key, value)` | 添加字符串字段 |

## API 设计模板

```c
/**
 * @brief API: 模块操作
 * METHOD /api/module/action
 */
esp_err_t module_web_handler(httpd_req_t *req)
{
    const char *TAG = "MODULE_WEB";

    // ========== 请求方法检查 ==========
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    // ========== 解析参数（可选）==========
    char param_buf[64];
    if (get_query_string(req, "param", param_buf, sizeof(param_buf))) {
        ESP_LOGI(TAG, "参数: %s", param_buf);
    }

    // ========== 业务处理 ==========
    // ... 业务逻辑 ...

    // ========== 构建响应 ==========
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return send_internal_error(req, "创建响应数据失败");
    }

    cJSON_AddStringToObject(data, "result", "success");
    cJSON_AddNumberToObject(data, "timestamp", esp_timer_get_time() / 1000000);

    // ========== 发送响应 ==========
    return send_success(req, data, "操作成功");
}
```

## 新增模块检查清单

### 添加新 Web API

- [ ] 引入 web_module.h
- [ ] 使用 `send_success()` / `send_error()` 发送响应
- [ ] 使用 cJSON 构建数据对象
- [ ] 正确释放 cJSON 对象
- [ ] 验证请求参数
- [ ] 在 http_server.c 中注册 URI

### 添加新模块目录

```
src/
└── new_module/
    ├── CMakeLists.txt
    ├── new_module.h
    ├── new_module.c
    ├── new_module_web.h
    └── new_module_web.c
```

## 注意事项

1. **缓冲区限制**：请求体解析使用 1024 字节缓冲区，大数据应使用其他方式
2. **日志级别**：API 成功响应不打印日志，错误使用 `ESP_LOGW`，严重错误使用 `ESP_LOGE`
3. **超时处理**：接收请求体超时返回 `HTTPD_SOCK_ERR_TIMEOUT`
4. **线程安全**：Web API 在 HTTP 服务器任务中执行，注意任务栈空间

## 示例：完整 API 实现

```c
#include "../web_module.h"

esp_err_t sensors_web_get_handler(httpd_req_t *req)
{
    cJSON *data = cJSON_CreateObject();
    if (!data) {
        return send_internal_error(req, "内存分配失败");
    }

    // 添加传感器数据
    cJSON *sensor1 = cJSON_CreateObject();
    cJSON_AddNumberToObject(sensor1, "value", get_sensor_value());
    cJSON_AddItemToObject(data, "sensor1", sensor1);

    return send_success(req, data, "获取成功");
}
```
