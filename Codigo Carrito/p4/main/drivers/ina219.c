#include "ina219.h"
#include "board_pins.h"
#include "i2c_bus.h"
#include "esp_log.h"

static const char *TAG = "ina219";

#define REG_CONFIG   0x00
#define REG_BUS      0x02
#define REG_CURRENT  0x04
#define REG_CALIB    0x05

static i2c_master_dev_handle_t s_dev;
static bool s_ok;

static bool write16(uint8_t reg, uint16_t v)
{
    uint8_t d[3] = {reg, (uint8_t)(v >> 8), (uint8_t)(v & 0xFF)};
    return i2c_master_transmit(s_dev, d, 3, 20) == ESP_OK;
}

static bool read16(uint8_t reg, uint16_t *v)
{
    uint8_t b[2];
    if (!i2c_reg_read(s_dev, reg, b, 2)) return false;
    *v = (uint16_t)((b[0] << 8) | b[1]);
    return true;
}

bool ina219_init(void)
{
    s_dev = i2c_bus_add(INA219_ADDR);
    if (!s_dev) return false;

    // BRNG 32 V, PGA /8 (+-320 mV), bus y shunt a 12 bits x 32 muestras (17 ms, filtra el ruido
    // del PWM), modo continuo: 0x2000 | 0x1800 | (0xD << 7) | (0xD << 3) | 0x7
    uint16_t config = 0x3EEF;
    if (!write16(REG_CONFIG, config)) {
        ESP_LOGW(TAG, "INA219 no responde en 0x%02X", INA219_ADDR);
        return false;
    }
    // Current_LSB = 0.1 mA  ->  Cal = 0.04096 / (Current_LSB * Rshunt) = 409600 / mohm
    write16(REG_CALIB, (uint16_t)(409600u / INA219_SHUNT_MOHM));
    s_ok = true;
    ESP_LOGI(TAG, "INA219 listo (shunt %d mohm)", INA219_SHUNT_MOHM);
    return true;
}

bool ina219_ok(void)
{
    return s_ok;
}

bool ina219_read(float *volts, float *amps)
{
    if (!s_ok) return false;
    uint16_t bus, cur;
    if (!read16(REG_BUS, &bus) || !read16(REG_CURRENT, &cur)) return false;
    *volts = (float)(bus >> 3) * 0.004f;         // 4 mV por LSB
    *amps = (float)(int16_t)cur * 0.0001f;       // 0.1 mA por LSB
    return true;
}
