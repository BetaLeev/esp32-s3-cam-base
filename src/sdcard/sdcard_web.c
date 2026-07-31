/**
 * @file sdcard_web.c
 * @brief TF卡Web文件管理HTTP处理模块
 */

#include "sdcard_web.h"
#include "sdcard.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "ff.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "SDCARD_WEB";

/**
 * @brief URL 解码
 */
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

/**
 * @brief 从 URI 中提取 path 参数（包含 URL 解码）
 */
static bool extract_path_from_uri(const char *uri, char *path, size_t path_size)
{
    const char *query = strstr(uri, "?path=");
    if (!query) return false;

    query += 6;
    size_t i = 0;

    while (query[i] && query[i] != '&' && i < path_size - 1) {
        path[i] = query[i];
        i++;
    }
    path[i] = '\0';

    // URL 解码（处理 %XX 编码字符，如 %7E -> ~）
    char decoded[path_size];
    url_decode(path, decoded, sizeof(decoded));
    strncpy(path, decoded, path_size - 1);
    path[path_size - 1] = '\0';

    return i > 0;
}

#define MAX_FILES 100
#define JSON_BUFFER_SIZE 8192  // 减小缓冲区大小，避免栈溢出

/**
 * @brief URL 编码单个字符
 */
static void url_encode_char(char c, char *dst)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
        (c >= '0' && c <= '9') || c == '-' || c == '_' || 
        c == '.' || c == '~') {
        dst[0] = c;
        dst[1] = '\0';
    } else if (c == '/') {
        dst[0] = '/';
        dst[1] = '\0';
    } else {
        snprintf(dst, 4, "%%%02X", (unsigned char)c);
    }
}

/**
 * @brief URL 编码字符串
 */
static void url_encode(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 4; i++) {
        url_encode_char(src[i], dst + j);
        j += strlen(dst + j);
    }
    dst[j] = '\0';
}

/**
 * @brief JSON字符串转义
 */
static void json_escape(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 1; i++) {
        char c = src[i];
        if (c == '"' || c == '\\') {
            if (j + 2 < dst_size) {
                dst[j++] = '\\';
                dst[j++] = c;
            }
        } else if (c == '\n') {
            if (j + 2 < dst_size) {
                dst[j++] = '\\';
                dst[j++] = 'n';
            }
        } else if (c == '\r') {
            if (j + 2 < dst_size) {
                dst[j++] = '\\';
                dst[j++] = 'r';
            }
        } else {
            dst[j++] = c;
        }
    }
    dst[j] = '\0';
}

/**
 * @brief 根据文件扩展名获取MIME类型
 */
static const char* get_mime_type(const char *filename)
{
    const char *ext = strrchr(filename, '.');
    if (ext) {
        ext++;
        // 图片
        if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0)
            return "image/jpeg";
        if (strcasecmp(ext, "png") == 0)
            return "image/png";
        if (strcasecmp(ext, "gif") == 0)
            return "image/gif";
        if (strcasecmp(ext, "bmp") == 0)
            return "image/bmp";
        if (strcasecmp(ext, "webp") == 0)
            return "image/webp";
        if (strcasecmp(ext, "ico") == 0)
            return "image/x-icon";
        // 音频
        if (strcasecmp(ext, "mp3") == 0)
            return "audio/mpeg";
        if (strcasecmp(ext, "wav") == 0)
            return "audio/wav";
        if (strcasecmp(ext, "ogg") == 0)
            return "audio/ogg";
        if (strcasecmp(ext, "flac") == 0)
            return "audio/flac";
        if (strcasecmp(ext, "aac") == 0)
            return "audio/aac";
        if (strcasecmp(ext, "m4a") == 0)
            return "audio/mp4";
        // 视频
        if (strcasecmp(ext, "mp4") == 0)
            return "video/mp4";
        if (strcasecmp(ext, "webm") == 0)
            return "video/webm";
        if (strcasecmp(ext, "mkv") == 0)
            return "video/x-matroska";
        if (strcasecmp(ext, "avi") == 0)
            return "video/x-msvideo";
        if (strcasecmp(ext, "mov") == 0)
            return "video/quicktime";
        if (strcasecmp(ext, "flv") == 0)
            return "video/x-flv";
        if (strcasecmp(ext, "wmv") == 0)
            return "video/x-ms-wmv";
        if (strcasecmp(ext, "3gp") == 0)
            return "video/3gpp";
    }
    return "application/octet-stream";
}

