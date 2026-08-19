#ifndef MIC_H
#define MIC_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mic_data_callback_t)(const uint8_t *data, size_t size);

void mic_set_params(int sample_rate, int shift_bits, float volume_scale);
void mic_get_params(int *sample_rate, int *shift_bits, float *volume_scale);

esp_err_t mic_init(void);
esp_err_t mic_read(void *dest, size_t size, size_t *bytes_read);
int mic_get_sound_level(void);
void mic_set_testing(bool enable);
bool mic_is_testing(void);
void mic_register_callback(mic_data_callback_t cb);

#ifdef __cplusplus
}
#endif

#endif // MIC_H