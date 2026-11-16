// Carrito KAKATA - puerta de enlace (ESP32-S3)
//   - Punto de acceso WiFi propio: el celular se conecta y abre http://192.168.4.1
//   - Servidor web (control, mapa, ajustes y pruebas) con WebSocket
//   - Enlace SPI con el ESP32-P4, que hace todo el trabajo en tiempo real
//
// Exclusion con el 433 MHz: si el control remoto se conecta primero, el P4 le avisa al S3
// y este apaga el WiFi; si un celular se conecta primero, el P4 apaga el radio 433.
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bridge.h"
#include "spi_slave_link.h"
#include "web_server.h"
#include "wifi_ap.h"

static const char *TAG = "kakata_s3";

// Cada 100 ms: informa al P4 si hay celular asociado y obedece su permiso de tener WiFi
static void supervisor_task(void *arg)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
        bridge_set_sta(wifi_ap_sta_count() > 0);
        wifi_ap_set_enabled(bridge_wifi_allowed());
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "KAKATA carrito (ESP32-S3) iniciando");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    bridge_init();
    wifi_ap_init();
    web_server_start();
    spi_slave_link_start();
    xTaskCreate(supervisor_task, "supervisor", 3072, NULL, 4, NULL);
}
