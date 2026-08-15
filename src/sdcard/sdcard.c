/**
 * @file sdcard.c
 * @brief TF卡(SD卡)模块实现 - 读写性能提升版
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

#define LOG_TAG "SDCARD"

static bool s_sdcard_mounted = false;
static sdmmc_card_t *s_card = NULL;
static char s_driver_path[3] = "0:";

/**
 * @brief 初始化TF卡
 */
esp_err_t sdcard_init(void) {
  SDCARD_LOGI(TAG, "========== TF卡初始化开始 ==========");
  SDCARD_LOGI(TAG, "[1/6] 配置SDMMC主机...");

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED; // 40MHz
  SDCARD_LOGI(TAG, "    主机频率: %d kHz (HighSpeed)", host.max_freq_khz);
  SDCARD_LOGI(TAG, "    主机flags: 0x%x", host.flags);

  SDCARD_LOGI(TAG, "[2/6] 配置TF卡引脚...");
  SDCARD_LOGI(TAG, "    CLK引脚: GPIO%d", SD_MMC_CLK_PIN);
  SDCARD_LOGI(TAG, "    CMD引脚: GPIO%d", SD_MMC_CMD_PIN);
  SDCARD_LOGI(TAG, "    D0引脚:  GPIO%d", SD_MMC_D0_PIN);

  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.clk = SD_MMC_CLK_PIN;
  slot_config.cmd = SD_MMC_CMD_PIN;
  slot_config.d0 = SD_MMC_D0_PIN;

  // =========================================================================
  // 【优化提示】如你的板子 D1, D2, D3 已正确连接，请注释掉 width = 1 这一行，
  // 释放下方 4-bit 模式相关代码，传输速度会直接翻三倍以上。
  // =========================================================================
  slot_config.width = 1; 
  SDCARD_LOGI(TAG, "    总线宽度: 1-bit 模式");

  /* 
  // --- SDMMC 4-BIT 高速模式配置预留区 ---
  slot_config.width = 4;
  slot_config.d1 = SD_MMC_D1_PIN; // 需在 config.h 中定义
  slot_config.d2 = SD_MMC_D2_PIN; // 需在 config.h 中定义
  slot_config.d3 = SD_MMC_D3_PIN; // 需在 config.h 中定义
  SDCARD_LOGI(TAG, "    总线宽度: 4-bit 极速模式启用");
  */

  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
  SDCARD_LOGI(TAG, "    启用内部上拉电阻: 是");

  SDCARD_LOGI(TAG, "[3/6] 配置挂载选项...");
  
  // 核心优化点：FatFS格式化/挂载的簇大小强制设为 32KB（有效减少FAT表跳转与IO操作频次）
  esp_vfs_fat_mount_config_t mount_config = {
    .format_if_mount_failed = false,  
    .max_files = 5,
    .allocation_unit_size = 32 * 1024 
  };
  SDCARD_LOGI(TAG, "    最大打开文件数: %d", mount_config.max_files);
  SDCARD_LOGI(TAG, "    分配单元(簇)大小: %d bytes [Performance Optimized]", mount_config.allocation_unit_size);

  SDCARD_LOGI(TAG, "[4/6] 挂载点: /sdcard");

  SDCARD_LOGI(TAG, "[5/6] 执行 esp_vfs_fat_sdmmc_mount()...");
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config,
                                          &mount_config, &s_card);

  SDCARD_LOGI(TAG, "[6/6] 挂载结果检查...");
  if (ret != ESP_OK) {
    SDCARD_LOGE(TAG, "    !!! 挂载失败 !!!");
    SDCARD_LOGE(TAG, "    错误码: 0x%x (%s)", ret, esp_err_to_name(ret));
    s_sdcard_mounted = false;
    return ret;
  }

  SDCARD_LOGI(TAG, "    卡片信息:");
  sdmmc_card_print_info(stdout, s_card);

  uint64_t card_size = (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;
  SDCARD_LOGI(TAG, "TF卡挂载成功!");
  SDCARD_LOGI(TAG, "    总容量: %.2f GB", (double)card_size / (1024.0 * 1024.0 * 1024.0));
  (void)card_size;  

  s_sdcard_mounted = true;
  SDCARD_LOGI(TAG, "========== TF卡初始化完成 ==========");
  return ESP_OK;
}

/**
 * @brief 获取TF卡信息
 */
