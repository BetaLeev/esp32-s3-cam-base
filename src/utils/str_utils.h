/**
 * @file str_utils.h
 * @brief 字符串处理工具函数
 */

#ifndef STR_UTILS_H
#define STR_UTILS_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief 格式化文件大小为可读字符串
 *
 * @param size 文件大小（字节）
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 */
void str_format_size(uint32_t size, char *buf, size_t buf_size);

/**
 * @brief 格式化文件大小为可读字符串（支持64位）
 *
 * @param size 文件大小（字节）
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 */
void str_format_size64(uint64_t size, char *buf, size_t buf_size);

#endif // STR_UTILS_H
