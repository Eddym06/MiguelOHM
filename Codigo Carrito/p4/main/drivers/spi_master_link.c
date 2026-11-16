#include "spi_master_link.h"
#include <math.h>
#include <string.h>
#include "board_pins.h"
#include "car_state.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "spi_link";

static s3_to_p4_t s_cmd;
static uint32_t s_cmd_ms;
static bool s_cmd_valid;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

bool spi_link_master_get(s3_to_p4_t *out, uint32_t *age_ms)
{
    bool ok;
    uint32_t t;
    portENTER_CRITICAL(&s_mux);
    ok = s_cmd_valid;
    if (ok) *out = s_cmd;
    t = s_cmd_ms;
    portEXIT_CRITICAL(&s_mux);
    if (ok && age_ms) *age_ms = now_ms() - t;
    return ok;
}

static uint16_t clamp_u16(float v)
{
    if (v < 0) return 0;
    if (v > 65535.0f) return 65535;
    return (uint16_t)v;
}

static int16_t clamp_i16(float v)
{
    if (v < -32768.0f) return -32768;
    if (v > 32767.0f) return 32767;
    return (int16_t)v;
}

static void build_status(p4_to_s3_t *m, uint8_t seq)
{
    uint32_t now = now_ms();
    memset(m, 0, sizeof(*m));
    m->magic = SPI_LINK_MAGIC;
    m->seq = seq;
    m->flags = g_car.wifi_allowed ? P4F_WIFI_ALLOWED : 0;
    m->link_mode = g_car.link_mode;
    m->nav_state = g_car.nav_state;

    car_lock();
    gps_fix_t gps = g_car.gps;
    uint32_t gga_ms = g_car.gps_gga_ms;
    memcpy(m->lidar_cm, g_car.lidar_cm, sizeof(m->lidar_cm));
    car_unlock();

    bool fix = gps.valid && (now - gga_ms) < 2000;
    m->sensor_ok = (fix ? SENS_GPS_FIX : 0) | (g_car.lidar_ok ? SENS_LIDAR : 0) |
                   (g_car.imu_ok ? SENS_IMU : 0) | (g_car.ina_ok ? SENS_INA : 0) |
                   (g_car.compass_ok ? SENS_COMPASS : 0) | (g_car.gps_uart_ok ? SENS_GPS_UART : 0);
    m->drive_flags = (g_car.headlights ? DRV_HEADLIGHTS : 0) | (g_car.rgb_on ? DRV_RGB_ON : 0) |
                     (g_car.nav_state != NAVS_IDLE ? DRV_AUTO : 0) | (g_car.brake ? DRV_BRAKE : 0) |
                     (uint8_t)(g_car.rgb_idx << DRV_COLOR_SHIFT);
    m->throttle = g_car.thr_applied;
    m->steer = g_car.steer_applied;
    m->batt_pct = g_car.ina_ok ? g_car.batt_pct : 0xFF;
    m->sats = gps.sats;
    m->vbat_mv = clamp_u16(g_car.vbat_v * 1000.0f);
    m->current_ma = clamp_i16(g_car.current_a * 1000.0f);
    m->rpm_l = clamp_i16(g_car.rpm_l);
    m->rpm_r = clamp_i16(g_car.rpm_r);
    m->speed_x100 = clamp_u16(g_car.speed_kmh * 100.0f);
    m->heading_d10 = clamp_u16(g_car.heading_deg * 10.0f);
    m->pitch_d10 = clamp_i16(g_car.pitch_deg * 10.0f);
    m->roll_d10 = clamp_i16(g_car.roll_deg * 10.0f);
    m->servo_d10 = clamp_i16(g_car.servo_deg * 10.0f);
    m->hdop_x100 = clamp_u16(gps.hdop * 100.0f);
    m->lat_e7 = (int32_t)llround(gps.lat * 1e7);
    m->lon_e7 = (int32_t)llround(gps.lon * 1e7);
    if (g_car.wp_valid) {
        m->wp_lat_e7 = (int32_t)llround(g_car.wp_lat * 1e7);
        m->wp_lon_e7 = (int32_t)llround(g_car.wp_lon * 1e7);
        m->wp_dist_dm = clamp_u16(g_car.wp_dist_m * 10.0f);
        m->wp_bearing_d10 = clamp_u16(g_car.wp_bearing_deg * 10.0f);
    }
    m->gps_speed_x100 = clamp_u16(gps.speed_kmh * 100.0f);
    m->gps_course_d10 = clamp_u16(gps.course_deg * 10.0f);
    m->cfg_ack = g_car.cfg_ack;
    m->cfg_key = g_car.cfg_key;
    m->cfg_val = g_car.cfg_val;
    m->test_id = g_car.test_id;
    m->gps_fix = gps.quality;
}

