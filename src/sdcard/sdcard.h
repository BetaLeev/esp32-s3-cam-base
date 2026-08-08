/**
 * @file sdcard.h
 * @brief TF卡(SD卡)模块接口定义
 */

#ifndef SDCARD_H
#define SDCARD_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/sdmmc_types.h"
#include "ff.h"

/**
 * @brief 初始化TF卡
 * @return ESP_OK 成功，其他失败
 */
esp_err_t sdcard_init(void);

/**
 * @brief 获取TF卡信息
 * @param out_total 输出总容量(字节)
 * @param out_free 输出可用空间(字节)
 * @return ESP_OK 成功，其他失败
 */
esp_err_t sdcard_get_info(uint64_t *out_total, uint64_t *out_free);

/**
 * @brief 获取TF卡已用字节数
 * @return 已用空间(字节)
 */
uint64_t sdcard_get_used_bytes(void);

/**
 * @brief 检查TF卡是否已挂载
 * @return true 已挂载，false 未挂载
 */
bool sdcard_is_mounted(void);

/**
 * @brief 获取TF卡挂载路径
 * @return 挂载路径字符串
 */
const char* sdcard_get_mount_point(void);

/**
 * @brief 获取TF卡CSD信息
 * @return CSD结构体指针，失败返回NULL
 */
const sdmmc_csd_t* sdcard_get_csd(void);

/* ========== 文件浏览功能 ========== */

/**
 * @brief 文件/目录信息结构体
 */
typedef struct {
    char name[256];           // 文件名
    uint32_t size;            // 文件大小(字节)
    uint8_t is_dir;           // 是否为目录
    uint32_t modified;        // 修改时间戳(Unix时间)
} sdcard_file_info_t;

/**
 * @brief 浏览目录中的文件
 * @param dir_path 目录路径(相对于挂载点)
 * @param out_count 输出文件数量
 * @param max_count 最大返回数量
 * @param out_files 输出文件信息数组(需预分配内存)
 * @return ESP_OK 成功，其他失败
 */
esp_err_t sdcard_browse_dir(const char *dir_path, uint32_t *out_count,
                             uint32_t max_count, sdcard_file_info_t *out_files);

/**
 * @brief 获取文件信息
 * @param file_path 文件路径(相对于挂载点)
 * @param out_info 输出文件信息
 * @return ESP_OK 成功，其他失败
 */
esp_err_t sdcard_get_file_info(const char *file_path, sdcard_file_info_t *out_info);

/**
 * @brief 获取文件大小（使用FatFS，更可靠）
 * @param vfs_path VFS路径（如 /sdcard/file.txt）
 * @param out_size 输出文件大小
 * @param out_is_dir 输出是否为目录（可选）
 * @return ESP_OK 成功，其他失败
 */
esp_err_t sdcard_get_file_size(const char *vfs_path, uint32_t *out_size, bool *out_is_dir);

/**
 * @brief 检查文件扩展名是否为图片
 * @param filename 文件名
 * @return true 是图片，false 不是
 */
bool sdcard_is_image_file(const char *filename);

/**
 * @brief 检查文件扩展名是否为视频
 * @param filename 文件名
 * @return true 是视频，false 不是
 */
bool sdcard_is_video_file(const char *filename);

/**
 * @brief 获取支持的图片格式列表
 * @return 逗号分隔的扩展名字符串
 */
const char* sdcard_get_image_extensions(void);

/**
 * @brief 获取支持的视频格式列表
 * @return 逗号分隔的扩展名字符串
 */
const char* sdcard_get_video_extensions(void);

#endif // SDCARD_H
