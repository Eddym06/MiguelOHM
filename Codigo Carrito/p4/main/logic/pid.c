#include "pid.h"

void pid_init(pid_ctl_t *p, float kp, float ki, float kd, float out_min, float out_max)
{
    p->kp = kp;
    p->ki = ki;
    p->kd = kd;
    p->out_min = out_min;
    p->out_max = out_max;
    pid_reset(p);
}

void pid_set_gains(pid_ctl_t *p, float kp, float ki, float kd)
{
    p->kp = kp;
    p->ki = ki;
    p->kd = kd;
}

void pid_reset(pid_ctl_t *p)
{
    p->integ = 0;
    p->prev_meas = 0;
    p->primed = false;
}

float pid_step(pid_ctl_t *p, float setpoint, float measurement, float ff, float dt)
{
    float err = setpoint - measurement;
    float d = 0;
    if (p->primed && dt > 0) d = -(measurement - p->prev_meas) / dt;
    p->prev_meas = measurement;
    p->primed = true;

    float integ_new = p->integ + p->ki * err * dt;
    float out = ff + p->kp * err + integ_new + p->kd * d;

    // anti-windup: si se satura empujando mas en la misma direccion, no integrar
    if (out > p->out_max) {
        out = p->out_max;
        if (err > 0) integ_new = p->integ;
    } else if (out < p->out_min) {
        out = p->out_min;
        if (err < 0) integ_new = p->integ;
    }
    p->integ = integ_new;
    return out;
}
