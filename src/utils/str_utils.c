/**
 * @file str_utils.c
 * @brief 字符串处理工具函数实现
 */

#include "str_utils.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <iconv.h>

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

bool str_gbk_to_utf8(const char *gk_input, char *utf8_output, size_t output_size)
{
    if (gk_input == NULL || utf8_output == NULL || output_size == 0) {
        return false;
    }

    /* 检查是否需要转换（包含中文高字节 0x80-0xFF） */
    bool need_convert = false;
    for (const unsigned char *p = (const unsigned char *)gk_input; *p; p++) {
        if (*p >= 0x80) {
            need_convert = true;
            break;
        }
    }

    /* 如果不需要转换，直接复制 */
    if (!need_convert) {
        strncpy(utf8_output, gk_input, output_size - 1);
        utf8_output[output_size - 1] = '\0';
        return true;
    }

    /* 使用 iconv 转换 GBK -> UTF-8 */
    iconv_t cd = iconv_open("UTF-8//IGNORE", "GBK");
    if (cd == (iconv_t)-1) {
        /* iconv 不可用，复制原字符串 */
        strncpy(utf8_output, gk_input, output_size - 1);
        utf8_output[output_size - 1] = '\0';
        return false;
    }

    size_t in_len = strlen(gk_input);
    size_t out_len = output_size - 1;

    char *in_ptr = (char *)gk_input;
    char *out_ptr = utf8_output;

    size_t ret = iconv(cd, &in_ptr, &in_len, &out_ptr, &out_len);
    iconv_close(cd);

    if (ret == (size_t)-1) {
        /* 转换失败，复制原字符串 */
        strncpy(utf8_output, gk_input, output_size - 1);
        utf8_output[output_size - 1] = '\0';
        return false;
    }

    *out_ptr = '\0';
    return true;
}
