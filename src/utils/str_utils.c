/**
 * @file str_utils.c
 * @brief 字符串处理工具函数实现
 */

#include "str_utils.h"
#include <stdio.h>

void str_format_size(uint32_t size, char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0) {
        return;
    }

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

void str_format_size64(uint64_t size, char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0) {
        return;
    }

    if (size < 1024) {
        snprintf(buf, buf_size, "%llu B", (unsigned long long)size);
    } else if (size < 1024ULL * 1024) {
        snprintf(buf, buf_size, "%.1f KB", size / 1024.0);
    } else if (size < 1024ULL * 1024 * 1024) {
        snprintf(buf, buf_size, "%.1f MB", size / (1024.0 * 1024.0));
    } else {
        snprintf(buf, buf_size, "%.2f GB", size / (1024.0 * 1024.0 * 1024.0));
    }
}