/**
 * @brief 格式化文件大小
 */
static void format_size_str(uint32_t size, char *buf, size_t buf_size)
{
    if (size < 1024) {
        snprintf(buf, buf_size, "%lu B", (unsigned long)size);
    } else if (size < 1024 * 1024) {
        snprintf(buf, buf_size, "%.1f KB", size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        snprintf(buf, buf_size, "%.1f MB", size / (1024.0 * 1024.0));
    } else {
        snprintf(buf, buf_size, "%.2f GB", size / (1024.0 * 1024.0 * 1024.0));
    }
}

/**
 * @brief 获取文件类型图标
 */
static const char* get_file_icon(const char *filename, uint8_t is_dir)
{
    if (is_dir) return "folder";

    const char *ext = strrchr(filename, '.');
    if (ext) {
        ext++;
        if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
            strcasecmp(ext, "png") == 0 || strcasecmp(ext, "gif") == 0 ||
            strcasecmp(ext, "bmp") == 0 || strcasecmp(ext, "webp") == 0) {
            return "image";
        }
        if (strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "wav") == 0 ||
            strcasecmp(ext, "flac") == 0 || strcasecmp(ext, "ogg") == 0 ||
            strcasecmp(ext, "aac") == 0 || strcasecmp(ext, "m4a") == 0) {
            return "audio";
        }
        if (strcasecmp(ext, "mp4") == 0 || strcasecmp(ext, "avi") == 0 ||
            strcasecmp(ext, "mkv") == 0 || strcasecmp(ext, "mov") == 0 ||
            strcasecmp(ext, "webm") == 0 || strcasecmp(ext, "mvi") == 0) {
            return "video";
        }
        if (strcasecmp(ext, "pdf") == 0) return "pdf";
        if (strcasecmp(ext, "txt") == 0 || strcasecmp(ext, "log") == 0 ||
            strcasecmp(ext, "md") == 0) return "text";
    }
    return "file";
}

/**
 * @brief API: 获取 TF 卡文件列表
 * @note 使用 malloc 从堆中分配缓冲区，避免栈溢出
 */
esp_err_t sdcard_web_files_handler(httpd_req_t *req)
{
    // 使用 malloc 从堆中分配缓冲区，避免栈溢出
    char *buffer = malloc(JSON_BUFFER_SIZE);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "分配JSON缓冲区失败");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "内存分配失败");
        return ESP_FAIL;
    }

    char escaped_name[256];
    char dir_path[256] = {0};
    char query_buf[512];

    if (httpd_req_get_url_query_str(req, query_buf, sizeof(query_buf)) == ESP_OK) {
        httpd_query_key_value(query_buf, "path", dir_path, sizeof(dir_path));
    }

    ESP_LOGI(TAG, "API /api/sdcard/files - path: '%s', mounted: %d", dir_path, sdcard_is_mounted());

    char full_path[512];
    if (dir_path[0] != '\0') {
        snprintf(full_path, sizeof(full_path), "0:/%s", dir_path);
    } else {
        snprintf(full_path, sizeof(full_path), "0:/");
    }

    FF_DIR dir;
    FRESULT res = f_opendir(&dir, full_path);
    ESP_LOGI(TAG, "f_opendir('%s') = %d", full_path, res);

    if (res != FR_OK) {
        snprintf(buffer, JSON_BUFFER_SIZE,
            "{\"mounted\":false,\"error\":\"打开目录失败 (res=%d)\"}", res);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, buffer, strlen(buffer));
        free(buffer);  // 安全释放
        return ESP_OK;
    }

    int offset = 0;
    offset += snprintf(buffer + offset, JSON_BUFFER_SIZE - offset,
        "{\"mounted\":true,\"path\":\"%s\",\"files\":[", dir_path);

    uint32_t file_count = 0;
    FILINFO fno;

    while (f_readdir(&dir, &fno) == FR_OK && file_count < MAX_FILES) {
        if (fno.fname[0] == 0) break;
        if (fno.fname[0] == '.') continue;

        // JSON转义文件名
        json_escape(fno.fname, escaped_name, sizeof(escaped_name));

        // 调试：打印文件名
        ESP_LOGD(TAG, "文件: '%s' -> '%s', 大小: %lu", fno.fname, escaped_name, (unsigned long)fno.fsize);

        if (file_count > 0 && offset < (int)JSON_BUFFER_SIZE - 1) {
            offset += snprintf(buffer + offset, JSON_BUFFER_SIZE - offset, ",");
        }

        char size_str[32];
        format_size_str((uint32_t)fno.fsize, size_str, sizeof(size_str));

        const char *icon = get_file_icon(fno.fname, (fno.fattrib & AM_DIR) ? 1 : 0);

        // 构建文件的完整访问路径
        char file_url[512];
        if (dir_path[0] != '\0') {
            snprintf(file_url, sizeof(file_url), "%s/%s", dir_path, fno.fname);
        } else {
            snprintf(file_url, sizeof(file_url), "%s", fno.fname);
        }

        // URL编码路径
        char encoded_url[600];
        url_encode(file_url, encoded_url, sizeof(encoded_url));

        // JSON转义URL
        char escaped_url[600];
        json_escape(encoded_url, escaped_url, sizeof(escaped_url));

        int written = snprintf(buffer + offset, JSON_BUFFER_SIZE - offset,
            "{\"name\":\"%s\",\"size\":%lu,\"size_str\":\"%s\",\"is_dir\":%d,\"modified\":0,\"icon\":\"%s\",\"url\":\"%s\"}",
            escaped_name, (unsigned long)fno.fsize, size_str,
            (fno.fattrib & AM_DIR) ? 1 : 0, icon, escaped_url);

        if (written > 0) offset += written;

        if (offset >= (int)JSON_BUFFER_SIZE - 400) break;
        file_count++;
    }

    f_closedir(&dir);

    offset += snprintf(buffer + offset, JSON_BUFFER_SIZE - offset,
        "],\"count\":%lu}", (unsigned long)file_count);

    ESP_LOGI(TAG, "返回文件数: %lu, JSON大小: %d", (unsigned long)file_count, offset);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));
    free(buffer);  // 安全释放
    return ESP_OK;
}

