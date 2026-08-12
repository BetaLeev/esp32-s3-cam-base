idf_component_register(
    SRCS "actuators.c" "actuators_web.c" "led.c" "led_web.c" "pulse.c" "pulse_web.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server driver freertos
)
