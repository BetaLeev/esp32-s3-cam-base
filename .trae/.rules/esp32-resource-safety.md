# ESP32 资源安全编程规范

> 本项目 ESP32-S3 开发资源管理规范

---

## 1. 互斥锁 (Mutex) 规范

### 1.1 黄金法则

```
获取锁 → 使用资源 → 释放锁
```

**每一条获取路径都必须有对应的释放路径！**

### 1.2 标准模板

```c
static SemaphoreHandle_t s_mutex = NULL;
static volatile bool s_busy = false;
static uint32_t s_request_count = 0;

static bool try_lock(uint32_t timeout_ms) {
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
    }
    BaseType_t acquired = xSemaphoreTake(s_mutex, pdMS_TO_TICKS(timeout_ms));
    if (acquired == pdTRUE) {
        s_busy = true;
        s_request_count++;
        return true;
    }
    ESP_LOGW(TAG, "资源忙，等待超时");
    return false;
}

static void unlock(void) {
    s_busy = false;
    if (s_mutex != NULL) {
        xSemaphoreGive(s_mutex);
    }
}
```

### 1.3 HTTP 处理函数模板

```c
esp_err_t protected_handler(httpd_req_t *req) {
    // 1. 获取锁
    if (!try_lock(5000)) {
        httpd_resp_send_err(req, HTTPD_503_SERVICE_UNAVAILABLE, "资源忙");
        return ESP_FAIL;
    }

    // 2. 分配内存
    char *buf = malloc(BUFFER_SIZE);
    if (buf == NULL) {
        unlock();
        return ESP_FAIL;
    }

    // 3. 打开文件（如果有）
    FIL file;
    bool file_opened = false;
    if (f_open(&file, path, FA_READ) != FR_OK) {
        free(buf);
        unlock();
        return ESP_FAIL;
    }
    file_opened = true;

    // 4. 正常处理...

    // 5. 清理
    if (file_opened) f_close(&file);
    free(buf);
    unlock();
    return ESP_OK;
}
```

### 1.4 常见错误 ⚠️

```c
// ❌ 错误1：获取锁后提前返回忘记释放
if (!try_lock(5000)) return ESP_FAIL;
if (error) return ESP_FAIL;  // 忘记 unlock！

// ❌ 错误2：内存分配失败忘记释放锁
if (!try_lock(5000)) return ESP_FAIL;
char *buf = malloc(SIZE);
if (buf == NULL) return ESP_FAIL;  // 忘记 unlock！

// ❌ 错误3：函数结尾忘记释放锁
if (!try_lock(5000)) return ESP_FAIL;
// ... 处理逻辑 ...
return ESP_OK;  // 忘记 unlock！
```

### 1.5 检查清单

- [ ] 函数入口获取锁
- [ ] 每个 `return ESP_FAIL` 之前有 `unlock()`
- [ ] 每个 `return ESP_OK` 之前有 `unlock()`
- [ ] 内存/malloc 失败路径有 `unlock()`
- [ ] 文件/f_open 失败路径有 `unlock()`
- [ ] 目录/f_opendir 失败路径有 `unlock()`

---

## 2. 内存分配规范

### 2.1 malloc/free 配对

```c
// ❌ 错误
char *buf = malloc(1024);
if (error) return ESP_FAIL;  // 泄漏！

// ✅ 正确
char *buf = malloc(1024);
if (error) {
    free(buf);
    return ESP_FAIL;
}
free(buf);
```

### 2.2 缓冲区大小

| 场景 | 大小 | 说明 |
|------|------|------|
| JSON 响应 | 8KB | 避免栈溢出 |
| 文件上传 | 32KB | 平衡性能 |
| 文件下载 | 2-4KB | 减少内存 |

---

## 3. 文件句柄规范

```c
// f_open/f_close 配对
FIL file;
bool opened = false;
if (f_open(&file, path, FA_READ) != FR_OK) {
    return ESP_FAIL;
}
opened = true;
// ...
if (opened) f_close(&file);

// f_opendir/f_closedir 配对
FF_DIR dir;
if (f_opendir(&dir, path) != FR_OK) {
    return ESP_FAIL;
}
f_closedir(&dir);
```

---

## 4. HTTP 错误码

```c
#ifndef HTTPD_503_SERVICE_UNAVAILABLE
#define HTTPD_503_SERVICE_UNAVAILABLE 503
#endif
```

| 错误码 | 含义 |
|--------|------|
| HTTPD_400 | 请求参数错误 |
| HTTPD_404 | 资源不存在 |
| HTTPD_500 | 服务器内部错误 |
| HTTPD_503 | 服务不可用 |

---

## 5. 本项目 SD 卡互斥锁

| 函数 | 超时 | 说明 |
|------|------|------|
| files_handler | 5000ms | 文件列表 |
| download_handler | 5000ms | 下载 |
| upload_handler | 30000ms | 上传 |
| mkdir_handler | 5000ms | 创建目录 |
| delete_handler | 5000ms | 删除 |

---

## 6. 代码审查清单

### 互斥锁
- [ ] 每条获取路径都有释放
- [ ] 所有 return 前都释放锁

### 内存
- [ ] malloc 有 free
- [ ] 失败时正确处理

### 文件
- [ ] f_open 有 f_close
- [ ] f_opendir 有 f_closedir

### HTTP
- [ ] 错误情况发送错误响应
- [ ] Content-Type 含 charset=utf-8