/**
 * @brief API: 打印文件列表到串口（调试用）
 */
esp_err_t sdcard_web_debug_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "========== TF卡文件列表 ==========");
    
    char full_path[512] = "0:/";
    
    FF_DIR dir;
    FRESULT res = f_opendir(&dir, full_path);
    if (res != FR_OK) {
        ESP_LOGW(TAG, "无法打开目录: %s (res=%d)", full_path, res);
        return ESP_FAIL;
    }
    
    FILINFO fno;
    int count = 0;
    
    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
        if (fno.fname[0] == '.') continue;
        
        if (fno.fattrib & AM_DIR) {
            ESP_LOGI(TAG, "[DIR]  %s/", fno.fname);
        } else {
            ESP_LOGI(TAG, "[FILE] %s (%.1f KB)", fno.fname, fno.fsize / 1024.0);
        }
        count++;
    }
    
    f_closedir(&dir);
    ESP_LOGI(TAG, "========== 共 %d 个项目 ==========", count);
    
    const char *response = "文件列表已打印到串口日志";
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

/**
 * @brief API: 获取 TF 卡信息
 */
esp_err_t sdcard_web_info_handler(httpd_req_t *req)
{
    char buffer[512];
    uint64_t total = 0, free = 0, used = 0;

    ESP_LOGI(TAG, "API /api/sdcard/info - mounted: %d", sdcard_is_mounted());

    sdcard_get_info(&total, &free);
    if (total > free) used = total - free;

    snprintf(buffer, sizeof(buffer),
        "{"
        "\"mounted\":%s,"
        "\"total\":%llu,"
        "\"free\":%llu,"
        "\"used\":%llu,"
        "\"total_gb\":%.2f,"
        "\"free_gb\":%.2f,"
        "\"used_gb\":%.2f,"
        "\"image_ext\":\"%s\","
        "\"video_ext\":\"%s\""
        "}",
        sdcard_is_mounted() ? "true" : "false",
        (unsigned long long)total,
        (unsigned long long)free,
        (unsigned long long)used,
        total / (1024.0 * 1024.0 * 1024.0),
        free / (1024.0 * 1024.0 * 1024.0),
        used / (1024.0 * 1024.0 * 1024.0),
        sdcard_get_image_extensions(),
        sdcard_get_video_extensions()
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buffer, strlen(buffer));
    return ESP_OK;
}

