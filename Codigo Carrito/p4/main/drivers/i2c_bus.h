#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "driver/i2c_master.h"

// Bus I2C compartido (BMI160, INA219, brujula). Devuelve false si los pines no estan definidos.
bool i2c_bus_init(void);
bool i2c_bus_ready(void);
// Agrega un dispositivo (400 kHz). NULL si falla.
i2c_master_dev_handle_t i2c_bus_add(uint8_t addr);
bool i2c_bus_probe(uint8_t addr);

bool i2c_reg_read(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *buf, size_t n);
bool i2c_reg_write(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val);
