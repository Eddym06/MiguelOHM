#pragma once
#include <stdbool.h>

// PID con avance (feed-forward), derivada sobre la medicion y anti-windup por saturacion.
typedef struct {
    float kp, ki, kd;
    float integ;
    float prev_meas;
    float out_min, out_max;
    bool  primed;
} pid_ctl_t;

void  pid_init(pid_ctl_t *p, float kp, float ki, float kd, float out_min, float out_max);
void  pid_set_gains(pid_ctl_t *p, float kp, float ki, float kd);
void  pid_reset(pid_ctl_t *p);
// out = ff + kp*e + integral + kd*d(-meas)/dt, limitado a [out_min, out_max]
float pid_step(pid_ctl_t *p, float setpoint, float measurement, float ff, float dt);
