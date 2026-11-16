idf_component_register(
    SRC_DIRS "." "logic" "drivers"
    INCLUDE_DIRS "." "logic" "drivers"
    REQUIRES shared
             esp_driver_uart esp_driver_gpio esp_driver_ledc esp_driver_pcnt
             esp_driver_rmt esp_driver_i2c esp_driver_spi
             nvs_flash esp_timer log freertos heap esp_system
)
