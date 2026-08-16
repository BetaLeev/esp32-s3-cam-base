/**
 * @file filesystem.c
 * @brief LittleFS 文件系统统一管理实现
 */
#include "filesystem.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "WEB_FS";

/* 文件系统配置 - 与 partitions.csv 一致 */
#define WEBFS_PARTITION_LABEL "webfs"
#define WEBFS_BASE_PATH "/web"

esp_err_t web_filesystem_init(void) {
    ESP_LOGI(TAG, "初始化 LittleFS 文件系统...");
    ESP_LOGI(TAG, "分区标签: %s, 挂载点: %s", WEBFS_PARTITION_LABEL, WEBFS_BASE_PATH);

    esp_vfs_littlefs_conf_t conf = {.base_path = WEBFS_BASE_PATH,
                                    .partition_label = WEBFS_PARTITION_LABEL,
                                    .format_if_mount_failed = false,
                                    .dont_mount = false};

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LittleFS 挂载失败: %s，请检查镜像是否正确烧录到 webfs 分区",
                 esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(WEBFS_PARTITION_LABEL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "获取 LittleFS 信息失败: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "LittleFS 挂载成功");
    ESP_LOGI(TAG, "总空间: %zu bytes, 已使用: %zu bytes", total, used);

    DIR *dir = opendir(WEBFS_BASE_PATH);
    if (dir) {
        struct dirent *entry;
        ESP_LOGI(TAG, "====== 开始列出 /web 目录下的文件 ======");
        while ((entry = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, "发现文件: %s", entry->d_name);
        }
        closedir(dir);
        ESP_LOGI(TAG, "========================================");
    } else {
        ESP_LOGE(TAG, "无法打开挂载点目录: %s", WEBFS_BASE_PATH);
    }

    return ESP_OK;
}

/**
 * @brief 获取文件系统信息
 */
esp_err_t web_filesystem_get_info(size_t *total, size_t *used) {
    if (total == NULL || used == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_littlefs_info(WEBFS_PARTITION_LABEL, total, used);
}

/**
 * @brief 根据扩展名获取 MIME 类型
 */
static const char *get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (ext == NULL)
        return "text/html";

    if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0)
        return "text/html";
    if (strcasecmp(ext, ".css") == 0)
        return "text/css";
    if (strcasecmp(ext, ".js") == 0 || strcasecmp(ext, ".mjs") == 0)
        return "application/javascript";
    if (strcasecmp(ext, ".json") == 0)
        return "application/json";
    if (strcasecmp(ext, ".png") == 0)
        return "image/png";
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcasecmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcasecmp(ext, ".svg") == 0)
        return "image/svg+xml";
    if (strcasecmp(ext, ".ico") == 0)
        return "image/x-icon";
    if (strcasecmp(ext, ".woff") == 0)
        return "font/woff";
    if (strcasecmp(ext, ".woff2") == 0)
        return "font/woff2";
    if (strcasecmp(ext, ".ttf") == 0)
        return "font/ttf";
    if (strcasecmp(ext, ".eot") == 0)
        return "application/vnd.ms-fontobject";
    if (strcasecmp(ext, ".xml") == 0)
        return "application/xml";
    if (strcasecmp(ext, ".txt") == 0)
        return "text/plain";

    return "application/octet-stream";
}

/**
 * @brief 读取文件内容
 */
esp_err_t web_filesystem_read_file(const char *path, char **out_buffer, size_t *out_len) {
    char full_path[256];
    
    // 自动剥离前导斜杠防止 // 路径问题
    while (path && *path == '/') path++;
    
    snprintf(full_path, sizeof(full_path), "%s/%s", WEBFS_BASE_PATH, path);

    FILE *f = fopen(full_path, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "无法打开文件: %s", full_path);
        return ESP_FAIL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        return ESP_FAIL;
    }

    char *buffer = malloc(file_size + 1);
    if (buffer == NULL) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t read_len = fread(buffer, 1, file_size, f);
    buffer[read_len] = '\0';
    fclose(f);

    *out_buffer = buffer;
    *out_len = read_len;

    return ESP_OK;
}

/**
 * @brief 发送静态文件到 HTTP 客户端（修正了双斜杠与 SPA Fallback 逻辑）
 */
esp_err_t web_filesystem_serve_file(httpd_req_t *req, const char *filepath) {
    char full_path[256];

    // 1. 规范化路径：剥离前导 '/' 字符，避免拼出 "/web//assets/..."
    if (filepath != NULL) {
        while (*filepath == '/') {
            filepath++;
        }
    }

    // 2. 空路径或根请求统一定向到 index.html
    if (filepath == NULL || strlen(filepath) == 0) {
        filepath = "index.html";
    }

    snprintf(full_path, sizeof(full_path), "%s/%s", WEBFS_BASE_PATH, filepath);

    FILE *f = fopen(full_path, "rb");
    if (f == NULL) {
        // 判断请求的资源文件是否有后缀名
        const char *ext = strrchr(filepath, '.');

        // 如果包含文件扩展名且不是 html，说明是真实的静态文件资源缺失，决不能 fallback 回 index.html
        if (ext != NULL && strcasecmp(ext, ".html") != 0 && strcasecmp(ext, ".htm") != 0) {
            ESP_LOGW(TAG, "静态资源缺失 404: %s", full_path);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }

        // 无扩展名（如 /setting, /about）才视为 Vue 前端路由，返回 index.html
        ESP_LOGI(TAG, "SPA 路由 Fallback: %s -> index.html", filepath);
        snprintf(full_path, sizeof(full_path), "%s/index.html", WEBFS_BASE_PATH);
        f = fopen(full_path, "rb");
        if (f == NULL) {
            ESP_LOGE(TAG, "FATAL: index.html 不存在!");
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
    }

    // 3. 动态配置正确的 MIME 类型与跨域 Header
    httpd_resp_set_type(req, get_mime_type(full_path));
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // HTML 禁用强缓存，JS/CSS 等静态资源配置长期缓存以提升加载速度
    if (strstr(full_path, ".html")) {
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    } else {
        httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000");
    }

    // 4. 分块传输响应内容
    char chunk[2048];
    size_t bytes_read;
    while ((bytes_read = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, bytes_read) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // 发送结束标记
    return ESP_OK;
}