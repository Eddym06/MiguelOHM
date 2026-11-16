#include "gps.h"
#include "board_pins.h"
#include "car_state.h"
#include "nmea.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "gps";

static const uint32_t BAUDS[] = {38400, 115200, 9600, 57600};

static void gps_task(void *arg)
{
    const uart_port_t port = (uart_port_t)GPS_UART_NUM;
    nmea_t nmea;
    nmea_init(&nmea);
    int bi = 0;
    uint32_t last_ok_ms = now_ms();
    uint32_t last_ok_count = 0;
    uint8_t buf[128];

    for (;;) {
        int n = uart_read_bytes(port, buf, sizeof(buf), pdMS_TO_TICKS(20));
        uint32_t t = now_ms();
        for (int i = 0; i < n; i++) {
            if (nmea_feed(&nmea, buf[i], t)) {
                car_lock();
                g_car.gps = nmea.fix;
                if (nmea.fix.last_gga_ms == t) g_car.gps_gga_ms = t;
                car_unlock();
            }
        }
        if (nmea.ok != last_ok_count) {
            last_ok_count = nmea.ok;
            last_ok_ms = t;
            g_car.gps_uart_ok = true;
        } else if (t - last_ok_ms > 3000) {
            // sin sentencias validas: probar el siguiente baudio
            g_car.gps_uart_ok = false;
            bi = (bi + 1) % (int)(sizeof(BAUDS) / sizeof(BAUDS[0]));
            uart_set_baudrate(port, BAUDS[bi]);
            ESP_LOGW(TAG, "sin NMEA, probando %lu baudios", (unsigned long)BAUDS[bi]);
            last_ok_ms = t;
        }
    }
}

void gps_start(void)
{
    if (PIN_GPS_RX < 0) {
        ESP_LOGW(TAG, "pin RX del GPS sin definir: GPS desactivado");
        return;
    }
    const uart_port_t port = (uart_port_t)GPS_UART_NUM;
    uart_config_t cfg = {
        .baud_rate = (int)BAUDS[0],
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(port, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(port, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(port, PIN_GPS_TX < 0 ? UART_PIN_NO_CHANGE : PIN_GPS_TX, PIN_GPS_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    xTaskCreate(gps_task, "gps", 4096, NULL, 5, NULL);
}
