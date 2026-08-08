idf_component_register(
    SRCS "actuators.c" "actuators_web.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server driver freertos
)
