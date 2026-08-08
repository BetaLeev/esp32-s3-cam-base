# 安全编程规则

## 核心原则

**永远不要信任用户输入**。所有来自外部的数据都必须经过验证和清理。

---

## 一、输入验证规范

### 1.1 HTTP 请求参数验证

```c
// ❌ 危险：直接使用用户输入
snprintf(path, sizeof(path), "0:/sdcard/%s", user_input);

// ✅ 安全：验证并清理输入
#define MAX_PATH_COMPONENT_LEN 64
#define MAX_PATH_DEPTH 8

// 1. 检查危险字符
if (strstr(path, "..") != NULL) {
    return send_error(req, "非法路径", HTTP_BAD_REQUEST);
}

// 2. 检查路径深度
int depth = 0;
for (char *p = path; *p; p++) {
    if (*p == '/') depth++;
}
if (depth > MAX_PATH_DEPTH) {
    return send_error(req, "路径过深", HTTP_BAD_REQUEST);
}

// 3. 检查长度
if (strlen(path) > MAX_PATH_COMPONENT_LEN) {
    return send_error(req, "路径过长", HTTP_BAD_REQUEST);
}
```

### 1.2 路径遍历攻击防护

**禁止的模式**：
```c
// ❌ 这些都是危险的
"../../../etc/passwd"
"..%2F..%2F..%2Fetc%2Fpasswd"  // URL编码的..
"....//....//....//etc/passwd"  // 双点
"\..\..\..\windows\system32"     // Windows风格
```

**强制检查**：
```c
/**
 * @brief 验证路径安全性
 * @param path 用户输入的路径
 * @return true 安全, false 危险
 */
static bool is_path_safe(const char *path) {
    if (path == NULL || strlen(path) == 0) {
        return true;  // 空路径视为根目录
    }

    // 检查危险模式
    const char *dangerous[] = {
        "..", "%2e%2e", "%252e", "....",
        "\\", "//", "/.", "./",
    };

    char lower[256];
    snprintf(lower, sizeof(lower), "%s", path);
    for (size_t i = 0; lower[i]; i++) {
        lower[i] = tolower(lower[i]);
    }

    for (size_t i = 0; i < sizeof(dangerous)/sizeof(dangerous[0]); i++) {
        if (strstr(lower, dangerous[i]) != NULL) {
            return false;
        }
    }

    // 检查路径深度
    int depth = 0;
    for (const char *p = path; *p; p++) {
        if (*p == '/') depth++;
    }
    if (depth > MAX_ALLOWED_DEPTH) {
        return false;
    }

    return true;
}
```

---

## 二、缓冲区安全规范

### 2.1 禁止大数组在栈上

```c
// ❌ 危险：超过512字节的栈数组
void func(void) {
    char buf[1024];       // 禁止！
    char buf[4096];       // 绝对禁止！
    uint8_t data[2048];   // 禁止！
}

// ✅ 正确：使用堆内存
void func(void) {
    char *buf = malloc(4096);
    if (buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    // ... 使用 buf
    free(buf);
}
```

### 2.2 snprintf 安全使用

```c
// ✅ 正确：始终指定目标缓冲区大小
char path[256];
snprintf(path, sizeof(path), "%s/%s", base, name);

// ❌ 危险：没有指定大小
// snprintf(path, ???, "%s/%s", base, name);
```

### 2.3 动态缓冲区管理

```c
// ✅ 正确的内存管理模式
esp_err_t safe_operation(size_t needed_size) {
    // 1. 分配
    char *buf = malloc(needed_size);
    if (buf == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = ESP_OK;
    FILE *f = NULL;

    do {
        // 2. 使用
        f = fopen("test.bin", "rb");
        if (f == NULL) {
            ret = ESP_FAIL;
            break;
        }

        // ... 操作

    } while (0);

    // 3. 清理 - 顺序很重要
    if (f != NULL) {
        fclose(f);
    }
    free(buf);  // 最后释放内存

    return ret;
}
```

---

## 三、并发安全规范

### 3.1 互斥锁初始化

```c
// ❌ 危险：竞态条件
static SemaphoreHandle_t s_mutex = NULL;

static SemaphoreHandle_t get_mutex(void) {
    if (s_mutex == NULL) {  // 多个线程可能同时进入
        s_mutex = xSemaphoreCreateMutex();
    }
    return s_mutex;
}

// ✅ 正确：使用静态初始化或保护初始化
static SemaphoreHandle_t s_mutex = NULL;
static StaticSemaphore_t s_mutex_buffer;
static bool s_mutex_initialized = false;

// 方法1：静态初始化（推荐）
#define INIT_MUTEX() do { \
    static StaticSemaphore_t xMutexBuffer; \
    static SemaphoreHandle_t xMutex = NULL; \
    if (xMutex == NULL) { \
        xMutex = xSemaphoreCreateMutexStatic(&xMutexBuffer); \
    } \
} while(0)

// 方法2：运行时初始化带保护
static SemaphoreHandle_t get_mutex_safe(void) {
    static SemaphoreHandle_t mutex = NULL;
    static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

    portENTER_CRITICAL(&spinlock);
    if (mutex == NULL) {
        mutex = xSemaphoreCreateMutex();
    }
    portEXIT_CRITICAL(&spinlock);

    return mutex;
}
```

