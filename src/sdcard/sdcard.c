/**
 * @file sdcard.c
 * @brief TF卡(SD卡)模块实现 - SDMMC 1-bit模式
 */

#include "sdcard.h"
#include "config.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "ff.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "SDCARD";

static bool s_sdcard_mounted = false;
static sdmmc_card_t *s_card = NULL;
static char s_driver_path[3] = "0:";

/**
 * @brief 初始化TF卡
 */
esp_err_t sdcard_init(void)
{
    ESP_LOGI(TAG, "初始化TF卡...");

    // 配置SDMMC主机
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    // 配置TF卡引脚
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = SD_MMC_CLK_PIN;
    slot_config.cmd = SD_MMC_CMD_PIN;
    slot_config.d0 = SD_MMC_D0_PIN;

    // ESP32-S3 SDMMC使用1-bit模式
    slot_config.width = 1;

    // 启用内部上拉电阻
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    // 挂载选项
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    // 挂载TF卡
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &s_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGW(TAG, "TF卡挂载失败，可能是文件系统问题");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG, "TF卡未插入或引脚连接问题");
        } else {
            ESP_LOGE(TAG, "TF卡初始化失败: %s", esp_err_to_name(ret));
        }
        s_sdcard_mounted = false;
        return ret;
    }

    // 获取并打印卡片信息
    sdmmc_card_print_info(stdout, s_card);

    // 正确计算总容量
    uint64_t card_size = (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;
    ESP_LOGI(TAG, "TF卡挂载成功! 总容量: %.2f GB", card_size / (1024.0 * 1024.0 * 1024.0));

    s_sdcard_mounted = true;
    return ESP_OK;
}

/**
 * @brief 获取TF卡信息
 */
