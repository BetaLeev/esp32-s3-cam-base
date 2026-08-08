/**
 * @file mime_utils.c
 * @brief MIME类型工具函数实现
 */

#include "mime_utils.h"
#include <string.h>
#include <strings.h>

const char *mime_get_type(const char *filename)
{
    if (filename == NULL) {
        return "application/octet-stream";
    }

    const char *ext = NULL;
    const char *dot = strrchr(filename, '.');
    if (dot != NULL && dot != filename) {
        ext = dot + 1;
    }

    if (ext == NULL) {
        return "application/octet-stream";
    }

    /* 图片 */
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) {
        return "image/jpeg";
    }
    if (strcasecmp(ext, "png") == 0) {
        return "image/png";
    }
    if (strcasecmp(ext, "gif") == 0) {
        return "image/gif";
    }
    if (strcasecmp(ext, "bmp") == 0) {
        return "image/bmp";
    }
    if (strcasecmp(ext, "webp") == 0) {
        return "image/webp";
    }
    if (strcasecmp(ext, "ico") == 0) {
        return "image/x-icon";
    }
    if (strcasecmp(ext, "svg") == 0) {
        return "image/svg+xml";
    }

    /* 音频 */
    if (strcasecmp(ext, "mp3") == 0) {
        return "audio/mpeg";
    }
    if (strcasecmp(ext, "wav") == 0) {
        return "audio/wav";
    }
    if (strcasecmp(ext, "ogg") == 0) {
        return "audio/ogg";
    }
    if (strcasecmp(ext, "flac") == 0) {
        return "audio/flac";
    }
    if (strcasecmp(ext, "aac") == 0 || strcasecmp(ext, "m4a") == 0) {
        return "audio/mp4";
    }

    /* 视频 */
    if (strcasecmp(ext, "mp4") == 0) {
        return "video/mp4";
    }
    if (strcasecmp(ext, "webm") == 0) {
        return "video/webm";
    }
    if (strcasecmp(ext, "mkv") == 0) {
        return "video/x-matroska";
    }
    if (strcasecmp(ext, "avi") == 0) {
        return "video/x-msvideo";
    }

    /* 文档 */
    if (strcasecmp(ext, "pdf") == 0) {
        return "application/pdf";
    }
    if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
        return "text/html";
    }
    if (strcasecmp(ext, "css") == 0) {
        return "text/css";
    }
    if (strcasecmp(ext, "js") == 0) {
        return "application/javascript";
    }
    if (strcasecmp(ext, "json") == 0) {
        return "application/json";
    }
    if (strcasecmp(ext, "xml") == 0) {
        return "application/xml";
    }
    if (strcasecmp(ext, "txt") == 0 || strcasecmp(ext, "log") == 0 || strcasecmp(ext, "md") == 0) {
        return "text/plain";
    }

    /* 压缩文件 */
    if (strcasecmp(ext, "zip") == 0) {
        return "application/zip";
    }
    if (strcasecmp(ext, "tar") == 0) {
        return "application/x-tar";
    }
    if (strcasecmp(ext, "gz") == 0 || strcasecmp(ext, "gzip") == 0) {
        return "application/gzip";
    }

    /* 字体 */
    if (strcasecmp(ext, "ttf") == 0) {
        return "font/ttf";
    }
    if (strcasecmp(ext, "woff") == 0) {
        return "font/woff";
    }
    if (strcasecmp(ext, "woff2") == 0) {
        return "font/woff2";
    }

    return "application/octet-stream";
}

const char *mime_get_icon(const char *filename, int is_dir)
{
    if (is_dir) {
        return "folder";
    }

    if (filename == NULL) {
        return "file";
    }

    const char *ext = NULL;
    const char *dot = strrchr(filename, '.');
    if (dot != NULL && dot != filename) {
        ext = dot + 1;
    }

    if (ext == NULL) {
        return "file";
    }

    /* 图片 */
    if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
        strcasecmp(ext, "png") == 0 || strcasecmp(ext, "gif") == 0 ||
        strcasecmp(ext, "bmp") == 0 || strcasecmp(ext, "webp") == 0 ||
        strcasecmp(ext, "svg") == 0) {
        return "image";
    }

    /* 音频 */
    if (strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "wav") == 0 ||
        strcasecmp(ext, "flac") == 0 || strcasecmp(ext, "ogg") == 0 ||
        strcasecmp(ext, "aac") == 0 || strcasecmp(ext, "m4a") == 0) {
        return "audio";
    }

    /* 视频 */
    if (strcasecmp(ext, "mp4") == 0 || strcasecmp(ext, "avi") == 0 ||
        strcasecmp(ext, "mkv") == 0 || strcasecmp(ext, "mov") == 0 ||
        strcasecmp(ext, "webm") == 0) {
        return "video";
    }

    /* 文档 */
    if (strcasecmp(ext, "pdf") == 0) {
        return "pdf";
    }
    if (strcasecmp(ext, "txt") == 0 || strcasecmp(ext, "log") == 0 ||
        strcasecmp(ext, "md") == 0 || strcasecmp(ext, "html") == 0 ||
        strcasecmp(ext, "css") == 0 || strcasecmp(ext, "js") == 0 ||
        strcasecmp(ext, "json") == 0 || strcasecmp(ext, "xml") == 0) {
        return "text";
    }

    /* 压缩文件 */
    if (strcasecmp(ext, "zip") == 0 || strcasecmp(ext, "tar") == 0 ||
        strcasecmp(ext, "gz") == 0) {
        return "archive";
    }

    return "file";
}
