/**
 * @file ai.c
 * @brief AI 语音交互核心模块实现
 */
#include "ai.h"
#include "config.h"
#include "esp_timer.h"

#define LOG_TAG "AI"

static ai_service_status_t s_ai_status = {
    .state = AI_STATE_UNINIT,
    .initialized = false,
    .ws_connected = false,
    .recognize_count = 0,
    .success_count = 0,
    .fail_count = 0
};

static int64_t s_last_update_time = 0;

/* ========================================
 * 内部函数
 * ======================================== */

static void update_state(ai_state_t new_state) {
    s_ai_status.state = new_state;
    s_last_update_time = esp_timer_get_time() / 1000;
    AI_LOGD(LOG_TAG, "AI 状态变更: %d", new_state);
}

/* ========================================
 * 核心函数实现
 * ======================================== */

esp_err_t ai_init(void) {
    if (s_ai_status.initialized) {
        AI_LOGW(LOG_TAG, "AI 模块已经初始化");
        return ESP_OK;
    }

    AI_LOGI(LOG_TAG, "初始化 AI 模块...");

    update_state(AI_STATE_INITING);

    // TODO: 后续在此初始化 WakeNet、MultiNet 等模型
    // 目前为空实现，等待麦克风硬件到位

    update_state(AI_STATE_IDLE);
    s_ai_status.initialized = true;

    AI_LOGI(LOG_TAG, "AI 模块初始化完成");
    return ESP_OK;
}

esp_err_t ai_deinit(void) {
    if (!s_ai_status.initialized) {
        return ESP_OK;
    }

    AI_LOGI(LOG_TAG, "反初始化 AI 模块...");

    update_state(AI_STATE_UNINIT);
    s_ai_status.initialized = false;
    s_ai_status.ws_connected = false;

    AI_LOGI(LOG_TAG, "AI 模块已反初始化");
    return ESP_OK;
}

ai_state_t ai_get_state(void) {
    return s_ai_status.state;
}

const ai_service_status_t *ai_get_service_status(void) {
    return &s_ai_status;
}

bool ai_is_initialized(void) {
    return s_ai_status.initialized;
}

void ai_reset_stats(void) {
    s_ai_status.recognize_count = 0;
    s_ai_status.success_count = 0;
    s_ai_status.fail_count = 0;
    AI_LOGI(LOG_TAG, "AI 统计数据已重置");
}

/* ========================================
 * 辅助函数
 * ======================================== */

void ai_inc_recognize(bool success) {
    s_ai_status.recognize_count++;
    if (success) {
        s_ai_status.success_count++;
    } else {
        s_ai_status.fail_count++;
    }
}

void ai_set_ws_connected(bool connected) {
    s_ai_status.ws_connected = connected;
}

const char *ai_state_to_string(ai_state_t state) {
    switch (state) {
        case AI_STATE_UNINIT:
            return "uninitialized";
        case AI_STATE_INITING:
            return "initializing";
        case AI_STATE_IDLE:
            return "idle";
        case AI_STATE_LISTENING:
            return "listening";
        case AI_STATE_RECOGNIZING:
            return "recognizing";
        case AI_STATE_PROCESSING:
            return "processing";
        case AI_STATE_ERROR:
            return "error";
        default:
            return "unknown";
    }
}
