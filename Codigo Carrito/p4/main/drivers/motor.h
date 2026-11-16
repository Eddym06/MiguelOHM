#pragma once
#include <stdbool.h>

#define MOTOR_L 0
#define MOTOR_R 1

typedef enum {
    MOTOR_COAST = 0,   // IN1=IN2=0, EN=0: rueda libre (desacelera por inercia)
    MOTOR_DRIVE,       // IN segun el signo del duty, EN con PWM
    MOTOR_BRAKE,       // IN1=IN2=1, EN=100 %: freno motor
} motor_mode_t;

// Configura PWM (5 kHz), pines de direccion y encoders. Un motor sin pines queda inactivo.
void  motors_init(void);
bool  motor_present(int m);
bool  motor_encoder_present(int m);
// duty en -1..1 (el signo es la direccion; solo se usa en MOTOR_DRIVE)
void  motor_set(int m, motor_mode_t mode, float duty);
// Calcula las rpm de rueda desde los encoders. Llamar cada ciclo de control con su dt (s).
void  motors_update_encoders(float dt);
float motor_rpm(int m);            // con signo, ya filtrada y con la inversion aplicada
