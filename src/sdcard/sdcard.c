/**
 * @file sdcard.c
 * @brief TF卡(SD卡)模块实现 - SDMMC 1-bit模式
 */

#include "sdcard.h"
#include "config.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "sdmmc_cmd.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char *TAG = "SDCARD";
#define LOG_TAG TAG

static bool s_sdcard_mounted = false;
static sdmmc_card_t *s_card = NULL;
static char s_driver_path[3] = "0:";

/**
 * @brief 初始化TF卡
 */
esp_err_t sdcard_init(void) {
  SDCARD_LOGI(TAG, "========== TF卡初始化开始 ==========");
  SDCARD_LOGI(TAG, "[1/6] 配置SDMMC主机...");

  // 配置SDMMC主机 - 使用高速模式(40MHz)，1-bit模式下理论吞吐翻倍
  // SDMMC驱动会与卡协商，如果卡不支持高速会自动降回20MHz
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED; // 40MHz
  SDCARD_LOGI(TAG, "    主机频率: %d kHz (HighSpeed)", host.max_freq_khz);
  SDCARD_LOGI(TAG, "    主机flags: 0x%x", host.flags);

  SDCARD_LOGI(TAG, "[2/6] 配置TF卡引脚...");
  SDCARD_LOGI(TAG, "    CLK引脚: GPIO%d", SD_MMC_CLK_PIN);
  SDCARD_LOGI(TAG, "    CMD引脚: GPIO%d", SD_MMC_CMD_PIN);
  SDCARD_LOGI(TAG, "    D0引脚:  GPIO%d", SD_MMC_D0_PIN);

  // 配置TF卡引脚
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.clk = SD_MMC_CLK_PIN;
  slot_config.cmd = SD_MMC_CMD_PIN;
  slot_config.d0 = SD_MMC_D0_PIN;

  // ESP32-S3 SDMMC使用1-bit模式
  slot_config.width = 1;
  SDCARD_LOGI(TAG, "    总线宽度: 1-bit 模式");

  // 启用内部上拉电阻
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
  SDCARD_LOGI(TAG, "    启用内部上拉电阻: 是");

  SDCARD_LOGI(TAG, "[3/6] 配置挂载选项...");
  // 挂载选项
  esp_vfs_fat_mount_config_t mount_config = {
    .format_if_mount_failed = false,  // 不自动格式化
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
  };
  SDCARD_LOGI(TAG, "    格式化失败时格式化: 否");
  SDCARD_LOGI(TAG, "    最大打开文件数: %d", mount_config.max_files);
  SDCARD_LOGI(TAG, "    分配单元大小: %d bytes", mount_config.allocation_unit_size);

  SDCARD_LOGI(TAG, "[4/6] 挂载点: /sdcard");

  SDCARD_LOGI(TAG, "[5/6] 执行 esp_vfs_fat_sdmmc_mount()...");
  SDCARD_LOGI(TAG, "    等待TF卡响应...");

  // 挂载TF卡
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config,
                                          &mount_config, &s_card);

  SDCARD_LOGI(TAG, "[6/6] 挂载结果检查...");
  if (ret != ESP_OK) {
    SDCARD_LOGE(TAG, "    !!! 挂载失败 !!!");
    SDCARD_LOGE(TAG, "    错误码: 0x%x (%s)", ret, esp_err_to_name(ret));

    if (ret == ESP_FAIL) {
      SDCARD_LOGE(TAG, "    原因: FAT文件系统错误或挂载失败");
      SDCARD_LOGE(TAG, "    建议: 尝试在电脑上重新格式化TF卡为FAT32");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      SDCARD_LOGE(TAG, "    原因: TF卡未检测到");
      SDCARD_LOGE(TAG, "    建议: 检查TF卡是否插入，或检查引脚连接");
    } else if (ret == ESP_ERR_TIMEOUT) {
      SDCARD_LOGE(TAG, "    原因: 通信超时");
      SDCARD_LOGE(TAG, "    建议: 检查TF卡质量或引脚接触");
    } else if (ret == ESP_ERR_NO_MEM) {
      SDCARD_LOGE(TAG, "    原因: 内存不足");
    } else {
      SDCARD_LOGE(TAG, "    原因: 未知错误");
    }

    s_sdcard_mounted = false;
    SDCARD_LOGI(TAG, "========== TF卡初始化失败 ==========");
    return ret;
  }

  // 获取并打印卡片信息
  SDCARD_LOGI(TAG, "    卡片信息:");
  sdmmc_card_print_info(stdout, s_card);

  // 正确计算总容量
  uint64_t card_size = (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;
  SDCARD_LOGI(TAG, "TF卡挂载成功!");
  SDCARD_LOGI(TAG, "    总容量: %.2f GB", card_size / (1024.0 * 1024.0 * 1024.0));

  s_sdcard_mounted = true;
  SDCARD_LOGI(TAG, "========== TF卡初始化完成 ==========");
  return ESP_OK;
}