/**
 * @brief 解析 Range 请求头，提取起始和结束字节
 * @param range_str Range 头字符串，如 "bytes=0-1023"
 * @param file_size 文件总大小
 * @param out_start 输出：起始字节
 * @param out_end 输出：结束字节
 * @return true 表示解析成功
 */
static bool parse_range_header(const char *range_str, uint32_t file_size,
                                uint32_t *out_start, uint32_t *out_end)
{
    // 检查是否是 "bytes=start-end" 格式
    const char *prefix = "bytes=";
    size_t prefix_len = strlen(prefix);

    if (strncmp(range_str, prefix, prefix_len) != 0) {
        return false;
    }

    const char *range_val = range_str + prefix_len;
    char *endptr;

    // 解析起始字节
    uint32_t start = strtol(range_val, &endptr, 10);
    if (endptr == range_val || *endptr != '-') {
        return false;
    }

    // 解析结束字节（可选，如果省略则到文件末尾）
    uint32_t end;
    if (*(endptr + 1) == '\0') {
        // 没有指定结束字节，使用文件末尾
        end = file_size - 1;
    } else {
        end = strtol(endptr + 1, &endptr, 10);
        if (endptr == (endptr + 1) || *endptr != '\0') {
            return false;
        }
    }

    // 边界检查
    if (start > end) {
        return false;
    }
    if (start >= file_size) {
        start = file_size - 1;
    }
    if (end >= file_size) {
        end = file_size - 1;
    }

    *out_start = start;
    *out_end = end;
    return true;
}

/**
 * @brief 下载 TF 卡文件（支持 Range 请求，用于音视频流式播放）
 * @note 支持 HTTP Range 断点续传，兼容浏览器音视频流播放和拖动进度条
 */