esp_err_t sdcard_get_info(uint64_t *out_total, uint64_t *out_free)
{
    if (out_total == NULL || out_free == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_sdcard_mounted || s_card == NULL) {
        *out_total = 0;
        *out_free = 0;
        return ESP_FAIL;
    }

    // 从CSD计算总容量
    uint64_t csd_total = (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;

    // 使用FatFS的f_getfree获取空闲簇数
    DWORD fre_clust;
    FATFS *fs = NULL;
    FRESULT res = f_getfree(s_driver_path, &fre_clust, &fs);

    uint64_t fat_free = 0;
    uint64_t fat_total = 0;
    if (res == FR_OK && fs != NULL) {
        uint32_t cluster_size = (uint32_t)fs->csize * 512;
        DWORD total_clust = (DWORD)(fs->n_fatent - 2);
        fat_total = (uint64_t)total_clust * cluster_size;
        fat_free = (uint64_t)fre_clust * cluster_size;

        ESP_LOGD(TAG, "FAT总容量: %.2f GB, 空闲: %.2f GB",
                 fat_total / (1024.0 * 1024.0 * 1024.0),
                 fat_free / (1024.0 * 1024.0 * 1024.0));

        *out_total = fat_total;
        *out_free = fat_free;
    } else {
        ESP_LOGW(TAG, "f_getfree失败: res=%d", res);
        *out_total = csd_total;
        *out_free = csd_total;
    }

    if (*out_free > *out_total) {
        *out_free = *out_total;
    }

    return ESP_OK;
}

/**
 * @brief 获取TF卡已用字节数
 */
uint64_t sdcard_get_used_bytes(void)
{
    uint64_t total = 0, free = 0;
    if (sdcard_get_info(&total, &free) == ESP_OK) {
        return (total > free) ? (total - free) : 0;
    }
    return 0;
}

/**
 * @brief 检查TF卡是否已挂载
 */
bool sdcard_is_mounted(void)
{
    return s_sdcard_mounted;
}

/**
 * @brief 获取TF卡挂载路径
 */
const char* sdcard_get_mount_point(void)
{
    return "/sdcard";
}

/**
 * @brief 获取TF卡CSD信息
 */
const sdmmc_csd_t* sdcard_get_csd(void)
{
    return s_card ? &s_card->csd : NULL;
}

/* ========== 文件浏览功能 ========== */

/**
 * @brief 获取文件扩展名
 */
static const char* get_file_extension(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (dot && dot != filename) {
        return dot + 1;
    }
    return "";
}

/**
 * @brief 转换FatFS时间戳为Unix时间戳
 */
static uint32_t fatfs_to_unix_time(WORD fdate, WORD ftime)
{
    uint16_t year = (fdate >> 9) + 1980;
    uint16_t month = (fdate >> 5) & 0x0F;
    uint16_t day = fdate & 0x1F;
    uint16_t hour = ftime >> 11;
    uint16_t minute = (ftime >> 5) & 0x3F;
    uint16_t second = (ftime & 0x1F) * 2;

    struct tm tm_time = {
        .tm_sec = second,
        .tm_min = minute,
        .tm_hour = hour,
        .tm_mday = day,
        .tm_mon = month - 1,
        .tm_year = year - 1900
    };

    return (uint32_t)mktime(&tm_time);
}

/**
 * @brief 浏览目录中的文件
 */
esp_err_t sdcard_browse_dir(const char *dir_path, uint32_t *out_count,
                             uint32_t max_count, sdcard_file_info_t *out_files)
{
    ESP_LOGI(TAG, "sdcard_browse_dir - mounted: %d, path: '%s'", s_sdcard_mounted, dir_path ? dir_path : "(null)");

    if (!s_sdcard_mounted) {
        return ESP_FAIL;
    }

    char full_path[512];
    if (dir_path && strlen(dir_path) > 0) {
        snprintf(full_path, sizeof(full_path), "0:/%s", dir_path);
    } else {
        snprintf(full_path, sizeof(full_path), "0:/");
    }

    FF_DIR dir;
    FRESULT res = f_opendir(&dir, full_path);
    if (res != FR_OK) {
        ESP_LOGW(TAG, "无法打开目录: %s (res=%d)", full_path, res);
        return ESP_FAIL;
    }

    uint32_t count = 0;
    FILINFO fno;

    while (f_readdir(&dir, &fno) == FR_OK && count < max_count) {
        if (fno.fname[0] == 0) break;
        if (fno.fname[0] == '.') continue;

        strncpy(out_files[count].name, fno.fname, sizeof(out_files[count].name) - 1);
        out_files[count].name[sizeof(out_files[count].name) - 1] = '\0';
        out_files[count].size = (uint32_t)fno.fsize;
        out_files[count].is_dir = (fno.fattrib & AM_DIR) ? 1 : 0;
        out_files[count].modified = fatfs_to_unix_time(fno.fdate, fno.ftime);

        count++;
    }

    f_closedir(&dir);

    if (out_count) {
        *out_count = count;
    }

    ESP_LOGI(TAG, "浏览目录 %s: 找到 %lu 个文件", full_path, (unsigned long)count);
    return ESP_OK;
}

/**
 * @brief 获取文件信息
 */
esp_err_t sdcard_get_file_info(const char *file_path, sdcard_file_info_t *out_info)
{
    if (!s_sdcard_mounted || file_path == NULL || out_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char full_path[512];
    if (file_path[0] == '/') {
        snprintf(full_path, sizeof(full_path), "0:%s", file_path);
    } else {
        snprintf(full_path, sizeof(full_path), "0:/%s", file_path);
    }

    FILINFO fno;
    FRESULT res = f_stat(full_path, &fno);
    if (res != FR_OK) {
        return ESP_FAIL;
    }

    strncpy(out_info->name, fno.fname, sizeof(out_info->name) - 1);
    out_info->name[sizeof(out_info->name) - 1] = '\0';
    out_info->size = (uint32_t)fno.fsize;
    out_info->is_dir = (fno.fattrib & AM_DIR) ? 1 : 0;
    out_info->modified = fatfs_to_unix_time(fno.fdate, fno.ftime);

    return ESP_OK;
}

/**
 * @brief 检查文件扩展名是否为图片
 */
bool sdcard_is_image_file(const char *filename)
{
    if (filename == NULL) return false;

    const char *ext = get_file_extension(filename);
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
        strcasecmp(ext, "png") == 0 || strcasecmp(ext, "gif") == 0 ||
        strcasecmp(ext, "bmp") == 0 || strcasecmp(ext, "webp") == 0 ||
        strcasecmp(ext, "ico") == 0) {
        return true;
    }
    return false;
}

/**
 * @brief 检查文件扩展名是否为视频
 */
bool sdcard_is_video_file(const char *filename)
{
    if (filename == NULL) return false;

    const char *ext = get_file_extension(filename);
    if (strcasecmp(ext, "mp4") == 0 || strcasecmp(ext, "avi") == 0 ||
        strcasecmp(ext, "mkv") == 0 || strcasecmp(ext, "mov") == 0 ||
        strcasecmp(ext, "webm") == 0 || strcasecmp(ext, "flv") == 0 ||
        strcasecmp(ext, "wmv") == 0 || strcasecmp(ext, "3gp") == 0) {
        return true;
    }
    return false;
}

/**
 * @brief 获取支持的图片格式列表
 */
const char* sdcard_get_image_extensions(void)
{
    return "jpg,jpeg,png,gif,bmp,webp,ico";
}

/**
 * @brief 获取支持的视频格式列表
 */
const char* sdcard_get_video_extensions(void)
{
    return "mp4,avi,mkv,mov,webm,flv,wmv,3gp,mvi";
}
