#pragma once
#include <stdbool.h>
#include <stdint.h>

// Control de traccion: 2 ruedas con PID de velocidad (encoders), sincronia entre ruedas,
// giro por diferencial + servo, freno motor, rueda libre al soltar y failsafe.
void drive_start(void);

// Orden de movimiento (-100..100). Cada llamada rearma el failsafe.
void drive_set_command(float throttle, float steer, bool brake);
// Invalida la orden actual (motores a rueda libre al instante).
void drive_stop(void);

// Pruebas desde la web (maximo 3 s). Dejan de actuar solas al vencer `ms`.
void drive_test_motor(int motor, float duty_pct, uint32_t ms);   // lazo abierto
void drive_test_pid(float rpm, uint32_t ms);                     // ambos motores con PID
void drive_test_servo(float deg, uint32_t ms);
bool drive_test_active(void);