esp_err_t sdcard_get_info(uint64_t *out_total, uint64_t *out_free) {
  if (out_total == NULL || out_free == NULL) return ESP_ERR_INVALID_ARG;

  if (!s_sdcard_mounted || s_card == NULL) {
    *out_total = 0; *out_free = 0;
    return ESP_FAIL;
  }

  uint64_t csd_total = (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;

  DWORD fre_clust = 0;
  FATFS *fs = NULL;
  FRESULT res = f_getfree(s_driver_path, &fre_clust, &fs);

  if (res == FR_OK && fs != NULL) {
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
    *out_total = csd_total;
    *out_free = 0; 
  }

  if (*out_free > *out_total) {
    *out_free = *out_total;
  }

  return ESP_OK;
}

uint64_t sdcard_get_used_bytes(void) {
  uint64_t total = 0, free = 0;
  if (sdcard_get_info(&total, &free) == ESP_OK) {
    return (total > free) ? (total - free) : 0;
  }
  return 0;
}

bool sdcard_is_mounted(void) { return s_sdcard_mounted; }

const char *sdcard_get_mount_point(void) { return "/sdcard"; }

const sdmmc_csd_t *sdcard_get_csd(void) { return s_card ? &s_card->csd : NULL; }

/* ========== 文件浏览功能 ========== */

static const char *get_file_extension(const char *filename) {
  const char *dot = strrchr(filename, '.');
  if (dot && dot != filename) return dot + 1;
  return "";
}

static uint32_t fatfs_to_unix_time(WORD fdate, WORD ftime) {
  uint16_t year = (fdate >> 9) + 1980;
  uint16_t month = (fdate >> 5) & 0x0F;
  uint16_t day = fdate & 0x1F;
  uint16_t hour = ftime >> 11;
  uint16_t minute = (ftime >> 5) & 0x3F;
  uint16_t second = (ftime & 0x1F) * 2;

  struct tm tm_time = {.tm_sec = second, .tm_min = minute, .tm_hour = hour,
                       .tm_mday = day, .tm_mon = month - 1, .tm_year = year - 1900};
  return (uint32_t)mktime(&tm_time);
}

esp_err_t sdcard_browse_dir(const char *dir_path, uint32_t *out_count,
                            uint32_t max_count, sdcard_file_info_t *out_files) {
  if (!s_sdcard_mounted) return ESP_FAIL;

  char full_path[512];
  if (dir_path[0] != '\0') snprintf(full_path, sizeof(full_path), "0:/%s", dir_path);
  else snprintf(full_path, sizeof(full_path), "0:/");

  FF_DIR dir;
  FRESULT res = f_opendir(&dir, full_path);
  if (res != FR_OK) return ESP_FAIL;

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

  if (out_count) *out_count = count;
  return ESP_OK;
}

esp_err_t sdcard_get_file_info(const char *file_path, sdcard_file_info_t *out_info) {
  if (!s_sdcard_mounted || file_path == NULL || out_info == NULL) return ESP_ERR_INVALID_ARG;

  const char *rel_path = file_path;
  if (strncmp(file_path, "/sdcard", 7) == 0) rel_path = file_path + 7;

  char full_path[512];
  if (*rel_path == '/') snprintf(full_path, sizeof(full_path), "0:%s", rel_path);
  else if (*rel_path == '\0') snprintf(full_path, sizeof(full_path), "0:/");
  else snprintf(full_path, sizeof(full_path), "0:/%s", rel_path);

  FILINFO fno;
  FRESULT res = f_stat(full_path, &fno);
  if (res != FR_OK) return ESP_FAIL;

  strncpy(out_info->name, fno.fname, sizeof(out_info->name) - 1);
  out_info->name[sizeof(out_info->name) - 1] = '\0';
  out_info->size = (uint32_t)fno.fsize;
  out_info->is_dir = (fno.fattrib & AM_DIR) ? 1 : 0;
  out_info->modified = fatfs_to_unix_time(fno.fdate, fno.ftime);

  return ESP_OK;
}

esp_err_t sdcard_get_file_size(const char *vfs_path, uint32_t *out_size, bool *out_is_dir) {
  if (!s_sdcard_mounted || vfs_path == NULL || out_size == NULL) return ESP_ERR_INVALID_ARG;

  const char *rel_path = vfs_path;
  if (strncmp(vfs_path, "/sdcard", 7) == 0) rel_path = vfs_path + 7;

  char fatfs_path[512];
  if (*rel_path == '/') snprintf(fatfs_path, sizeof(fatfs_path), "0:%s", rel_path);
  else if (*rel_path == '\0') snprintf(fatfs_path, sizeof(fatfs_path), "0:/");
  else snprintf(fatfs_path, sizeof(fatfs_path), "0:/%s", rel_path);

  FILINFO fno;
  FRESULT res = f_stat(fatfs_path, &fno);
  if (res != FR_OK) return ESP_FAIL;

  *out_size = (uint32_t)fno.fsize;
  if (out_is_dir) *out_is_dir = (fno.fattrib & AM_DIR) ? true : false;

  return ESP_OK;
}

bool sdcard_is_image_file(const char *filename) {
  if (filename == NULL) return false;
  const char *ext = get_file_extension(filename);
  if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
      strcasecmp(ext, "png") == 0 || strcasecmp(ext, "gif") == 0 ||
      strcasecmp(ext, "bmp") == 0 || strcasecmp(ext, "webp") == 0 ||
      strcasecmp(ext, "ico") == 0) return true;
  return false;
}

bool sdcard_is_video_file(const char *filename) {
  if (filename == NULL) return false;
  const char *ext = get_file_extension(filename);
  if (strcasecmp(ext, "mp4") == 0 || strcasecmp(ext, "avi") == 0 ||
      strcasecmp(ext, "mkv") == 0 || strcasecmp(ext, "mov") == 0 ||
      strcasecmp(ext, "webm") == 0 || strcasecmp(ext, "flv") == 0 ||
      strcasecmp(ext, "wmv") == 0 || strcasecmp(ext, "3gp") == 0) return true;
  return false;
}

const char *sdcard_get_image_extensions(void) { return "jpg,jpeg,png,gif,bmp,webp,ico"; }
const char *sdcard_get_video_extensions(void) { return "mp4,avi,mkv,mov,webm,flv,wmv,3gp,mvi"; }