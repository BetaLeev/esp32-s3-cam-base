/**
 * @file ai.h
 * @brief AI 语音交互核心模块
 *
 * 提供语音识别、唤醒管理等功能
 */
#ifndef AI_H
#define AI_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/* ========================================
 * 状态定义
 * ======================================== */

/**
 * @brief AI 模块状态
 */
typedef enum {
    AI_STATE_UNINIT = 0,     /**< 未初始化 */
    AI_STATE_INITING,        /**< 初始化中 */
    AI_STATE_IDLE,           /**< 空闲 */
    AI_STATE_LISTENING,      /**< 监听中 */
    AI_STATE_RECOGNIZING,    /**< 识别中 */
    AI_STATE_PROCESSING,      /**< 处理中 */
    AI_STATE_ERROR           /**< 错误 */
} ai_state_t;

/**
 * @brief AI 服务状态
 */
typedef struct {
    ai_state_t state;            /**< 当前状态 */
    bool initialized;             /**< 是否已初始化 */
    bool ws_connected;            /**< WebSocket 是否连接 */
    uint32_t recognize_count;    /**< 识别次数 */
    uint32_t success_count;      /**< 成功次数 */
    uint32_t fail_count;         /**< 失败次数 */
} ai_service_status_t;

/* ========================================
 * 函数声明
 * ======================================== */

/**
 * @brief 初始化 AI 模块
 */
esp_err_t ai_init(void);

/**
 * @brief 反初始化 AI 模块
 */
esp_err_t ai_deinit(void);

/**
 * @brief 获取 AI 模块状态
 */
ai_state_t ai_get_state(void);

/**
 * @brief 获取 AI 服务状态
 */
const ai_service_status_t *ai_get_service_status(void);

/**
 * @brief 检查 AI 模块是否已初始化
 */
bool ai_is_initialized(void);

/**
 * @brief 重置统计数据
 */
void ai_reset_stats(void);

/**
 * @brief 更新识别统计
 */
void ai_inc_recognize(bool success);

/**
 * @brief 设置 WebSocket 连接状态
 */
void ai_set_ws_connected(bool connected);

/**
 * @brief AI 状态转字符串
 */
const char *ai_state_to_string(ai_state_t state);

#endif /* AI_H */