static void spi_task(void *arg)
{
    spi_device_handle_t dev = (spi_device_handle_t)arg;
    // buffers alineados a 64 bytes para DMA con cache (192 = 3 x 64)
    uint8_t *tx = heap_caps_aligned_calloc(64, 1, SPI_LINK_FRAME_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    uint8_t *rx = heap_caps_aligned_calloc(64, 1, SPI_LINK_FRAME_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!tx || !rx) {
        ESP_LOGE(TAG, "sin memoria para los buffers SPI");
        vTaskDelete(NULL);
        return;
    }
    uint8_t seq = 0;
    uint32_t bad = 0;
    TickType_t wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(SPI_LINK_PERIOD_MS));

        // espera a que el S3 tenga su trama lista
        if (PIN_SPI_HS >= 0) {
            int tries = 0;
            while (gpio_get_level(PIN_SPI_HS) == 0 && tries < 50) {
                esp_rom_delay_us(100);
                tries++;
            }
            if (tries >= 50) continue;
        }

        p4_to_s3_t st;
        build_status(&st, seq++);
        spi_link_seal(tx, &st, sizeof(st));
        spi_transaction_t t = {
            .length = SPI_LINK_FRAME_SIZE * 8,
            .tx_buffer = tx,
            .rx_buffer = rx,
        };
        if (spi_device_transmit(dev, &t) != ESP_OK) continue;

        s3_to_p4_t cmd;
        if (spi_link_open(rx, &cmd, sizeof(cmd))) {
            portENTER_CRITICAL(&s_mux);
            s_cmd = cmd;
            s_cmd_ms = now_ms();
            s_cmd_valid = true;
            portEXIT_CRITICAL(&s_mux);
        } else if ((++bad % 100) == 1) {
            ESP_LOGW(TAG, "tramas del S3 invalidas: %lu", (unsigned long)bad);
        }
    }
}

void spi_link_master_start(void)
{
    if (PIN_SPI_SCLK < 0 || PIN_SPI_MOSI < 0 || PIN_SPI_MISO < 0 || PIN_SPI_CS < 0) {
        ESP_LOGW(TAG, "pines SPI sin definir: sin enlace con el S3 (sin pagina web)");
        return;
    }
    if (PIN_SPI_HS >= 0) {
        gpio_config_t io = {.pin_bit_mask = PIN_BIT(PIN_SPI_HS), .mode = GPIO_MODE_INPUT,
                            .pull_down_en = GPIO_PULLDOWN_ENABLE};
        ESP_ERROR_CHECK(gpio_config(&io));
    }
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SPI_LINK_FRAME_SIZE,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = SPI_LINK_HZ,
        .mode = 0,
        .spics_io_num = PIN_SPI_CS,
        .queue_size = 2,
    };
    spi_device_handle_t dev;
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &dev));
    xTaskCreate(spi_task, "spi_link", 6144, dev, 8, NULL);
    ESP_LOGI(TAG, "enlace SPI con el S3 a %d Hz", SPI_LINK_HZ);
}
