#include "i2c_bus.h"
#include "board_pins.h"
#include "esp_log.h"

static const char *TAG = "i2c";
static i2c_master_bus_handle_t s_bus;

bool i2c_bus_init(void)
{
    if (PIN_I2C_SDA < 0 || PIN_I2C_SCL < 0) {
        ESP_LOGW(TAG, "pines I2C sin definir: BMI160, INA219 y brujula desactivados");
        return false;
    }
    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus: %s", esp_err_to_name(err));
        s_bus = NULL;
        return false;
    }
    return true;
}

bool i2c_bus_ready(void)
{
    return s_bus != NULL;
}

i2c_master_dev_handle_t i2c_bus_add(uint8_t addr)
{
    if (!s_bus) return NULL;
    i2c_device_config_t dev = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t h = NULL;
    if (i2c_master_bus_add_device(s_bus, &dev, &h) != ESP_OK) return NULL;
    return h;
}

bool i2c_bus_probe(uint8_t addr)
{
    if (!s_bus) return false;
    return i2c_master_probe(s_bus, addr, 20) == ESP_OK;
}

bool i2c_reg_read(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *buf, size_t n)
{
    return i2c_master_transmit_receive(dev, &reg, 1, buf, n, 20) == ESP_OK;
}

bool i2c_reg_write(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t d[2] = {reg, val};
    return i2c_master_transmit(dev, d, 2, 20) == ESP_OK;
}
