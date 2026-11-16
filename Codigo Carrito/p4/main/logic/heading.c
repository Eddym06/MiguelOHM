#include "heading.h"
#include <math.h>

#define PI_F   3.14159265f
#define D2R    (PI_F / 180.0f)
#define R2D    (180.0f / PI_F)

float wrap360(float deg)
{
    deg = fmodf(deg, 360.0f);
    if (deg < 0) deg += 360.0f;
    return deg;
}

float wrap180(float deg)
{
    deg = wrap360(deg);
    if (deg > 180.0f) deg -= 360.0f;
    return deg;
}

void heading_init(heading_t *h, float mag_gain)
{
    h->yaw_deg = 0;
    h->init = false;
    h->mag_gain = mag_gain;
}

void heading_gyro(heading_t *h, float yaw_rate_cw_dps, float dt)
{
    h->yaw_deg = wrap360(h->yaw_deg + yaw_rate_cw_dps * dt);
}

void heading_mag(heading_t *h, float mag_heading_deg)
{
    if (!h->init) {
        h->yaw_deg = wrap360(mag_heading_deg);
        h->init = true;
        return;
    }
    float err = wrap180(mag_heading_deg - h->yaw_deg);
    h->yaw_deg = wrap360(h->yaw_deg + h->mag_gain * err);
}

float compass_heading(float mx, float my, float mz, float roll_deg, float pitch_deg,
                      float off_x, float off_y, float decl_deg, float rot_deg, bool invert)
{
    mx -= off_x;
    my -= off_y;
    float cr = cosf(roll_deg * D2R), sr = sinf(roll_deg * D2R);
    float cp = cosf(pitch_deg * D2R), sp = sinf(pitch_deg * D2R);
    float xh = mx * cp + my * sr * sp + mz * cr * sp;
    float yh = my * cr - mz * sr;
    float h = atan2f(-yh, xh) * R2D;
    if (invert) h = -h;
    return wrap360(h + rot_deg + decl_deg);
}
