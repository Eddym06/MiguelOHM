#include "sensors.h"
#include <math.h>
#include "board_pins.h"
#include "car_state.h"
#include "settings.h"
#include "battery.h"
#include "heading.h"
#include "drivers/i2c_bus.h"
#include "drivers/bmi160.h"
#include "drivers/ina219.h"
#include "drivers/compass.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sensors";

#define R2D 57.2957795f

static volatile bool s_gyro_recal;
static volatile bool s_cal_active;
static volatile bool s_cal_stop;
static float s_cal_min[2], s_cal_max[2];

void sensors_gyro_recal(void)
{
    s_gyro_recal = true;
}

void sensors_compass_cal(bool start)
{
    if (start) {
        s_cal_min[0] = s_cal_min[1] = 1e9f;
        s_cal_max[0] = s_cal_max[1] = -1e9f;
        s_cal_active = true;
    } else {
        s_cal_stop = true;
    }
}

bool sensors_compass_cal_active(void)
{
    return s_cal_active;
}

static void sensors_task(void *arg)
{
    heading_t hdg;
    heading_init(&hdg, 0.02f);

    float roll = 0, pitch = 0;
    bool att_init = false;
    uint32_t last_us = (uint32_t)esp_timer_get_time();
    uint32_t tick = 0;
    float vbat_filt = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10));
        tick++;

        if (s_gyro_recal) {
            s_gyro_recal = false;
            bmi160_calibrate_gyro();
            last_us = (uint32_t)esp_timer_get_time();
        }

        // ---------------- IMU (100 Hz) ----------------
        imu_sample_t s;
        if (bmi160_ok() && bmi160_read(&s)) {
            uint32_t us = (uint32_t)esp_timer_get_time();
            float dt = (us - last_us) * 1e-6f;
            last_us = us;
            if (dt <= 0 || dt > 0.2f) dt = 0.01f;

            float acc_roll = atan2f(s.ay, s.az) * R2D;
            float acc_pitch = atan2f(-s.ax, sqrtf(s.ay * s.ay + s.az * s.az)) * R2D;
            if (!att_init) {
                roll = acc_roll;
                pitch = acc_pitch;
                att_init = true;
            } else {
                roll = 0.98f * (roll + s.gx * dt) + 0.02f * acc_roll;
                pitch = 0.98f * (pitch + s.gy * dt) + 0.02f * acc_pitch;
            }
            // gz positivo = giro antihorario (Z hacia arriba); el rumbo crece en sentido horario
            heading_gyro(&hdg, -s.gz, dt);

            g_car.roll_deg = roll;
            g_car.pitch_deg = pitch;
            g_car.yaw_rate_cw_dps = -s.gz;
            g_car.imu_ok = true;
        } else {
            g_car.imu_ok = false;
        }

        // ---------------- Brujula (~50 Hz) ----------------
        if (compass_ok() && (tick % 2 == 0)) {
            float mx, my, mz;
            if (compass_read(&mx, &my, &mz)) {
                g_car.mag_x = mx;
                g_car.mag_y = my;
                g_car.mag_z = mz;
                g_car.compass_ok = true;

                if (s_cal_active) {   // calibracion hard-iron 2D: girar el carrito 360 grados
                    if (mx < s_cal_min[0]) s_cal_min[0] = mx;
                    if (mx > s_cal_max[0]) s_cal_max[0] = mx;
                    if (my < s_cal_min[1]) s_cal_min[1] = my;
                    if (my > s_cal_max[1]) s_cal_max[1] = my;
                }
                float h = compass_heading(mx, my, mz, roll, pitch, CFG(MAG_OFF_X), CFG(MAG_OFF_Y),
                                          CFG(COMPASS_DECL_DEG), CFG(COMPASS_ROT_DEG),
                                          CFG(COMPASS_INVERT) > 0.5f);
                if (!g_car.imu_ok) {
                    hdg.yaw_deg = h;       // sin giroscopio, la brujula manda
                    hdg.init = true;
                } else {
                    heading_mag(&hdg, h);
                }
            }
        }
        if (s_cal_stop) {
            s_cal_stop = false;
            s_cal_active = false;
            if (s_cal_max[0] > s_cal_min[0] && s_cal_max[1] > s_cal_min[1]) {
                float ox = (s_cal_max[0] + s_cal_min[0]) / 2;
                float oy = (s_cal_max[1] + s_cal_min[1]) / 2;
                settings_set(SET_MAG_OFF_X, ox);
                settings_set(SET_MAG_OFF_Y, oy);
                ESP_LOGI(TAG, "calibracion brujula: offset X=%.1f Y=%.1f uT", ox, oy);
            } else {
                ESP_LOGW(TAG, "calibracion brujula sin datos suficientes");
            }
        }
        if (hdg.init) {
            g_car.heading_deg = hdg.yaw_deg;
            g_car.heading_valid = true;
        }

        // ---------------- INA219 (10 Hz) ----------------
        if (ina219_ok() && (tick % 10 == 0)) {
            float v, a;
            if (ina219_read(&v, &a)) {
                vbat_filt = (vbat_filt == 0) ? v : vbat_filt + (v - vbat_filt) * 0.2f;
                g_car.vbat_v = vbat_filt;
                g_car.current_a = a;
                g_car.batt_pct = battery_percent_li(vbat_filt, (int)CFG(BATT_CELLS));
                g_car.ina_ok = true;
            } else {
                g_car.ina_ok = false;
            }
        }
    }
}

void sensors_start(void)
{
    if (!i2c_bus_init()) return;
    bmi160_init();
    compass_init();
    ina219_init();
    xTaskCreate(sensors_task, "sensors", 6144, NULL, 7, NULL);
}
