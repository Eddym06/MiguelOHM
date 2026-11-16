#include "compass.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "compass";

typedef enum { MAG_NONE, MAG_IST8310, MAG_QMC5883L, MAG_HMC5883L } mag_type_t;

static mag_type_t s_type = MAG_NONE;
static i2c_master_dev_handle_t s_dev;

bool compass_ok(void)
{
    return s_type != MAG_NONE;
}

const char *compass_name(void)
{
    switch (s_type) {
    case MAG_IST8310: return "IST8310";
    case MAG_QMC5883L: return "QMC5883L";
    case MAG_HMC5883L: return "HMC5883L";
    default: return "ninguno";
    }
}

static bool try_ist8310(uint8_t addr)
{
    i2c_master_dev_handle_t d = i2c_bus_add(addr);
    if (!d) return false;
    uint8_t id = 0;
    if (!i2c_reg_read(d, 0x00, &id, 1) || id != 0x10) {   // WHO_AM_I = 0x10
        i2c_master_bus_rm_device(d);
        return false;
    }
    i2c_reg_write(d, 0x41, 0x24);   // promedio x16 en X/Z y Y
    i2c_reg_write(d, 0x42, 0xC0);   // pulse duration normal
    s_dev = d;
    s_type = MAG_IST8310;
    return true;
}

static bool try_qmc5883l(uint8_t addr)
{
    i2c_master_dev_handle_t d = i2c_bus_add(addr);
    if (!d) return false;
    uint8_t id = 0;
    if (!i2c_reg_read(d, 0x0D, &id, 1) || id != 0xFF) {   // chip id QMC5883L = 0xFF
        i2c_master_bus_rm_device(d);
        return false;
    }
    i2c_reg_write(d, 0x0B, 0x01);   // periodo SET/RESET
    i2c_reg_write(d, 0x09, 0x1D);   // OSR 512, +-8 G, 200 Hz, continuo
    s_dev = d;
    s_type = MAG_QMC5883L;
    return true;
}

static bool try_hmc5883l(uint8_t addr)
{
    i2c_master_dev_handle_t d = i2c_bus_add(addr);
    if (!d) return false;
    uint8_t id[3] = {0};
    if (!i2c_reg_read(d, 0x0A, id, 3) || id[0] != 'H' || id[1] != '4' || id[2] != '3') {
        i2c_master_bus_rm_device(d);
        return false;
    }
    i2c_reg_write(d, 0x00, 0x78);   // 8 muestras, 75 Hz
    i2c_reg_write(d, 0x01, 0x20);   // +-1.3 Ga
    i2c_reg_write(d, 0x02, 0x00);   // continuo
    s_dev = d;
    s_type = MAG_HMC5883L;
    return true;
}

bool compass_init(void)
{
    if (!i2c_bus_ready()) return false;
    if (try_ist8310(0x0E) || try_ist8310(0x0C) || try_qmc5883l(0x0D) || try_hmc5883l(0x1E)) {
        ESP_LOGI(TAG, "brujula detectada: %s", compass_name());
        return true;
    }
    ESP_LOGW(TAG, "no se detecto brujula (IST8310/QMC5883L/HMC5883L). Sin rumbo absoluto.");
    return false;
}

static int16_t le16(const uint8_t *b) { return (int16_t)(b[0] | (b[1] << 8)); }
static int16_t be16(const uint8_t *b) { return (int16_t)((b[0] << 8) | b[1]); }

bool compass_read(float *x, float *y, float *z)
{
    uint8_t b[6];
    switch (s_type) {
    case MAG_IST8310: {
        // modo disparo unico: se pide la medicion y se lee en la siguiente llamada (>= 6 ms despues)
        static bool triggered;
        bool got = false;
        if (triggered) {
            uint8_t st = 0;
            if (i2c_reg_read(s_dev, 0x02, &st, 1) && (st & 0x01) && i2c_reg_read(s_dev, 0x03, b, 6)) {
                *x = le16(&b[0]) * 0.3f;
                *y = le16(&b[2]) * 0.3f;
                *z = le16(&b[4]) * 0.3f;
                got = true;
            }
        }
        i2c_reg_write(s_dev, 0x0A, 0x01);
        triggered = true;
        return got;
    }
    case MAG_QMC5883L: {
        uint8_t st = 0;
        if (!i2c_reg_read(s_dev, 0x06, &st, 1) || !(st & 0x01)) return false;
        if (!i2c_reg_read(s_dev, 0x00, b, 6)) return false;
        *x = le16(&b[0]) / 30.0f;     // 3000 LSB/G = 30 LSB/uT
        *y = le16(&b[2]) / 30.0f;
        *z = le16(&b[4]) / 30.0f;
        return true;
    }
    case MAG_HMC5883L: {
        uint8_t st = 0;
        if (!i2c_reg_read(s_dev, 0x09, &st, 1) || !(st & 0x01)) return false;
        if (!i2c_reg_read(s_dev, 0x03, b, 6)) return false;
        *x = be16(&b[0]) / 10.9f;     // 1090 LSB/G = 10.9 LSB/uT; orden de registros X, Z, Y
        *z = be16(&b[2]) / 10.9f;
        *y = be16(&b[4]) / 10.9f;
        return true;
    }
    default:
        return false;
    }
}
