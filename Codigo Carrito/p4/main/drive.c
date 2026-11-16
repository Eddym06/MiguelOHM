#include "drive.h"
#include <math.h>
#include "car_state.h"
#include "settings.h"
#include "pid.h"
#include "drive_mix.h"
#include "drivers/motor.h"
#include "drivers/servo.h"
#include "drivers/lights.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "drive";

#define LOOP_MS 20

static struct {
    volatile float thr, steer;
    volatile bool brake;
    volatile uint32_t last_ms;
} s_cmd;

enum { T_NONE = 0, T_MOTOR, T_PID, T_SERVO };
static struct {
    volatile int kind;
    volatile int motor;
    volatile float value;
    volatile uint32_t until;
} s_test;

void drive_set_command(float throttle, float steer, bool brake)
{
    s_cmd.thr = fmaxf(-100.0f, fminf(100.0f, throttle));
    s_cmd.steer = fmaxf(-100.0f, fminf(100.0f, steer));
    s_cmd.brake = brake;
    uint32_t t = now_ms();
    s_cmd.last_ms = t ? t : 1;
}

void drive_stop(void)
{
    s_cmd.thr = 0;
    s_cmd.steer = 0;
    s_cmd.brake = false;
    s_cmd.last_ms = 0;
}

static void start_test(int kind, int motor, float v, uint32_t ms)
{
    if (ms > 3000) ms = 3000;
    s_test.kind = kind;
    s_test.motor = motor;
    s_test.value = v;
    s_test.until = now_ms() + ms;
}

void drive_test_motor(int motor, float duty_pct, uint32_t ms) { start_test(T_MOTOR, motor, duty_pct, ms); }
void drive_test_pid(float rpm, uint32_t ms) { start_test(T_PID, 0, rpm, ms); }
void drive_test_servo(float deg, uint32_t ms) { start_test(T_SERVO, 0, deg, ms); }

bool drive_test_active(void)
{
    return s_test.kind != T_NONE && (int32_t)(s_test.until - now_ms()) > 0;
}

