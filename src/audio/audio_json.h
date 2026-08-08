/**
 * @file audio_json.h
 * @brief 简单的JSON解析器（无外部依赖）
 */
#ifndef AUDIO_JSON_H
#define AUDIO_JSON_H

#include <stdint.h>
#include <stdbool.h>

/* 简化的JSON值类型 */
typedef enum {
    JSON_TYPE_INT,
    JSON_TYPE_STRING,
    JSON_TYPE_BOOL,
    JSON_TYPE_INVALID
} json_type_t;

/* JSON值结构 */
typedef struct {
    json_type_t type;
    union {
        int int_value;
        struct {
            char* str;
            int len;
        } str_value;
        bool bool_value;
    } value;
} json_value_t;

/* JSON解析结果 */
typedef struct {
    int int_val;
    char* str_val;
    int str_len;
    bool is_valid;
} json_parse_result_t;

/**
 * @brief 跳过空白字符
 */
static inline const char* json_skip_whitespace(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    return p;
}

/**
 * @brief 解析JSON整数
 */
static inline bool json_parse_int(const char* json, const char* key, json_parse_result_t* result) {
    result->is_valid = false;
    result->int_val = 0;
    result->str_val = NULL;

    char key_buf[32];
    snprintf(key_buf, sizeof(key_buf), "\"%s\"", key);

    const char* p = strstr(json, key_buf);
    if (p == NULL) {
        return false;
    }

    p = strchr(p, ':');
    if (p == NULL) {
        return false;
    }
    p++;

    p = json_skip_whitespace(p);
    if (*p == '\0') {
        return false;
    }

    char* end;
    result->int_val = (int)strtol(p, &end, 10);
    if (end == p) {
        return false;
    }

    result->is_valid = true;
    return true;
}

/**
 * @brief 解析JSON字符串
 */
static inline bool json_parse_string(const char* json, const char* key, json_parse_result_t* result) {
    result->is_valid = false;
    result->str_val = NULL;

    char key_buf[64];
    snprintf(key_buf, sizeof(key_buf), "\"%s\"", key);

    const char* p = strstr(json, key_buf);
    if (p == NULL) {
        return false;
    }

    p = strchr(p, ':');
    if (p == NULL) {
        return false;
    }
    p++;

    p = json_skip_whitespace(p);
    if (*p != '"') {
        return false;
    }
    p++;

    const char* end = strchr(p, '"');
    if (end == NULL) {
        return false;
    }

    result->str_val = (char*)p;
    result->str_len = end - p;
    result->is_valid = true;
    return true;
}

#endif /* AUDIO_JSON_H */
