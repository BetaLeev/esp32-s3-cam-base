idf_component_register(
    SRCS "audio.c" "audio_web.c" "audio_i2s.cpp" "audio_async.cpp" "audio_simple.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_http_server
        driver
        freertos
        esp_netif
        esp_timer
        nvs_flash
)
