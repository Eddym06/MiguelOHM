#pragma once
#include <stdbool.h>

// Inicia el bus I2C y la tarea que lee BMI160 (100 Hz), brujula (~50 Hz) e INA219 (10 Hz)
// y mantiene en g_car la inclinacion, el rumbo fusionado, y la bateria.
void sensors_start(void);

// Acciones de prueba (desde la pagina web)
void sensors_gyro_recal(void);
void sensors_compass_cal(bool start);   // start=true empieza a medir; false termina y guarda
bool sensors_compass_cal_active(void);
