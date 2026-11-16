// Carrito KAKATA - cerebro (ESP32-P4)
//   433 MHz  <-> control remoto          (drivers/rc433.c)
//   SPI      <-> ESP32-S3 (WiFi + web)   (drivers/spi_master_link.c)
//   sensores: BMI160, INA219, brujula, GPS M100 Pro, LIDAR D6
//   traccion: 2 motores JGB37-520 + L298N a 5 kHz, PID con encoders, servo MG90
//   luces: faros, RGB de ruedas, tira WS2812 del aleron
//
// Los pines estan en board_pins.h (pendientes de la PCB).
#include "esp_log.h"
#include "car_state.h"
#include "settings.h"
#include "sensors.h"
#include "drive.h"
#include "link_mgr.h"
#include "drivers/gps.h"
#include "drivers/lidar.h"

static const char *TAG = "kakata";

void app_main(void)
{
    ESP_LOGI(TAG, "KAKATA carrito (ESP32-P4) iniciando");
    car_state_init();
    settings_init();

    sensors_start();   // I2C: BMI160, brujula, INA219
    gps_start();
    lidar_start();
    drive_start();     // motores, servo, luces
    link_mgr_start();  // 433 MHz + SPI al S3 + arbitraje

    ESP_LOGI(TAG, "listo");
}