### 3.2 临界区保护

```c
// ✅ 使用互斥锁保护临界区
esp_err_t sdcard_operation(void) {
    SemaphoreHandle_t mutex = get_mutex_safe();
    if (mutex == NULL) {
        return ESP_ERR_NOT_INITIALIZED;
    }

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    // ... 临界区操作 ...

    xSemaphoreGive(mutex);
    return ret;
}
```

### 3.3 资源获取即初始化 (RAII 模式)

```c
// ✅ 使用结构体封装锁管理
typedef struct {
    SemaphoreHandle_t mutex;
    bool acquired;
} sd_lock_t;

static sd_lock_t sd_lock_create(void) {
    return (sd_lock_t){
        .mutex = get_mutex_safe(),
        .acquired = false
    };
}

static bool sd_lock_acquire(sd_lock_t *lock, uint32_t timeout_ms) {
    if (lock == NULL || lock->mutex == NULL) {
        return false;
    }
    lock->acquired = xSemaphoreTake(lock->mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
    return lock->acquired;
}

static void sd_lock_release(sd_lock_t *lock) {
    if (lock != NULL && lock->acquired && lock->mutex != NULL) {
        xSemaphoreGive(lock->mutex);
        lock->acquired = false;
    }
}

// 使用示例
esp_err_t safe_sdcard_read(void) {
    sd_lock_t lock = sd_lock_create();
    if (!sd_lock_acquire(&lock, 5000)) {
        return ESP_ERR_TIMEOUT;
    }

    // ... 操作 ...

    sd_lock_release(&lock);  // 确保释放
    return ESP_OK;
}
```

---

## 四、文件操作安全规范

### 4.1 路径验证流程

```c
// ✅ 完整的路径验证流程
esp_err_t safe_file_operation(const char *user_path, const char *mount_point) {
    // 1. 基础检查
    if (user_path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 2. URL解码（如果来自HTTP请求）
    char decoded[256];
    url_decode_inplace(user_path, decoded, sizeof(decoded));

    // 3. 路径安全检查
    if (!is_path_safe(decoded)) {
        return ESP_ERR_INVALID_ARG;
    }

    // 4. 限制长度
    if (strlen(decoded) > MAX_SAFE_PATH_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    // 5. 构建完整路径
    char full_path[512];
    if (mount_point[strlen(mount_point) - 1] == '/') {
        snprintf(full_path, sizeof(full_path), "%s%s", mount_point, decoded);
    } else {
        snprintf(full_path, sizeof(full_path), "%s/%s", mount_point, decoded);
    }

    // 6. 验证最终路径仍在允许范围内
    if (!path_within_mount(full_path, mount_point)) {
        return ESP_ERR_INVALID_ARG;
    }

    // 7. 执行操作
    // ...
}
```

### 4.2 禁止的操作

```c
// ❌ 禁止：危险的文件操作
system(cmd);           // 禁止：命令注入
popen(cmd, "r");       // 禁止：命令注入
exec*();               // 禁止：命令注入
strcpy/dest, src);     // 禁止：缓冲区溢出
gets();                // 禁止：无边界读取
sprintf();             // 禁止：缓冲区溢出
```

---

## 五、安全检查清单

每个涉及用户输入的函数必须检查：

- [ ] **长度检查**：输入是否超过最大允许长度？
- [ ] **危险字符**：是否包含 `..`, `/`, `\`, 特殊控制字符？
- [ ] **类型检查**：数值参数是否在有效范围内？
- [ ] **NULL 检查**：指针是否为 NULL？
- [ ] **缓冲区边界**：写入是否会溢出？
- [ ] **资源释放**：所有分配的内存/句柄是否正确释放？
- [ ] **超时设置**：操作是否有超时保护？

---

## 六、常见漏洞模式

| 漏洞类型 | 危险模式 | 安全模式 |
|---------|---------|---------|
| 路径遍历 | `sprintf(p, "%s/%s", base, input)` | `path_join(base, input)` + 安全检查 |
| 缓冲区溢出 | `strcpy(buf, input)` | `strncpy(buf, input, size-1)` |
| 命令注入 | `system(cmd)` | 禁止，使用安全的API |
| 整数溢出 | `malloc(size * count)` | `size_mul_check(size, count)` |
| 竞态条件 | 懒初始化无锁保护 | 静态初始化或临界区保护 |
