/**
 * @file hw_storage.h
 * @brief 存储配置 (SPIFFS / SD卡)
 */
#ifndef HW_STORAGE_H
#define HW_STORAGE_H

#include <stdint.h>

/* ========================================
 * LittleFS 配置 (用于 Web 文件系统)
 * ======================================== */
#define LITTLEFS_PARTITION_LABEL  "webfs"
#define LITTLEFS_MOUNT_POINT      "/web"
#define LITTLEFS_FORMAT_ON_MOUNT  1   /**< 0=不格式化 1=格式化 */

/* 兼容性别名 (旧代码可能引用) */
#define SPIFFS_PARTITION_LABEL    LITTLEFS_PARTITION_LABEL
#define SPIFFS_MOUNT_POINT        LITTLEFS_MOUNT_POINT
#define SPIFFS_FORMAT_ON_MOUNT    LITTLEFS_FORMAT_ON_MOUNT

/* ========================================
 * SD 卡配置
 * ======================================== */
#define SD_CARD_MOUNT_POINT       "/sdcard"
#define SD_CARD_MAX_FILES         20
#define SD_CARD_FORMAT_ON_MOUNT   0   /**< 0=不格式化 1=格式化 */

/* SDMMC 总线宽度 */
#define SD_BUS_WIDTH_1BIT         1   /**< 1位模式 (当前使用) */
#define SD_BUS_WIDTH_4BIT         4   /**< 4位模式 */

/* ========================================
 * 日志配置 (预留)
 * ======================================== */
#define LOG_TO_FILE_ENABLED       0
#define LOG_FILE_PATH             "/web/logs/system.log"
#define LOG_FILE_MAX_SIZE         1024*1024    /**< 单个日志文件最大 1MB */
#define LOG_FILE_MAX_COUNT        5           /**< 保留最近5个日志文件 */

/* ========================================
 * 配置备份 (预留)
 * ======================================== */
#define CONFIG_BACKUP_ENABLED     0
#define CONFIG_BACKUP_PATH        "/web/config.backup.json"

#endif /* HW_STORAGE_H */
