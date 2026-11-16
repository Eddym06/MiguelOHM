#include "spi_slave_link.h"
#include <string.h>
#include "bridge.h"
#include "s3_config.h"
#include "spi_link.h"
#include "driver/gpio.h"
#include "driver/spi_slave.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "spi_slave";

#define HOST SPI2_HOST

// "datos listos" hacia el P4: sube cuando la transaccion esta armada, baja al terminar
static void IRAM_ATTR on_setup(spi_slave_transaction_t *t)
{
    if (PIN_SPI_HS >= 0) gpio_set_level(PIN_SPI_HS, 1);
}

static void IRAM_ATTR on_done(spi_slave_transaction_t *t)
{
    if (PIN_SPI_HS >= 0) gpio_set_level(PIN_SPI_HS, 0);
}

static void spi_task(void *arg)
{
    uint8_t *tx = heap_caps_calloc(1, SPI_LINK_FRAME_SIZE, MALLOC_CAP_DMA);
    uint8_t *rx = heap_caps_calloc(1, SPI_LINK_FRAME_SIZE, MALLOC_CAP_DMA);
    if (!tx || !rx) {
        ESP_LOGE(TAG, "sin memoria para los buffers SPI");
        vTaskDelete(NULL);
        return;
    }
    uint32_t bad = 0;
    for (;;) {
        s3_to_p4_t cmd;
        bridge_fill_cmd(&cmd);
        spi_link_seal(tx, &cmd, sizeof(cmd));
        memset(rx, 0, SPI_LINK_FRAME_SIZE);

        spi_slave_transaction_t t = {
            .length = SPI_LINK_FRAME_SIZE * 8,
            .tx_buffer = tx,
            .rx_buffer = rx,
        };
        // bloquea hasta que el P4 hace su transaccion
        if (spi_slave_transmit(HOST, &t, portMAX_DELAY) != ESP_OK) continue;

        p4_to_s3_t st;
        if (spi_link_open(rx, &st, sizeof(st))) {
            bridge_set_status(&st);
        } else if ((++bad % 100) == 1) {
            ESP_LOGW(TAG, "tramas del P4 invalidas: %lu", (unsigned long)bad);
        }
    }
}

void spi_slave_link_start(void)
{
    if (PIN_SPI_SCLK < 0 || PIN_SPI_MOSI < 0 || PIN_SPI_MISO < 0 || PIN_SPI_CS < 0) {
        ESP_LOGW(TAG, "pines SPI sin definir: sin enlace con el P4 (la web mostrara 'sin datos')");
        return;
    }
    if (PIN_SPI_HS >= 0) {
        gpio_config_t io = {.pin_bit_mask = PIN_BIT(PIN_SPI_HS), .mode = GPIO_MODE_OUTPUT};
        ESP_ERROR_CHECK(gpio_config(&io));
        gpio_set_level(PIN_SPI_HS, 0);
    }
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SPI_LINK_FRAME_SIZE,
    };
    spi_slave_interface_config_t slv = {
        .mode = 0,
        .spics_io_num = PIN_SPI_CS,
        .queue_size = 2,
        .flags = 0,
        .post_setup_cb = on_setup,
        .post_trans_cb = on_done,
    };
    ESP_ERROR_CHECK(spi_slave_initialize(HOST, &bus, &slv, SPI_DMA_CH_AUTO));
    xTaskCreate(spi_task, "spi_slave", 4096, NULL, 10, NULL);
    ESP_LOGI(TAG, "esclavo SPI listo");
}
