#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_CONF_SKIP 0

/* 单色黑白屏必须为 1 */
#define LV_COLOR_DEPTH 1

/* 内存与字体配置 */
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_FREE    free
    #define LV_MEM_CUSTOM_REALLOC realloc
#endif

#define LV_FONT_SIMSUN_16_CJK 1
#define LV_FONT_DEFAULT &lv_font_simsun_16_cjk

#endif /* LV_CONF_H */