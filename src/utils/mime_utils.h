/**
 * @file mime_utils.h
 * @brief MIME类型工具函数
 */

#ifndef MIME_UTILS_H
#define MIME_UTILS_H

#include <stddef.h>

/**
 * @brief 根据文件名获取MIME类型
 *
 * @param filename 文件名
 * @return MIME类型字符串
 */
const char *mime_get_type(const char *filename);

/**
 * @brief 根据文件名获取文件类型图标
 *
 * @param filename 文件名
 * @param is_dir 是否为目录
 * @return 图标类型字符串
 */
const char *mime_get_icon(const char *filename, int is_dir);

#endif // MIME_UTILS_H
