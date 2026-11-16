#include "wifi_ap.h"
#include <string.h>
#include "s3_config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

static const char *TAG = "wifi_ap";

static volatile int s_sta_count;
static bool s_enabled;

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    switch (id) {
    case WIFI_EVENT_AP_STACONNECTED:
        s_sta_count++;
        ESP_LOGI(TAG, "celular conectado (%d)", s_sta_count);
        break;
    case WIFI_EVENT_AP_STADISCONNECTED:
        if (s_sta_count > 0) s_sta_count--;
        ESP_LOGI(TAG, "celular desconectado (%d)", s_sta_count);
        break;
    case WIFI_EVENT_AP_STOP:
        s_sta_count = 0;
        break;
    default:
        break;
    }
}

void wifi_ap_init(void)
{
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t cfg = {0};
    strncpy((char *)cfg.ap.ssid, AP_SSID, sizeof(cfg.ap.ssid) - 1);
    strncpy((char *)cfg.ap.password, AP_PASSWORD, sizeof(cfg.ap.password) - 1);
    cfg.ap.ssid_len = strlen(AP_SSID);
    cfg.ap.channel = AP_CHANNEL;
    cfg.ap.max_connection = AP_MAX_CLIENTS;
    cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    cfg.ap.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    s_enabled = true;
    ESP_LOGI(TAG, "WiFi \"%s\" listo: http://192.168.4.1", AP_SSID);
}

void wifi_ap_set_enabled(bool enabled)
{
    if (enabled == s_enabled) return;
    s_enabled = enabled;
    if (enabled) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start());
        ESP_LOGI(TAG, "WiFi encendido (433 MHz inactivo)");
    } else {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
        s_sta_count = 0;
        ESP_LOGI(TAG, "WiFi apagado (enlace 433 MHz activo)");
    }
}

bool wifi_ap_enabled(void)
{
    return s_enabled;
}

int wifi_ap_sta_count(void)
{
    return s_sta_count;
}
