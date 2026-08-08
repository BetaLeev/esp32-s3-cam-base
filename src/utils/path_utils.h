/**
 * @file path_utils.h
 * @brief 路径处理工具函数
 *
 * 提供统一的路径操作接口，避免复制粘贴导致的路径格式不一致问题
 */

#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <stddef.h>
#include <stdbool.h>

/**
 * @brief 路径最大长度
 */
#define PATH_MAX_LEN 512

/**
 * @brief 路径组件最大长度
 */
#define PATH_COMPONENT_MAX_LEN 64

/**
 * @brief 路径遍历攻击检测
 *
 * 检查路径是否包含危险字符或模式
 *
 * @param path 待检查的路径
 * @return true 安全, false 包含危险模式
 */
bool path_is_safe(const char *path);

/**
 * @brief URL 解码（原地版本）
 *
 * @param src 源字符串
 * @param dst 目标缓冲区
 * @param dst_size 目标缓冲区大小
 * @return true 成功, false 失败
 */
bool path_url_decode(const char *src, char *dst, size_t dst_size);

/**
 * @brief 构建 FatFS 格式路径
 *
 * @param mount_point 挂载点（如 "0:/sdcard"）
 * @param relative_path 相对路径（如 "photos/image.jpg"）
 * @param out 输出缓冲区
 * @param out_size 输出缓冲区大小
 * @return true 成功, false 失败
 */
bool path_build_fatfs(const char *mount_point, const char *relative_path,
                       char *out, size_t out_size);

/**
 * @brief 构建 VFS 格式路径
 *
 * @param mount_point 挂载点（如 "/sdcard"）
 * @param relative_path 相对路径（如 "photos/image.jpg"）
 * @param out 输出缓冲区
 * @param out_size 输出缓冲区大小
 * @return true 成功, false 失败
 */
bool path_build_vfs(const char *mount_point, const char *relative_path,
                     char *out, size_t out_size);

/**
 * @brief 获取文件扩展名
 *
 * @param filename 文件名
 * @return 扩展名字符串（不含点），空字符串表示无扩展名
 */
const char *path_get_extension(const char *filename);

/**
 * @brief 检查路径是否在允许的挂载点内
 *
 * @param full_path 完整路径
 * @param mount_point 挂载点
 * @return true 在范围内, false 超出范围
 */
bool path_within_mount(const char *full_path, const char *mount_point);

#endif // PATH_UTILS_H
