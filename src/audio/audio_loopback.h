#ifndef AUDIO_LOOPBACK_H
#define AUDIO_LOOPBACK_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 开启或关闭麦克风实时扩音（直通扬声器）
 */
void audio_loopback_set_enabled(bool enable);

/**
 * @brief 检查当前扩音是否开启
 */
bool audio_loopback_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_LOOPBACK_H */