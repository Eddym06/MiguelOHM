#pragma once
#include <stdbool.h>

bool ina219_init(void);
bool ina219_ok(void);
// Voltaje del bus (V) y corriente (A, con signo). false si falla la lectura.
bool ina219_read(float *volts, float *amps);