esp_err_t sdcard_web_download_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, ">>> 下载开始");

    // 从 URI 中提取 path 参数
    char file_path[200] = {0};

    if (!extract_path_from_uri(req->uri, file_path, sizeof(file_path))) {
        ESP_LOGW(TAG, "提取path失败");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "No path");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "path: %s", file_path);

    // 构建 FatFS 路径
    char full_path[250];
    snprintf(full_path, sizeof(full_path), "0:/%s", file_path);
    ESP_LOGI(TAG, "full: %s", full_path);

    // 打开文件
    FIL file;
    FRESULT res = f_open(&file, full_path, FA_READ);
    if (res != FR_OK) {
        ESP_LOGW(TAG, "打开失败: %d", res);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
        return ESP_FAIL;
    }

    uint32_t file_size = f_size(&file);
    ESP_LOGI(TAG, "大小: %lu bytes", (unsigned long)file_size);

    // 获取文件名用于 MIME 类型
    const char *fname = strrchr(file_path, '/');
    fname = fname ? fname + 1 : file_path;

    const char *mime = get_mime_type(fname);
    httpd_resp_set_type(req, mime);
    ESP_LOGI(TAG, "MIME: %s", mime);

    // 初始化 Range 相关变量
    char range_buf[64] = {0};
    uint32_t range_start = 0;
    uint32_t range_end = file_size - 1;
    bool is_range_request = false;

    // 尝试获取 Range 请求头
    if (httpd_req_get_hdr_value_str(req, "Range", range_buf, sizeof(range_buf)) == ESP_OK) {
        ESP_LOGI(TAG, "Range 请求: %s", range_buf);
        if (parse_range_header(range_buf, file_size, &range_start, &range_end)) {
            is_range_request = true;
            ESP_LOGI(TAG, "Range 解析: start=%lu, end=%lu",
                     (unsigned long)range_start, (unsigned long)range_end);
        }
    }

    // 计算要发送的内容长度
    uint32_t content_length = range_end - range_start + 1;

    if (is_range_request) {
        // 处理 Range 请求：返回 206 Partial Content
        char crange[80];
        snprintf(crange, sizeof(crange), "bytes %lu-%lu/%lu",
                 (unsigned long)range_start,
                 (unsigned long)range_end,
                 (unsigned long)file_size);

        httpd_resp_set_status(req, "206 Partial Content");
        httpd_resp_set_hdr(req, "Content-Range", crange);

        // 设置 Content-Length
        char clen[20];
        snprintf(clen, sizeof(clen), "%lu", (unsigned long)content_length);
        httpd_resp_set_hdr(req, "Content-Length", clen);

        // 使用 FatFS f_lseek 跳转到指定位置
        res = f_lseek(&file, range_start);
        if (res != FR_OK) {
            ESP_LOGE(TAG, "f_lseek 失败: %d", res);
            f_close(&file);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Seek error");
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "跳转到字节: %lu", (unsigned long)range_start);
    } else {
        // 正常请求：返回 200 OK，发送整个文件
        httpd_resp_set_status(req, "200 OK");

        // 设置 Content-Length
        char clen[20];
        snprintf(clen, sizeof(clen), "%lu", (unsigned long)file_size);
        httpd_resp_set_hdr(req, "Content-Length", clen);
    }

    // 设置 inline 模式，支持浏览器直接播放
    httpd_resp_set_hdr(req, "Content-Disposition", "inline");
    httpd_resp_set_hdr(req, "Accept-Ranges", "bytes");
    // 添加缓存控制，允许浏览器缓存
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");

    ESP_LOGI(TAG, "发送中... Content-Length: %lu bytes", (unsigned long)content_length);

    // 使用较小的缓冲区（2KB），减少内存压力
    #define CHUNK_SIZE 2048
    char *buf = malloc(CHUNK_SIZE);
    if (buf == NULL) {
        ESP_LOGE(TAG, "分配缓冲区失败");
        f_close(&file);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "内存分配失败");
        return ESP_FAIL;
    }

    UINT bytes_read;
    uint32_t sent = 0;
    esp_err_t send_err = ESP_OK;

    while (sent < content_length) {
        // 计算本次要读取的字节数
        UINT to_read = (content_length - sent < CHUNK_SIZE) ? (content_length - sent) : CHUNK_SIZE;

        res = f_read(&file, buf, to_read, &bytes_read);
        if (res != FR_OK) {
            ESP_LOGE(TAG, "f_read 失败: res=%d", res);
            send_err = ESP_FAIL;
            break;
        }
        if (bytes_read == 0) {
            ESP_LOGW(TAG, "读取到文件末尾");
            break;
        }

        // 发送到客户端
        send_err = httpd_resp_send_chunk(req, buf, bytes_read);
        if (send_err != ESP_OK) {
            // 客户端断开连接，优雅退出
            ESP_LOGW(TAG, "客户端断开连接，终止发送 (sent=%lu/%lu)",
                     (unsigned long)sent, (unsigned long)content_length);
            break;
        }

        sent += bytes_read;

        // 每发送 64KB 输出一次日志，便于追踪进度
        if ((sent & 0xFFFF) == 0 || sent >= content_length) {
            ESP_LOGI(TAG, "进度: %lu/%lu bytes (%.1f%%)",
                     (unsigned long)sent, (unsigned long)content_length,
                     (sent * 100.0) / content_length);
        }
    }

    free(buf);

    // 发送结束 chunk
    httpd_resp_send_chunk(req, NULL, 0);

    // 关闭文件
    f_close(&file);

    ESP_LOGI(TAG, "<<< 完成: %lu/%lu bytes", (unsigned long)sent, (unsigned long)content_length);

    return send_err;
}

/**
 * @brief API: 上传文件到 TF 卡
 * @note 支持通过 POST 请求上传文件，文件数据在请求体中
 */