static void drive_task(void *arg)
{
    pid_ctl_t pid[2];
    pid_init(&pid[0], 1, 0, 0, 0, 1);
    pid_init(&pid[1], 1, 0, 0, 0, 1);
    float ramp[2] = {0, 0};
    int last_sign[2] = {1, 1};
    float stall_ms[2] = {0, 0}, inv_ms[2] = {0, 0};
    bool fault[2] = {false, false};

    TickType_t wake = xTaskGetTickCount();
    int64_t last_us = esp_timer_get_time();

    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(LOOP_MS));
        int64_t us = esp_timer_get_time();
        float dt = (float)(us - last_us) * 1e-6f;
        last_us = us;
        if (dt < 0.005f || dt > 0.1f) dt = LOOP_MS / 1000.0f;
        uint32_t now = now_ms();

        motors_update_encoders(dt);
        float rpm[2] = {motor_rpm(0), motor_rpm(1)};
        const float rpm_max = CFG(RPM_MAX);
        for (int i = 0; i < 2; i++) pid_set_gains(&pid[i], CFG(PID_KP), CFG(PID_KI), CFG(PID_KD));

        // ---------- que se pide ----------
        float thr = 0, steer = 0;
        bool brake = false;
        bool ovr[2] = {false, false};
        float ovr_duty[2] = {0, 0};
        bool servo_ovr = false;
        float servo_ovr_deg = 0;

        if (drive_test_active()) {
            if (s_test.kind == T_MOTOR) {
                ovr[s_test.motor & 1] = true;
                ovr_duty[s_test.motor & 1] = fmaxf(-1.0f, fminf(1.0f, s_test.value / 100.0f));
            } else if (s_test.kind == T_PID) {
                thr = fmaxf(-100.0f, fminf(100.0f, s_test.value / rpm_max * 100.0f));
            } else if (s_test.kind == T_SERVO) {
                servo_ovr = true;
                servo_ovr_deg = s_test.value;
            }
        } else {
            s_test.kind = T_NONE;
            bool fresh = s_cmd.last_ms != 0 && (now - s_cmd.last_ms) < (uint32_t)CFG(FAILSAFE_MS);
            if (fresh) {
                thr = s_cmd.thr;
                steer = s_cmd.steer;
                brake = s_cmd.brake;
            }
        }

        // ---------- direccion: servo (y diferencial mas abajo) ----------
        float servo_deg = servo_ovr ? servo_ovr_deg : steer / 100.0f * CFG(SERVO_RANGE_DEG) + CFG(SERVO_TRIM_DEG);
        servo_set_angle(servo_deg);

        // ---------- motores ----------
        float applied_thr = thr;
        if (ovr[0] || ovr[1]) {
            for (int i = 0; i < 2; i++) {
                if (ovr[i]) motor_set(i, MOTOR_DRIVE, ovr_duty[i]);
                else motor_set(i, MOTOR_COAST, 0);
                ramp[i] = rpm[i];
                pid_reset(&pid[i]);
            }
            applied_thr = (ovr[0] ? ovr_duty[0] : ovr_duty[1]) * 100.0f;
        } else if (brake) {
            // freno motor: IN1=IN2=1. Se pierde la rampa y el PID.
            for (int i = 0; i < 2; i++) {
                motor_set(i, MOTOR_BRAKE, 0);
                ramp[i] = 0;
                pid_reset(&pid[i]);
            }
            applied_thr = 0;
        } else if (fabsf(thr) < 1.0f) {
            // acelerador suelto: rueda libre, desacelera por inercia
            for (int i = 0; i < 2; i++) {
                motor_set(i, MOTOR_COAST, 0);
                ramp[i] = rpm[i];
                pid_reset(&pid[i]);
                stall_ms[i] = inv_ms[i] = 0;
                fault[i] = false;
            }
            applied_thr = 0;
        } else {
            mix_cfg_t mc = {rpm_max, CFG(DIFF_GAIN)};
            float tgt[2];
            drive_mix(thr, steer, &mc, &tgt[0], &tgt[1]);
            float step = CFG(RAMP_PCT_S) / 100.0f * rpm_max * dt;
            for (int i = 0; i < 2; i++) ramp[i] = ramp_toward(ramp[i], tgt[i], step);

            float sp[2] = {ramp[0], ramp[1]};
            if (motor_encoder_present(0) && motor_encoder_present(1) && !fault[0] && !fault[1]) {
                sync_adjust(rpm[0], rpm[1], &sp[0], &sp[1], CFG(SYNC_KP), rpm_max);
            }

            for (int i = 0; i < 2; i++) {
                float s = sp[i];
                if (fabsf(s) < 0.02f * rpm_max) {
                    motor_set(i, MOTOR_COAST, 0);
                    continue;
                }
                int sign = s >= 0 ? 1 : -1;
                if (sign != last_sign[i]) {
                    pid_reset(&pid[i]);
                    last_sign[i] = sign;
                }
                float meas = rpm[i] * (float)sign;     // positivo = gira hacia donde se pide

                // si hay que invertir el giro con la rueda aun lanzada, esperar sin "frenar al reves"
                if (meas < -0.15f * rpm_max) {
                    motor_set(i, MOTOR_COAST, 0);
                    pid_reset(&pid[i]);
                    inv_ms[i] += dt * 1000.0f;
                } else {
                    float ff = fminf(1.0f, CFG(PID_FF) * fabsf(s) / rpm_max);
                    float duty_mag;
                    if (!motor_encoder_present(i) || fault[i]) {
                        duty_mag = ff;                 // lazo abierto si no hay encoder fiable
                    } else {
                        duty_mag = pid_step(&pid[i], fabsf(s) / rpm_max, meas / rpm_max, ff, dt);
                    }
                    float min_duty = CFG(MIN_DUTY_PCT) / 100.0f;
                    motor_set(i, MOTOR_DRIVE, (float)sign * duty_deadband(duty_mag, min_duty));

                    // ---- deteccion de encoder muerto o invertido ----
                    if (motor_encoder_present(i) && !fault[i]) {
                        if (duty_mag > 0.35f && fabsf(rpm[i]) < 0.03f * rpm_max) stall_ms[i] += dt * 1000.0f;
                        else stall_ms[i] = 0;
                        if (meas < -0.05f * rpm_max) inv_ms[i] += dt * 1000.0f;
                        else if (inv_ms[i] > 0) inv_ms[i] -= 0.5f * dt * 1000.0f;
                    }
                }
                if (!fault[i] && motor_encoder_present(i) && (stall_ms[i] > 1200 || inv_ms[i] > 800)) {
                    fault[i] = true;
                    ESP_LOGW(TAG, "encoder %s sin lectura coherente (%s): lazo abierto hasta soltar el acelerador",
                             i == 0 ? "izq" : "der", inv_ms[i] > 800 ? "sentido invertido, revisar ajuste" : "no cuenta");
                }
            }
        }

        // ---------- estado publicado ----------
        g_car.rpm_l = rpm[0];
        g_car.rpm_r = rpm[1];
        g_car.speed_kmh = 0.5f * (fabsf(rpm[0]) + fabsf(rpm[1])) * 3.14159265f * CFG(WHEEL_MM) * 0.00006f;
        g_car.servo_deg = servo_deg;
        g_car.thr_applied = (int8_t)applied_thr;
        g_car.steer_applied = (int8_t)steer;
        g_car.brake = brake;
        g_car.enc_fault = (uint8_t)((fault[0] ? 1 : 0) | (fault[1] ? 2 : 0));

        lights_tick(applied_thr, brake, now);
    }
}

void drive_start(void)
{
    motors_init();
    servo_init();
    lights_init();
    xTaskCreate(drive_task, "drive", 6144, NULL, 10, NULL);
}
