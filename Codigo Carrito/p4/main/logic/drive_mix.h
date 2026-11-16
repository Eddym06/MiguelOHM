#pragma once

typedef struct {
    float rpm_max;     // rpm de rueda a acelerador 100 %
    float diff_gain;   // fraccion de diferencia de velocidad con el giro al maximo (0..1)
} mix_cfg_t;

// Acelerador y giro (-100..100) -> rpm objetivo de cada rueda.
// Giro a la derecha (+): la rueda izquierda va mas rapida. Funciona igual en reversa.
// Si alguna rueda excede rpm_max se escalan ambas para conservar la proporcion.
void drive_mix(float throttle_pct, float steer_pct, const mix_cfg_t *c,
               float *tgt_l_rpm, float *tgt_r_rpm);

// Mueve cur hacia tgt como maximo max_delta.
float ramp_toward(float cur, float tgt, float max_delta);

// Sincronia cruzada: si la diferencia medida entre ruedas (izq - der) se aleja de la
// diferencia pedida, reparte la correccion entre ambos setpoints. Trabaja en rpm con signo.
void sync_adjust(float meas_l, float meas_r, float *sp_l, float *sp_r, float kp, float rpm_max);

// Compensa la zona muerta del L298N/motor: mapea |duty| (0,1] a [min_duty, 1].
float duty_deadband(float duty, float min_duty);