esp_err_t sdcard_web_upload_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, ">>> 上传开始");

    // 获取目标路径参数
    char target_dir[256] = {0};
    char query_buf[512];

    if (httpd_req_get_url_query_str(req, query_buf, sizeof(query_buf)) == ESP_OK) {
        httpd_query_key_value(query_buf, "path", target_dir, sizeof(target_dir));
    }

    // 获取文件名（从 query 参数或 Content-Disposition header）
    char filename[256] = {0};
    char content_disp[512] = {0};

    // 优先从 URL query 参数获取文件名
    if (httpd_req_get_url_query_str(req, query_buf, sizeof(query_buf)) == ESP_OK) {
        httpd_query_key_value(query_buf, "filename", filename, sizeof(filename));
        // URL 解码文件名
        if (filename[0] != '\0') {
            char decoded[256];
            url_decode(filename, decoded, sizeof(decoded));
            strncpy(filename, decoded, sizeof(filename) - 1);
            filename[sizeof(filename) - 1] = '\0';
        }
    }

    // 如果 query 中没有，从 Content-Disposition header 获取
    if (filename[0] == '\0') {
        if (httpd_req_get_hdr_value_str(req, "Content-Disposition",
                                         content_disp, sizeof(content_disp)) == ESP_OK) {
            // 解析 filename="xxx"
            char *fname_start = strstr(content_disp, "filename=\"");
            if (fname_start) {
                fname_start += 10;
                char *fname_end = strchr(fname_start, '"');
                if (fname_end) {
                    size_t len = fname_end - fname_start;
                    if (len < sizeof(filename)) {
                        strncpy(filename, fname_start, len);
                        filename[len] = '\0';
                    }
                }
            }
        }
    }

    if (filename[0] == '\0') {
        ESP_LOGW(TAG, "未找到文件名");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "未指定文件名");
        return ESP_FAIL;
    }

    // 构建完整路径
    char full_path[600];
    if (target_dir[0] != '\0') {
        snprintf(full_path, sizeof(full_path), "0:/%s/%s", target_dir, filename);
    } else {
        snprintf(full_path, sizeof(full_path), "0:/%s", filename);
    }

    // 检查可用空间
    uint64_t total = 0, free_space = 0;
    sdcard_get_info(&total, &free_space);

    size_t file_size = req->content_len;
    ESP_LOGI(TAG, "上传信息: 文件名='%s', 大小=%u bytes, 可用=%llu bytes",
             filename, file_size, (unsigned long long)free_space);

    if (file_size > free_space) {
        ESP_LOGW(TAG, "空间不足: 需要 %u bytes, 可用 %llu bytes", file_size, (unsigned long long)free_space);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "磁盘空间不足");
        return ESP_FAIL;
    }

    if (file_size == 0) {
        ESP_LOGW(TAG, "文件大小为0");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "文件为空");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "准备上传: %s", full_path);

    // 打开文件（创建/覆盖）
    FIL file;
    FRESULT res = f_open(&file, full_path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        ESP_LOGE(TAG, "创建文件失败: %d", res);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "无法创建文件");
        return ESP_FAIL;
    }

    // 使用堆分配缓冲区接收数据
    #define UPLOAD_CHUNK_SIZE 4096
    char *buf = malloc(UPLOAD_CHUNK_SIZE);
    if (buf == NULL) {
        ESP_LOGE(TAG, "分配缓冲区失败");
        f_close(&file);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "内存分配失败");
        return ESP_FAIL;
    }

    size_t received = 0;
    int remaining = req->content_len;

    // 循环接收数据
    while (remaining > 0) {
        size_t to_recv = (remaining < UPLOAD_CHUNK_SIZE) ? remaining : UPLOAD_CHUNK_SIZE;
        int ret = httpd_req_recv(req, buf, to_recv);

        if (ret < 0) {
            ESP_LOGE(TAG, "接收数据失败: %d", ret);
            free(buf);
            f_close(&file);
            // 删除不完整的文件
            f_unlink(full_path);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "接收数据失败");
            return ESP_FAIL;
        }

        if (ret == 0) {
            ESP_LOGW(TAG, "连接提前关闭");
            break;
        }

        // 写入文件
        UINT written = 0;
        res = f_write(&file, buf, ret, &written);
        if (res != FR_OK || written != (UINT)ret) {
            ESP_LOGE(TAG, "写入文件失败: res=%d, written=%u", res, written);
            free(buf);
            f_close(&file);
            f_unlink(full_path);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "写入文件失败");
            return ESP_FAIL;
        }

        received += ret;
        remaining -= ret;

        // 每 64KB 输出一次进度
        if ((received & 0xFFFF) == 0 || remaining == 0) {
            ESP_LOGI(TAG, "上传进度: %lu/%lu bytes (%.1f%%)",
                     (unsigned long)received, (unsigned long)file_size,
                     (received * 100.0) / file_size);
        }
    }

    free(buf);
    f_close(&file);

    ESP_LOGI(TAG, "<<< 上传完成: %s (%lu bytes)", full_path, (unsigned long)received);

    // 返回成功响应（JSON）
    char json_response[512];
    snprintf(json_response, sizeof(json_response),
             "{\"success\":true,\"name\":\"%s\",\"size\":%lu}",
             filename, (unsigned long)received);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, strlen(json_response));

    return ESP_OK;
}
