#include "drive_mix.h"
#include <math.h>

void drive_mix(float throttle_pct, float steer_pct, const mix_cfg_t *c,
               float *tgt_l_rpm, float *tgt_r_rpm)
{
    float v = throttle_pct / 100.0f * c->rpm_max;
    float d = steer_pct / 100.0f * c->diff_gain;
    float l = v * (1.0f + d);
    float r = v * (1.0f - d);

    float m = fmaxf(fabsf(l), fabsf(r));
    if (m > c->rpm_max) {
        float s = c->rpm_max / m;
        l *= s;
        r *= s;
    }
    *tgt_l_rpm = l;
    *tgt_r_rpm = r;
}

float ramp_toward(float cur, float tgt, float max_delta)
{
    float diff = tgt - cur;
    if (diff > max_delta) return cur + max_delta;
    if (diff < -max_delta) return cur - max_delta;
    return tgt;
}

void sync_adjust(float meas_l, float meas_r, float *sp_l, float *sp_r, float kp, float rpm_max)
{
    if (rpm_max <= 0) return;
    float err = (meas_l - meas_r) - (*sp_l - *sp_r);  // rpm
    float corr = 0.5f * kp * err;
    // limite para que la sincronia nunca domine al setpoint
    float lim = 0.25f * rpm_max;
    if (corr > lim) corr = lim;
    if (corr < -lim) corr = -lim;
    *sp_l -= corr;
    *sp_r += corr;
}

float duty_deadband(float duty, float min_duty)
{
    float a = fabsf(duty);
    if (a < 0.005f) return 0;
    if (a > 1.0f) a = 1.0f;
    float out = min_duty + a * (1.0f - min_duty);
    return duty < 0 ? -out : out;
}
