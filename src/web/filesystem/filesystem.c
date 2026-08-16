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
                                    .format_if_mount_failed =
                                        false, // 禁止挂载失败时格式化！防止冲掉烧录好的镜像
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
    if (strcasecmp(ext, ".js") == 0)
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
 * @brief 发送静态文件到 HTTP 客户端（包含 SPA Fallback 逻辑）
 */
esp_err_t web_filesystem_serve_file(httpd_req_t *req, const char *filepath) {
    char full_path[256];

    // 如果传入路径为空或为 "/"，默认映射到 index.html
    if (filepath == NULL || strlen(filepath) == 0 || strcmp(filepath, "/") == 0) {
        filepath = "index.html";
    }

    snprintf(full_path, sizeof(full_path), "%s/%s", WEBFS_BASE_PATH, filepath);

    FILE *f = fopen(full_path, "rb");
    if (f == NULL) {
        // 判断是否为带后缀的静态资源请求
        const char *ext = strrchr(filepath, '.');
        bool is_static_resource =
            (ext && (strcasecmp(ext, ".js") == 0 || strcasecmp(ext, ".css") == 0 ||
                     strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 ||
                     strcasecmp(ext, ".ico") == 0 || strcasecmp(ext, ".svg") == 0 ||
                     strcasecmp(ext, ".woff2") == 0 || strcasecmp(ext, ".woff") == 0 ||
                     strcasecmp(ext, ".ttf") == 0 || strcasecmp(ext, ".eot") == 0 ||
                     strcasecmp(ext, ".json") == 0));

        // 如果是特定的静态资源请求缺失，直接返回 404
        if (is_static_resource) {
            ESP_LOGW(TAG, "静态资源未找到: %s", filepath);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }

        // Vue 前端路由请求（如 /setting, /about），重定向 fallback 到 index.html
        ESP_LOGI(TAG, "SPA fallback: %s -> index.html", filepath);
        snprintf(full_path, sizeof(full_path), "%s/index.html", WEBFS_BASE_PATH);
        f = fopen(full_path, "rb");
        if (f == NULL) {
            ESP_LOGE(TAG, "FATAL: index.html 也不存在! 请确认 Flash 中已烧录 webfs 镜像。");
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
    }

    // 设置 Content-Type 及 CORS Header
    httpd_resp_set_type(req, get_mime_type(full_path));
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // JS/CSS 等资源可开启缓存，html 页面关闭缓存
    if (strstr(full_path, ".html")) {
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    } else {
        httpd_resp_set_hdr(req, "Cache-Control", "max-age=31536000");
    }

    // 块传输文件内容
    char chunk[1024];
    size_t bytes_read;
    while ((bytes_read = fread(chunk, 1, sizeof(chunk), f)) > 0) {
        if (httpd_resp_send_chunk(req, chunk, bytes_read) != ESP_OK) {
            fclose(f);
            return ESP_FAIL;
        }
    }

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0); // 结束分块传输
    return ESP_OK;
}