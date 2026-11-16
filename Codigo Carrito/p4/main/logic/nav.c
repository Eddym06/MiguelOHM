#include "nav.h"
#include "heading.h"
#include "spi_link.h"
#include <math.h>
#include <string.h>

#define PI_D  3.14159265358979323846
#define D2R_D (PI_D / 180.0)

void nav_geo(double lat1, double lon1, double lat2, double lon2, float *dist_m, float *bearing_deg)
{
    double p1 = lat1 * D2R_D, p2 = lat2 * D2R_D;
    double dp = p2 - p1, dl = (lon2 - lon1) * D2R_D;
    double a = sin(dp / 2) * sin(dp / 2) + cos(p1) * cos(p2) * sin(dl / 2) * sin(dl / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    *dist_m = (float)(6371000.0 * c);
    double y = sin(dl) * cos(p2);
    double x = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl);
    *bearing_deg = wrap360((float)(atan2(y, x) / D2R_D));
}

void nav_init(nav_t *n)
{
    memset(n, 0, sizeof(*n));
    n->state = NAVS_IDLE;
}

void nav_set_target(nav_t *n, double lat, double lon)
{
    memset(n, 0, sizeof(*n));
    n->has_wp = true;
    n->wp_lat = lat;
    n->wp_lon = lon;
    n->state = NAVS_RUNNING;
}

void nav_cancel(nav_t *n)
{
    memset(n, 0, sizeof(*n));
    n->state = NAVS_IDLE;
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void nav_step(nav_t *n, const nav_cfg_t *c, const nav_in_t *in, float dt_s, nav_out_t *out)
{
    out->throttle = 0;
    out->steer = 0;
    out->dist_m = 0;
    out->bearing_deg = 0;
    out->state = n->state;

    if (!n->has_wp || n->state != NAVS_RUNNING) return;

    if (!in->gps_valid || !in->heading_valid) {
        n->no_gps_s += dt_s;
        if (n->no_gps_s > 5.0f) n->state = NAVS_NO_GPS;
        out->state = n->state;
        return;
    }
    n->no_gps_s = 0;

    float dist, bearing;
    nav_geo(in->lat, in->lon, n->wp_lat, n->wp_lon, &dist, &bearing);
    out->dist_m = dist;
    out->bearing_deg = bearing;

    if (dist < c->arrive_m) {
        n->state = NAVS_ARRIVED;
        out->state = n->state;
        return;
    }

    if (n->cooldown_s > 0) n->cooldown_s -= dt_s;

    // maniobra de reversa en curso
    if (n->reverse_ms_left > 0) {
        n->reverse_ms_left -= dt_s * 1000.0f;
        out->throttle = -30;
        out->steer = n->reverse_steer;
        out->state = NAVS_AVOID;
        if (n->reverse_ms_left <= 0) n->cooldown_s = 0.8f;
        return;
    }

    float err = wrap180(bearing - in->heading_deg);   // + = el punto queda a la derecha
    float steer = clampf(err * c->steer_kp * 2.5f, -100.0f, 100.0f);
    float speed = c->speed_pct * (1.0f - 0.6f * fminf(1.0f, fabsf(err) / 90.0f));
    if (dist < 2.0f * c->arrive_m) speed *= 0.6f;
    uint8_t state = NAVS_RUNNING;

    if (in->lidar_valid && in->lidar_cm) {
        float front_min = 1e9f, repulse = 0, left_clear = 0, right_clear = 0;
        for (int s = 0; s < LIDAR_SECTORS; s++) {
            float cm = in->lidar_cm[s] == LIDAR_NO_DATA ? 1000.0f : (float)in->lidar_cm[s];
            float a = (float)(s * 10 + 5);
            if (c->lidar_invert) a = -a;
            a = wrap180(a + c->lidar_yaw_deg);        // + = derecha del carrito
            float aa = fabsf(a);
            if (aa <= 25.0f && cm < front_min) front_min = cm;
            if (aa <= 70.0f && cm < c->avoid_cm) {
                float w = (c->avoid_cm - cm) / c->avoid_cm * (1.0f - aa / 90.0f);
                repulse += (a >= 0 ? 1.0f : -1.0f) * w;
            }
            if (a > 30.0f && a <= 100.0f) right_clear += fminf(cm, 300.0f);
            if (a < -30.0f && a >= -100.0f) left_clear += fminf(cm, 300.0f);
        }

        steer = clampf(steer - repulse * 60.0f, -100.0f, 100.0f);

        if (front_min < c->avoid_cm) {
            state = NAVS_AVOID;
            n->clear_s = 0;
            float k = (front_min - c->stop_cm) / (c->avoid_cm - c->stop_cm);
            speed *= clampf(k, 0.25f, 1.0f);
        } else {
            n->clear_s += dt_s;
            if (n->clear_s > 3.0f) n->reverse_count = 0;
        }

        if (front_min < c->stop_cm && n->cooldown_s <= 0) {
            n->reverse_count++;
            if (n->reverse_count > 6) {
                n->state = NAVS_BLOCKED;
                out->state = n->state;
                return;
            }
            float side = (right_clear >= left_clear) ? 1.0f : -1.0f;
            // en reversa, girar el servo al lado contrario hace que la nariz apunte hacia `side`
            n->reverse_steer = -side * 80.0f;
            n->reverse_ms_left = 1100.0f;
            out->throttle = -30;
            out->steer = n->reverse_steer;
            out->state = NAVS_AVOID;
            return;
        }
    }

    out->throttle = speed;
    out->steer = steer;
    out->state = state;
}
