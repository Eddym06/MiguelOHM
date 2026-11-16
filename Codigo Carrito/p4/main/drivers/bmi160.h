#pragma once
#include <stdbool.h>

typedef struct {
    float ax, ay, az;   // g
    float gx, gy, gz;   // grados/s (sin sesgo)
} imu_sample_t;

bool bmi160_init(void);
bool bmi160_ok(void);
// Lee acelerometro y giroscopio. Devuelve false si falla el bus.
bool bmi160_read(imu_sample_t *s);
// Mide el sesgo del giroscopio (el carrito debe estar quieto). Bloquea ~1 s.
void bmi160_calibrate_gyro(void);
