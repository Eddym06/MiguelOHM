#include "rc433.h"
#include "board_pins.h"
#include "car_state.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "rc433";

static rc433_cmd_cb_t s_cb;
static volatile bool s_enabled = true;
static volatile uint32_t s_tx_until_ms;
static volatile uint32_t s_ok, s_bad;
static bool s_started;

static uart_port_t port(void) { return (uart_port_t)RF_UART_NUM; }

static void apply_en(bool on)
{
    if (PIN_RF_EN < 0) return;
    int level = (RF_EN_ACTIVE_HIGH ? on : !on) ? 1 : 0;
    gpio_set_level(PIN_RF_EN, level);
}

void rc433_set_enabled(bool enabled)
{
    s_enabled = enabled;
    if (s_started) {
        apply_en(enabled);
        uart_flush_input(port());
    }
}

bool rc433_enabled(void)
{
    return s_enabled;
}

uint32_t rc433_rx_ok(void) { return s_ok; }
uint32_t rc433_rx_bad(void) { return s_bad; }

void rc433_send_telem(const rc433_telem_t *t)
{
    if (!s_started || !s_enabled) return;
    uint8_t wire[RC433_WIRE_LEN];
    rc433_build(RC433_TYPE_TELEM, t->speed_x10, t->rpm_div4, t->batt_pct, wire);
    // ventana en la que se ignora lo que capte el receptor propio
    s_tx_until_ms = now_ms() + RC433_AIRTIME_MS + 4;
    uart_write_bytes(port(), wire, RC433_WIRE_LEN);
}

static void rf_task(void *arg)
{
    rc433_parser_t parser;
    rc433_parser_init(&parser);
    uint8_t buf[64];
    uint8_t raw[RC433_FRAME_LEN];

    for (;;) {
        int n = uart_read_bytes(port(), buf, sizeof(buf), pdMS_TO_TICKS(5));
        if (n <= 0) continue;
        uint32_t t = now_ms();
        if (!s_enabled || (int32_t)(s_tx_until_ms - t) > 0) {
            rc433_parser_init(&parser);   // transmitiendo o radio apagada: descartar
            continue;
        }
        for (int i = 0; i < n; i++) {
            rc433_result_t r = rc433_parser_feed(&parser, buf[i], t, raw);
            if (r == RC433_RES_CMD) {
                s_ok++;
                if (s_cb) {
                    rc433_cmd_t c = {(int8_t)raw[1], (int8_t)raw[2], raw[3]};
                    s_cb(&c);
                }
            } else if (r == RC433_RES_BAD) {
                s_bad++;
            }
        }
    }
}

void rc433_start(rc433_cmd_cb_t on_cmd)
{
    s_cb = on_cmd;
    if (PIN_RF_TX < 0 || PIN_RF_RX < 0) {
        ESP_LOGW(TAG, "pines del 433 sin definir: radio desactivada");
        return;
    }
    if (PIN_RF_EN >= 0) {
        gpio_config_t io = {.pin_bit_mask = PIN_BIT(PIN_RF_EN), .mode = GPIO_MODE_OUTPUT};
        ESP_ERROR_CHECK(gpio_config(&io));
    }
    uart_config_t cfg = {
        .baud_rate = RC433_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(port(), 512, 256, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(port(), &cfg));
    ESP_ERROR_CHECK(uart_set_pin(port(), PIN_RF_TX, PIN_RF_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    // linea invertida: reposo = LOW = sin portadora; el DO del receptor llega en logica positiva
    ESP_ERROR_CHECK(uart_set_line_inverse(port(), UART_SIGNAL_TXD_INV | UART_SIGNAL_RXD_INV));
    s_started = true;
    apply_en(s_enabled);
    xTaskCreate(rf_task, "rf433", 4096, NULL, 9, NULL);
    ESP_LOGI(TAG, "radio 433 lista (%d baudios, linea invertida)", RC433_BAUD);
}
