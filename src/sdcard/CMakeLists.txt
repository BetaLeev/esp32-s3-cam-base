idf_component_register(
    SRCS "sdcard.c" "sdcard_web.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server fatfs driver
)
