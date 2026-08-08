# 防止复制粘贴开发模式规则

## 核心原则

**复制粘贴 = 重构信号**。当你发现自己在复制代码时，说明需要一个共享的抽象。

---

## 一、代码复用规范

### 1.1 工具函数必须抽象

以下功能**必须**提取为公共函数，禁止在多个文件中复制相同代码：

| 功能 | 公共位置 | 函数名 |
|------|---------|--------|
| 路径拼接 | `utils/path_utils.h` | `path_join()`, `path_build_fatfs()`, `path_build_vfs()` |
| 文件扩展名 | `utils/path_utils.h` | `path_get_extension()` |
| URL解码 | `utils/url_utils.h` | `url_decode()` |
| 时间戳转换 | `utils/time_utils.h` | `time_fatfs_to_unix()` |
| 文件大小格式化 | `utils/str_utils.h` | `format_size()` |
| MIME类型 | `utils/mime_utils.h` | `mime_get_type()` |

### 1.2 复制前的思考流程

```
发现问题需要某功能
    ↓
检查 utils/ 目录下是否有现成实现
    ↓
无 → 创建 utils/xxx_utils.h 实现
    ↓
是 → include 并使用
    ↓
如果现有实现不满足需求 → 扩展它，而不是复制
```

---

## 二、禁止复制粘贴模式

### 2.1 禁止的模式

```c
// ❌ 禁止：复制代码后修改
esp_err_t handler_a(httpd_req_t *req) {
    char buf[256];
    // ... 复杂逻辑
    return result;
}

esp_err_t handler_b(httpd_req_t *req) {
    char buf[256];  // 复制了 handler_a 的代码
    // ... 几乎相同的逻辑
    return result;
}
```

### 2.2 正确的模式

```c
// ✅ 正确：提取公共逻辑为函数
static esp_err_t common_operation(char *output, size_t size) {
    // 公共逻辑
    return ESP_OK;
}

esp_err_t handler_a(httpd_req_t *req) {
    char buf[256];
    return common_operation(buf, sizeof(buf));
}

esp_err_t handler_b(httpd_req_t *req) {
    char buf[256];
    return common_operation(buf, sizeof(buf));
}
```

---

## 三、模块内代码复用

### 3.1 同模块内复用

同目录下的 `.c` 文件可以相互调用，但必须通过头文件暴露接口：

```
src/sdcard/
├── sdcard.h        // 公共接口
├── sdcard.c        // 核心实现
├── sdcard_web.h    // Web API 接口
└── sdcard_web.c    // Web API 实现 → 使用 sdcard.c 的函数
```

### 3.2 Web Handler 模板

同类 Web Handler 应使用统一模板：

```c
/**
 * @brief API: 描述
 * METHOD /api/module/action
 */
esp_err_t module_web_ACTION_handler(httpd_req_t *req)
{
    // 1. 方法检查
    if (req->method != HTTP_GET) {
        return send_bad_request(req, "仅支持 GET 请求");
    }

    // 2. 参数获取
    char param[64] = {0};
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        httpd_query_key_value(buf, "key", param, sizeof(param));
    }

    // 3. 参数验证
    if (param[0] == '\0') {
        return send_bad_request(req, "缺少必需参数");
    }

    // 4. 业务逻辑
    // ...

    // 5. 响应
    return send_success(req, data, "操作成功");
}
```

---

## 四、代码审查清单

复制代码前必须确认：

- [ ] 该功能是否已存在于 utils/ 或其他模块？
- [ ] 能否通过参数化或回调来通用化现有代码？
- [ ] 复制的代码是否会引入重复逻辑？
- [ ] 如果未来需要修改，复制的代码是否需要修改多处？

---

## 五、违反此规则的代价

1. **代码膨胀**：每复制一次，未来维护成本翻倍
2. **不一致性**：不同副本可能逐渐分化
3. **Bug 传播**：一个 Bug 可能存在于多个副本中
4. **技术债务**：积累到一定程度必须重构

---

## 六、工具函数目录结构

```
src/utils/
├── utils.h              // 工具函数总头文件
├── path_utils.h         // 路径处理
├── path_utils.c
├── url_utils.h          // URL编解码
├── url_utils.c
├── str_utils.h          // 字符串处理
├── str_utils.c
├── mime_utils.h         // MIME类型
├── mime_utils.c
└── time_utils.h         // 时间处理
    └── time_utils.c
```
