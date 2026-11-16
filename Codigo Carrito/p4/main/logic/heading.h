#pragma once
#include <stdbool.h>

// Rumbo del carrito: giroscopio (suave, rapido) corregido por la brujula (absoluta).
// Convencion: grados 0..360, 0 = norte, sentido horario.

float wrap360(float deg);
float wrap180(float deg);

typedef struct {
    float yaw_deg;
    bool  init;
    float mag_gain;   // 0..1, correccion por muestra de brujula
} heading_t;

void  heading_init(heading_t *h, float mag_gain);
// yaw_rate_cw_dps: velocidad de giro, positiva en sentido horario
void  heading_gyro(heading_t *h, float yaw_rate_cw_dps, float dt);
void  heading_mag(heading_t *h, float mag_heading_deg);

// Rumbo magnetico con compensacion de inclinacion (ejes X adelante, Y derecha, Z abajo).
// off_x/off_y: offsets hard-iron. rot_deg: giro de montaje. decl_deg: declinacion magnetica.
float compass_heading(float mx, float my, float mz, float roll_deg, float pitch_deg,
                      float off_x, float off_y, float decl_deg, float rot_deg, bool invert);