/**
 * @brief 获取TF卡信息
 */
esp_err_t sdcard_get_info(uint64_t *out_total, uint64_t *out_free) {
  if (out_total == NULL || out_free == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!s_sdcard_mounted || s_card == NULL) {
    *out_total = 0;
    *out_free = 0;
    return ESP_FAIL;
  }

  // 从CSD计算物理总容量
  uint64_t csd_total = (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;

  // 使用FatFS的f_getfree获取FAT信息
  DWORD fre_clust = 0;
  FATFS *fs = NULL;
  FRESULT res = f_getfree(s_driver_path, &fre_clust, &fs);

  if (res == FR_OK && fs != NULL) {
    // FatFS成功获取信息
    uint32_t bytes_per_cluster = (uint32_t)fs->csize * 512;
    uint64_t fat_total = (uint64_t)(fs->n_fatent - 2) * bytes_per_cluster;
    uint64_t fat_free = (uint64_t)fre_clust * bytes_per_cluster;

    SDCARD_LOGI(TAG, "SD卡容量 - CSD: %.2f GB, FAT总: %.2f GB, 空闲: %.2f GB",
             csd_total / (1024.0 * 1024.0 * 1024.0),
             fat_total / (1024.0 * 1024.0 * 1024.0),
             fat_free / (1024.0 * 1024.0 * 1024.0));

    *out_total = fat_total;
    *out_free = fat_free;
  } else {
    // FatFS失败，使用CSD容量作为总容量
    SDCARD_LOGW(TAG, "f_getfree失败(res=%d)，使用CSD容量", res);
    *out_total = csd_total;
    *out_free = 0;  // 无法获取空闲空间，设为0
  }

  // 安全检查
  if (*out_free > *out_total) {
    *out_free = *out_total;
  }

  return ESP_OK;
}

/**
 * @brief 获取TF卡已用字节数
 */
uint64_t sdcard_get_used_bytes(void) {
  uint64_t total = 0, free = 0;
  if (sdcard_get_info(&total, &free) == ESP_OK) {
    return (total > free) ? (total - free) : 0;
  }
  return 0;
}

/**
 * @brief 检查TF卡是否已挂载
 */
bool sdcard_is_mounted(void) { return s_sdcard_mounted; }

/**
 * @brief 获取TF卡挂载路径
 */
const char *sdcard_get_mount_point(void) { return "/sdcard"; }

/**
 * @brief 获取TF卡CSD信息
 */
const sdmmc_csd_t *sdcard_get_csd(void) { return s_card ? &s_card->csd : NULL; }

/* ========== 文件浏览功能 ========== */

/**
 * @brief 获取文件扩展名
 */
static const char *get_file_extension(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (dot && dot != filename) {
    return dot + 1;
  }
  return "";
}

/**
 * @brief 转换FatFS时间戳为Unix时间戳
 */
static uint32_t fatfs_to_unix_time(WORD fdate, WORD ftime) {
  uint16_t year = (fdate >> 9) + 1980;
  uint16_t month = (fdate >> 5) & 0x0F;
  uint16_t day = fdate & 0x1F;
  uint16_t hour = ftime >> 11;
  uint16_t minute = (ftime >> 5) & 0x3F;
  uint16_t second = (ftime & 0x1F) * 2;

  struct tm tm_time = {.tm_sec = second,
                       .tm_min = minute,
                       .tm_hour = hour,
                       .tm_mday = day,
                       .tm_mon = month - 1,
                       .tm_year = year - 1900};

  return (uint32_t)mktime(&tm_time);
}

/**
 * @brief 浏览目录中的文件
 */
esp_err_t sdcard_browse_dir(const char *dir_path, uint32_t *out_count,
                            uint32_t max_count, sdcard_file_info_t *out_files) {
  SDCARD_LOGI(TAG, "sdcard_browse_dir - mounted: %d, path: '%s'", s_sdcard_mounted,
           dir_path ? dir_path : "(null)");

  if (!s_sdcard_mounted) {
    return ESP_FAIL;
  }

  char full_path[512];
  if (dir_path[0] != '\0') {
    snprintf(full_path, sizeof(full_path), "0:/%s", dir_path);
  } else {
    snprintf(full_path, sizeof(full_path), "0:/");
  }

  FF_DIR dir;
  FRESULT res = f_opendir(&dir, full_path);
  if (res != FR_OK) {
    SDCARD_LOGW(TAG, "无法打开目录: %s (res=%d)", full_path, res);
    return ESP_FAIL;
  }

  uint32_t count = 0;
  FILINFO fno;

  while (f_readdir(&dir, &fno) == FR_OK && count < max_count) {
    if (fno.fname[0] == 0)
      break;
    if (fno.fname[0] == '.')
      continue;

    strncpy(out_files[count].name, fno.fname,
            sizeof(out_files[count].name) - 1);
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

  SDCARD_LOGI(TAG, "浏览目录 %s: 找到 %lu 个文件", full_path,
           (unsigned long)count);
  return ESP_OK;
}

/**
 * @brief 获取文件信息
 */
esp_err_t sdcard_get_file_info(const char *file_path,
                               sdcard_file_info_t *out_info) {
  if (!s_sdcard_mounted || file_path == NULL || out_info == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // 去掉 /sdcard 前缀（如果有），转换为FatFS路径
  const char *rel_path = file_path;
  if (strncmp(file_path, "/sdcard", 7) == 0) {
    rel_path = file_path + 7;
  }

  char full_path[512];
  if (*rel_path == '/') {
    snprintf(full_path, sizeof(full_path), "0:%s", rel_path);
  } else if (*rel_path == '\0') {
    snprintf(full_path, sizeof(full_path), "0:/");
  } else {
    snprintf(full_path, sizeof(full_path), "0:/%s", rel_path);
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
 * @brief 获取文件大小（使用FatFS，更可靠）
 * @param vfs_path VFS路径（如 /sdcard/file.txt）
 * @param out_size 输出文件大小
 * @param out_is_dir 输出是否为目录
 * @return ESP_OK 成功，其他失败
 */
esp_err_t sdcard_get_file_size(const char *vfs_path, uint32_t *out_size, bool *out_is_dir) {
  if (!s_sdcard_mounted || vfs_path == NULL || out_size == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // 去掉 /sdcard 前缀（如果有），转换为FatFS路径
  const char *rel_path = vfs_path;
  if (strncmp(vfs_path, "/sdcard", 7) == 0) {
    rel_path = vfs_path + 7;
  }

  char fatfs_path[512];
  if (*rel_path == '/') {
    snprintf(fatfs_path, sizeof(fatfs_path), "0:%s", rel_path);
  } else if (*rel_path == '\0') {
    snprintf(fatfs_path, sizeof(fatfs_path), "0:/");
  } else {
    snprintf(fatfs_path, sizeof(fatfs_path), "0:/%s", rel_path);
  }

  FILINFO fno;
  FRESULT res = f_stat(fatfs_path, &fno);
  if (res != FR_OK) {
    SDCARD_LOGW(TAG, "f_stat失败: %s (res=%d)", fatfs_path, res);
    return ESP_FAIL;
  }

  *out_size = (uint32_t)fno.fsize;
  if (out_is_dir) {
    *out_is_dir = (fno.fattrib & AM_DIR) ? true : false;
  }

  return ESP_OK;
}

/**
 * @brief 检查文件扩展名是否为图片
 */
bool sdcard_is_image_file(const char *filename) {
  if (filename == NULL)
    return false;

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
bool sdcard_is_video_file(const char *filename) {
  if (filename == NULL)
    return false;

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
const char *sdcard_get_image_extensions(void) {
  return "jpg,jpeg,png,gif,bmp,webp,ico";
}

/**
 * @brief 获取支持的视频格式列表
 */
const char *sdcard_get_video_extensions(void) {
  return "mp4,avi,mkv,mov,webm,flv,wmv,3gp,mvi";
}
