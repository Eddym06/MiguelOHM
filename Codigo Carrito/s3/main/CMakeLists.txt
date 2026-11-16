idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES shared esp_wifi esp_netif esp_event nvs_flash esp_http_server json
             esp_driver_spi esp_driver_gpio log freertos esp_timer heap esp_system
    EMBED_TXTFILES "web/index.html"
)
