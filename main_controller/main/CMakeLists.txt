idf_component_register(
    SRCS "main.c"
         "gpio_handler.c"
         "espnow_handler.c"
         "sim7600.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_adc esp_wifi esp_now nvs_flash esp_event esp_timer
)