# ESP32 开发最佳实践

## 核心原则

### 优先使用官方示例

遇到问题时，**先查找 ESP-IDF 官方示例**，不要自己造轮子。

官方示例位置：
```
$IDF_PATH/examples/protocols/http_server/file_serving
$IDF_PATH/examples/storage/sd_card
```

---

## 文件下载最佳实践

### 官方实现（推荐）

参考 `examples/protocols/http_server/file_serving/main/file_server.c`

```c
// 文件下载 handler
static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    // 获取文件路径
    const char *filename = get_path_from_uri(filepath, base_path, req->uri, sizeof(filepath));

    // 检查文件是否存在
    if (stat(filepath, &file_stat) == -1) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    // 打开文件
    fd = fopen(filepath, "r");
    if (!fd) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read file");
        return ESP_FAIL;
    }

    // 设置 MIME 类型
    set_content_type_from_file(req, filename);

    // 分块发送（使用 scratch buffer）
    char scratch[SCRATCH_BUFSIZE];  // 8192 字节
    size_t chunksize;
    do {
        chunksize = fread(scratch, 1, SCRATCH_BUFSIZE, fd);
        if (chunksize > 0) {
            if (httpd_resp_send_chunk(req, scratch, chunksize) != ESP_OK) {
                fclose(fd);
                httpd_resp_send_chunk(req, NULL);  // 结束
                return ESP_FAIL;
            }
        }
    } while (chunksize != 0);

    fclose(fd);
    httpd_resp_send_chunk(req, NULL);  // 结束传输
    return ESP_OK;
}
```

### 关键要点

1. **使用 `httpd_resp_send_chunk`** - 分块发送，内部处理协议
2. **使用 scratch buffer** - 固定 8192 字节，避免频繁 malloc
3. **循环发送** - 直到 `chunksize == 0`
4. **结束传输** - 发送 `httpd_resp_send_chunk(req, NULL, 0)`
5. **错误处理** - 失败时也要发送 NULL chunk

### 常见错误做法

| 错误做法 | 问题 |
|---------|------|
| 设置 Content-Length + chunked | HTTP 协议冲突 |
| 一次性读取大文件到内存 | 内存不足重启 |
| 多次调用 httpd_resp_set_hdr | 头信息冲突 |
| 自己实现 Range 支持 | 复杂容易出错 |

---

## URL 编码/解码

### httpd_query_key_value 的行为

`httpd_query_key_value()` 会自动解码：
- `%20` → 空格
- `%E4%B8%AD` → 中文

**但不解码**：
- `%2F` → `/` （路径分隔符）

### 正确做法

```c
// 从 query 提取参数
char path[256];
if (httpd_query_key_value(query_buf, "path", path, sizeof(path)) == ESP_OK) {
    // httpd_query_key_value 已解码大部分
    // 但需要手动解码 %2F
    char decoded[256];
    url_decode(path, decoded, sizeof(decoded));
}
```

### URL 解码函数

```c
static void url_decode(const char *src, char *dst, size_t dst_size)
{
    size_t i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = {src[i+1], src[i+2], '\0'};
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}
```

---

## CORS 跨域支持

### 基础设置

```c
httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
```

### 完整设置（用于预检请求）

```c
httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Range");
```

---

## 排查清单

遇到问题时的检查顺序：

1. [ ] 浏览器直接访问后端接口确认后端正常
2. [ ] 观察后端串口日志确认请求到达
3. [ ] 检查 URL 编码/解码是否正确
4. [ ] 检查 HTTP 响应头设置
5. [ ] 参考官方示例确认实现方式

---

## 参考链接

- [ESP-IDF HTTP Server Examples](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server)
- [File Serving Example](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/file_serving)
- [SDMMC Example](https://github.com/espressif/esp-idf/tree/master/examples/storage/sd_card/sdmmc)
