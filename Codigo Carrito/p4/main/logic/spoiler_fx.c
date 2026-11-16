#include "spoiler_fx.h"
#include <math.h>

static rgb8_t scale(uint8_t r, uint8_t g, uint8_t b, float k)
{
    if (k < 0) k = 0;
    if (k > 1) k = 1;
    rgb8_t c = {(uint8_t)(r * k), (uint8_t)(g * k), (uint8_t)(b * k)};
    return c;
}

void spoiler_fx_render(rgb8_t *leds, int n, float throttle_pct, bool brake, float brightness)
{
    if (n <= 0) return;

    if (brake) {
        for (int i = 0; i < n; i++) leds[i] = scale(255, 0, 0, brightness);
        return;
    }

    float t = fabsf(throttle_pct) / 100.0f;
    if (t < 0.05f) {
        for (int i = 0; i < n; i++) leds[i] = scale(255, 0, 0, brightness * 0.06f);
        return;
    }

    int half = (n + 1) / 2;          // grupos de afuera hacia el centro
    float lit = t * (float)half;     // cantidad de grupos encendidos (fraccional)
    for (int i = 0; i < n; i++) {
        int e = i < (n - 1 - i) ? i : (n - 1 - i);    // 0 en los extremos
        if ((float)e >= lit) {
            leds[i] = scale(0, 0, 0, 0);
            continue;
        }
        float f = (float)e / (float)half;
        if (throttle_pct < 0) {
            leds[i] = scale(255, 255, 255, brightness * 0.8f);
        } else if (f < 0.34f) {
            leds[i] = scale(0, 255, 0, brightness);
        } else if (f < 0.67f) {
            leds[i] = scale(255, 0, 0, brightness);
        } else {
            leds[i] = scale(0, 0, 255, brightness);
        }
    }
}

void spoiler_fx_sweep(rgb8_t *leds, int n, float progress, float brightness)
{
    int on = (int)(progress * (float)n + 0.5f);
    for (int i = 0; i < n; i++) {
        leds[i] = (i < on) ? scale(255, 255, 255, brightness) : scale(0, 0, 0, 0);
    }
}
