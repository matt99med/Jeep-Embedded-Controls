idf_component_register(
    SRCS "main.c"
         "espnow_keyfob.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_wifi esp_now nvs_flash esp_event esp_netif
)