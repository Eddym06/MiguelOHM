#include "bmi160.h"
#include "board_pins.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bmi160";

#define REG_CHIP_ID   0x00
#define REG_DATA      0x0C   // gyro X,Y,Z luego accel X,Y,Z (int16 little-endian)
#define REG_ACC_CONF  0x40
#define REG_ACC_RANGE 0x41
#define REG_GYR_CONF  0x42
#define REG_GYR_RANGE 0x43
#define REG_CMD       0x7E

#define GYRO_LSB_DPS  65.6f      // +-500 dps
#define ACCEL_LSB_G   8192.0f    // +-4 g

static i2c_master_dev_handle_t s_dev;
static bool s_ok;
static float s_bias[3];

bool bmi160_init(void)
{
    s_dev = i2c_bus_add(BMI160_ADDR);
    if (!s_dev) return false;

    uint8_t id = 0;
    if (!i2c_reg_read(s_dev, REG_CHIP_ID, &id, 1) || id != 0xD1) {
        ESP_LOGW(TAG, "BMI160 no responde (id=0x%02X, esperado 0xD1)", id);
        return false;
    }
    i2c_reg_write(s_dev, REG_CMD, 0xB6);           // soft reset
    vTaskDelay(pdMS_TO_TICKS(100));
    i2c_reg_write(s_dev, REG_CMD, 0x11);           // acelerometro normal
    vTaskDelay(pdMS_TO_TICKS(10));
    i2c_reg_write(s_dev, REG_CMD, 0x15);           // giroscopio normal
    vTaskDelay(pdMS_TO_TICKS(100));
    i2c_reg_write(s_dev, REG_ACC_CONF, 0x28);      // 100 Hz, filtro normal
    i2c_reg_write(s_dev, REG_ACC_RANGE, 0x05);     // +-4 g
    i2c_reg_write(s_dev, REG_GYR_CONF, 0x28);      // 100 Hz, filtro normal
    i2c_reg_write(s_dev, REG_GYR_RANGE, 0x02);     // +-500 dps
    s_ok = true;
    ESP_LOGI(TAG, "BMI160 listo");
    bmi160_calibrate_gyro();
    return true;
}

bool bmi160_ok(void)
{
    return s_ok;
}

bool bmi160_read(imu_sample_t *s)
{
    if (!s_ok) return false;
    uint8_t b[12];
    if (!i2c_reg_read(s_dev, REG_DATA, b, sizeof(b))) return false;
    int16_t v[6];
    for (int i = 0; i < 6; i++) v[i] = (int16_t)(b[2 * i] | (b[2 * i + 1] << 8));
    s->gx = v[0] / GYRO_LSB_DPS - s_bias[0];
    s->gy = v[1] / GYRO_LSB_DPS - s_bias[1];
    s->gz = v[2] / GYRO_LSB_DPS - s_bias[2];
    s->ax = v[3] / ACCEL_LSB_G;
    s->ay = v[4] / ACCEL_LSB_G;
    s->az = v[5] / ACCEL_LSB_G;
    return true;
}

void bmi160_calibrate_gyro(void)
{
    if (!s_ok) return;
    float acc[3] = {0, 0, 0};
    int n = 0;
    s_bias[0] = s_bias[1] = s_bias[2] = 0;
    for (int i = 0; i < 100; i++) {
        imu_sample_t s;
        if (bmi160_read(&s)) {
            acc[0] += s.gx;
            acc[1] += s.gy;
            acc[2] += s.gz;
            n++;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (n > 20) {
        for (int i = 0; i < 3; i++) s_bias[i] = acc[i] / n;
        ESP_LOGI(TAG, "sesgo giroscopio: %.2f %.2f %.2f dps", s_bias[0], s_bias[1], s_bias[2]);
    }
}
