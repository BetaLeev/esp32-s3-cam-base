/**
 * @file hw_storage.h
 * @brief 存储配置 (SPIFFS / SD卡)
 */
#ifndef HW_STORAGE_H
#define HW_STORAGE_H

#include <stdint.h>

/* ========================================
 * SPIFFS 配置
 * ======================================== */
#define SPIFFS_PARTITION_LABEL    "spiffs"
#define SPIFFS_MOUNT_POINT        "/spiffs"
#define SPIFFS_MAX_FILES          10
#define SPIFFS_FORMAT_ON_MOUNT    0   /**< 0=不格式化 1=格式化 */

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
#define LOG_FILE_PATH             "/spiffs/logs/system.log"
#define LOG_FILE_MAX_SIZE         1024*1024    /**< 单个日志文件最大 1MB */
#define LOG_FILE_MAX_COUNT        5           /**< 保留最近5个日志文件 */

/* ========================================
 * 配置备份 (预留)
 * ======================================== */
#define CONFIG_BACKUP_ENABLED     0
#define CONFIG_BACKUP_PATH        "/spiffs/config.backup.json"

#endif /* HW_STORAGE_H */
