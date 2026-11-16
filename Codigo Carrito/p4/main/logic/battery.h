#pragma once
#include <stdint.h>

// Porcentaje de una bateria Li-ion/LiPo de `cells` celdas en serie (curva aproximada por celda).
uint8_t battery_percent_li(float pack_volts, int cells);
