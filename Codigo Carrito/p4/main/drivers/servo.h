#pragma once
#include <stdbool.h>

void servo_init(void);
bool servo_present(void);
// Angulo en grados respecto al centro (+ = derecha). Usa centro/us por grado/limites de los ajustes.
void servo_set_angle(float deg);
