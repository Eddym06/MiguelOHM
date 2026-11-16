#pragma once
#include <stdbool.h>

// GPS M100 Pro por UART (NMEA). Si no llegan sentencias se prueban varios baudios.
void gps_start(void);
