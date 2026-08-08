idf_component_register(
    SRCS "audio.c" "audio_web.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server driver freertos esp_netif
)
