idf_component_register(
    SRCS "sensors.c" "sensors_web.c"
        "thermistor/thermistor.c"
        "photosensor/photosensor.c"
        "dht11/dht11.c"
        "adc_manager.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server driver freertos esp_adc
)
