/**
 * @file path_utils.c
 * @brief 路径处理工具函数实现
 */

#include "path_utils.h"
#include "esp_log.h"
#include <ctype.h>
#include <string.h>

static const char *TAG = "PATH_UTILS";

/* 危险路径模式列表 */
static const char *dangerous_patterns[] = {
    "..",
    "%2e%2e",  // URL编码的 ..
    "%252e",    // 双重URL编码
    "....",     // 多重点
};

/* ============================================================================
 * 路径安全检查
 * ============================================================================ */

bool path_is_safe(const char *path)
{
    if (path == NULL) {
        return false;
    }

    /* 检查长度 - 空路径是合法的（表示根目录） */
    size_t len = strlen(path);
    if (len >= PATH_MAX_LEN) {
        return false;
    }

    /* 空路径是合法的 */
    if (len == 0) {
        return true;
    }

    /* 创建小写副本用于检查 */
    char lower[PATH_MAX_LEN];
    for (size_t i = 0; i < len && i < sizeof(lower) - 1; i++) {
        lower[i] = (char)tolower((unsigned char)path[i]);
    }
    lower[len < sizeof(lower) ? len : sizeof(lower) - 1] = '\0';

    /* 检查危险模式 */
    size_t pattern_count = sizeof(dangerous_patterns) / sizeof(dangerous_patterns[0]);
    for (size_t i = 0; i < pattern_count; i++) {
        if (strstr(lower, dangerous_patterns[i]) != NULL) {
            ESP_LOGW(TAG, "检测到危险路径模式: %s", dangerous_patterns[i]);
            return false;
        }
    }

    /* 检查反斜杠（Windows风格路径） */
    if (strchr(lower, '\\') != NULL) {
        ESP_LOGW(TAG, "检测到反斜杠字符");
        return false;
    }

    /* 检查控制字符 */
    for (size_t i = 0; i < len; i++) {
        if (iscntrl((unsigned char)path[i])) {
            ESP_LOGW(TAG, "检测到控制字符");
            return false;
        }
    }

    return true;
}

/* ============================================================================
 * URL 解码
 * ============================================================================ */

bool path_url_decode(const char *src, char *dst, size_t dst_size)
{
    if (src == NULL || dst == NULL || dst_size == 0) {
        return false;
    }

    size_t i = 0, j = 0;

    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && src[i + 1] && src[i + 2]) {
            /* URL 编码的字符 */
            char hex[3] = {src[i + 1], src[i + 2], '\0'};
            char *endptr;
            unsigned char decoded = (unsigned char)strtol(hex, &endptr, 16);

            if (*endptr == '\0') {
                dst[j++] = (char)decoded;
                i += 3;
                continue;
            }
        } else if (src[i] == '+') {
            /* + 号转为空格（application/x-www-form-urlencoded） */
            dst[j++] = ' ';
            i++;
            continue;
        }

        dst[j++] = src[i++];
    }

    dst[j] = '\0';
    return true;
}

/* ============================================================================
 * 路径构建
 * ============================================================================ */

static size_t path_get_mount_len(const char *mount_point)
{
    if (mount_point == NULL) {
        return 0;
    }
    size_t len = strlen(mount_point);
    return (len > 0 && mount_point[len - 1] == '/') ? len - 1 : len;
}

bool path_build_fatfs(const char *mount_point, const char *relative_path,
                       char *out, size_t out_size)
{
    if (mount_point == NULL || out == NULL || out_size == 0) {
        return false;
    }

    /* 验证挂载点格式（应为 0:/xxx 或类似格式） */
    if (strncmp(mount_point, "0:", 2) != 0) {
        ESP_LOGE(TAG, "FatFS 挂载点格式错误: %s", mount_point);
        return false;
    }

    size_t pos = 0;

    /* 添加挂载点 */
    size_t mount_len = strlen(mount_point);
    if (pos + mount_len >= out_size) {
        return false;
    }
    strcpy(out, mount_point);
    pos += mount_len;

    /* 添加相对路径 */
    if (relative_path != NULL && relative_path[0] != '\0') {
        /* 确保中间有分隔符 */
        if (out[pos - 1] != '/') {
            if (pos >= out_size) return false;
            out[pos++] = '/';
        }

        size_t rel_len = strlen(relative_path);
        if (pos + rel_len >= out_size) {
            return false;
        }
        strcpy(out + pos, relative_path);
    }

    return true;
}

bool path_build_vfs(const char *mount_point, const char *relative_path,
                     char *out, size_t out_size)
{
    if (mount_point == NULL || out == NULL || out_size == 0) {
        return false;
    }

    /* 验证挂载点格式（应为 /xxx 格式） */
    if (mount_point[0] != '/') {
        ESP_LOGE(TAG, "VFS 挂载点格式错误: %s", mount_point);
        return false;
    }

    size_t pos = 0;

    /* 添加挂载点（去掉末尾斜杠） */
    size_t mount_len = path_get_mount_len(mount_point);
    if (pos + mount_len >= out_size) {
        return false;
    }
    strncpy(out, mount_point, mount_len);
    out[mount_len] = '\0';
    pos = mount_len;

    /* 添加相对路径 */
    if (relative_path != NULL && relative_path[0] != '\0') {
        /* 跳过relative_path开头的斜杠，避免产生双斜杠 */
        const char *rel_start = relative_path;
        while (*rel_start == '/') rel_start++;
        
        if (*rel_start != '\0') {
            /* 确保中间有分隔符 */
            if (out[pos - 1] != '/') {
                if (pos >= out_size) return false;
                out[pos++] = '/';
            }

            size_t rel_len = strlen(rel_start);
            if (pos + rel_len >= out_size) {
                return false;
            }
            strcpy(out + pos, rel_start);
        }
    }

    return true;
}

/* ============================================================================
 * 扩展名和挂载点检查
 * ============================================================================ */

const char *path_get_extension(const char *filename)
{
    if (filename == NULL) {
        return "";
    }

    const char *dot = strrchr(filename, '.');
    if (dot != NULL && dot != filename) {
        return dot + 1;
    }
    return "";
}

bool path_within_mount(const char *full_path, const char *mount_point)
{
    if (full_path == NULL || mount_point == NULL) {
        return false;
    }

    size_t mount_len = strlen(mount_point);

    /* 确保 full_path 以 mount_point 开头 */
    if (strncmp(full_path, mount_point, mount_len) != 0) {
        return false;
    }

    /* 检查下一个字符（应该是 / 或 \0） */
    char next = full_path[mount_len];
    if (next != '\0' && next != '/') {
        return false;
    }

    return true;
}
