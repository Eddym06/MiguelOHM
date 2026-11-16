#pragma once
#include <stdbool.h>

// Magnetometro del GPS M100 Pro. Se autodetecta el chip en el bus I2C:
// IST8310 (0x0E/0x0C), QMC5883L (0x0D) o HMC5883L (0x1E).
bool compass_init(void);
bool compass_ok(void);
const char *compass_name(void);
// Campo magnetico en uT (ejes del sensor). false si no hay dato nuevo o falla.
bool compass_read(float *x, float *y, float *z);
