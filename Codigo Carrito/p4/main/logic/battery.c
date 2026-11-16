#include "battery.h"

uint8_t battery_percent_li(float pack_volts, int cells)
{
    static const float tv[] = {3.27f, 3.61f, 3.69f, 3.71f, 3.73f, 3.75f, 3.77f, 3.79f, 3.80f, 3.82f, 3.84f,
                               3.85f, 3.87f, 3.91f, 3.95f, 3.98f, 4.02f, 4.08f, 4.11f, 4.15f, 4.20f};
    static const uint8_t tp[] = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50,
                                 55, 60, 65, 70, 75, 80, 85, 90, 95, 100};
    const int n = (int)(sizeof(tv) / sizeof(tv[0]));
    if (cells < 1) cells = 1;
    float v = pack_volts / (float)cells;
    if (v <= tv[0]) return 0;
    if (v >= tv[n - 1]) return 100;
    for (int i = 1; i < n; i++) {
        if (v <= tv[i]) {
            float f = (v - tv[i - 1]) / (tv[i] - tv[i - 1]);
            float p = tp[i - 1] + f * (float)(tp[i] - tp[i - 1]);
            return (uint8_t)(p + 0.5f);
        }
    }
    return 100;
}
