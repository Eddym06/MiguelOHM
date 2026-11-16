#include "lidar.h"
#include <string.h>
#include "board_pins.h"
#include "car_state.h"
#include "ld_parser.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lidar";

static ld_parser_t s_ld;

static void lidar_task(void *arg)
{
    const uart_port_t port = (uart_port_t)LIDAR_UART_NUM;
    uint8_t buf[256];
    uint32_t revs = 0;

    for (;;) {
        int n = uart_read_bytes(port, buf, sizeof(buf), pdMS_TO_TICKS(20));
        uint32_t t = now_ms();
        for (int i = 0; i < n; i++) ld_feed(&s_ld, buf[i], t);

        if (s_ld.revolutions != revs) {      // vuelta completa nueva
            revs = s_ld.revolutions;
            car_lock();
            memcpy(g_car.lidar_cm, s_ld.sector_cm, sizeof(g_car.lidar_cm));
            g_car.lidar_ms = t;
            car_unlock();
        }
        g_car.lidar_ok = (s_ld.packets > 0) && (t - s_ld.last_packet_ms < 500);
    }
}

void lidar_start(void)
{
    ld_init(&s_ld);
    if (PIN_LIDAR_RX < 0) {
        ESP_LOGW(TAG, "pin RX del LIDAR sin definir: LIDAR desactivado");
        return;
    }
    const uart_port_t port = (uart_port_t)LIDAR_UART_NUM;
    uart_config_t cfg = {
        .baud_rate = LIDAR_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(port, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(port, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(port, UART_PIN_NO_CHANGE, PIN_LIDAR_RX, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
    xTaskCreate(lidar_task, "lidar", 4096, NULL, 6, NULL);
}
