#pragma once
// Pines (segun KAKATA_RC433_V1.kicad_pcb) y parametros ajustables del control.

// ---------------- Pines ----------------
// Joysticks (ADC1). MT = eje vertical (Y), MD = eje horizontal (X)
#define PIN_JOY0_Y      4    // ADC_JOY0_MT -> acelerar / retroceder
#define PIN_JOY0_X      5    // ADC_JOY0_MD (sin uso)
#define PIN_JOY0_BTN    3    // FRENO motor mientras se mantiene presionado
#define PIN_JOY1_X      1    // ADC_JOY1_MD -> giro / servo
#define PIN_JOY1_Y      2    // ADC_JOY1_MT (sin uso)
#define PIN_JOY1_BTN    46   // cambia MANUAL <-> GYRO

// Botones (activos en LOW, pull-up externo de 22K)
#define PIN_BTN1        9    // BTN_0 -> faros
#define PIN_BTN2        10   // BTN_2 -> sin uso
#define PIN_BTN4        11   // BTN_1 -> pantalla del OLED
#define PIN_BTN3        12   // BTN_3 -> LEDs RGB de las ruedas

// LEDs de estado del control (via 74HC04, HIGH = encendido)
#define PIN_LED1        45   // enlace RF con el carrito
#define PIN_LED2        48   // modo GYRO activo
#define PIN_LED3        47   // faros encendidos
#define PIN_LED4        21   // LEDs RGB encendidos
#define PIN_LED5        14   // parpadea al transmitir
#define PIN_LED6        13   // bateria baja

// I2C: OLED (0x3C) y MPU-6050 (0x68)
#define PIN_SDA         6
#define PIN_SCL         7

// Radio 433 MHz
#define PIN_RF_EN       15   // EN del transmisor
#define PIN_RF_TX       17   // DAT del transmisor
#define PIN_RF_RX       18   // DO del receptor
#define RF_SERIAL       Serial1
#define RF_EN_ACTIVE_HIGH 1

// Bateria
#define PIN_VBAT        8
#define VBAT_R_TOP      100000.0f  // R26
#define VBAT_R_BOT      22000.0f   // R27
#define VBAT_CAL        1.00f      // factor de ajuste fino (comparar con multimetro)

// ---------------- Ajustes ----------------
#define OLED_ADDR           0x3C
#define LONG_PRESS_MS       2000

// Joysticks
#define JOY_DEADZONE        0.08f  // zona muerta (fraccion del recorrido)
#define JOY_TRAVEL          0.90f  // recorrido util (el ADC no llega al extremo)
#define JOY0_Y_INVERT       0      // pon 1 si "adelante" da valores negativos
#define JOY1_X_INVERT       0      // pon 1 si derecha da valores negativos

// Modo GYRO (grados de inclinacion respecto a la posicion al activar el modo)
#define TILT_DEADZONE_DEG   6.0f
#define TILT_FULL_DEG       30.0f
#define GYRO_SWAP_AXES      0      // 1 si el MPU esta girado 90 grados en el PCB
#define GYRO_INVERT_FWD     0      // 1 si inclinar hacia adelante da "reversa"
#define GYRO_INVERT_STEER   0      // 1 si inclinar a la derecha gira a la izquierda

// Velocimetro
#define SPEED_MAX_KMH       20.0f
#define RPM_MAX             1000.0f  // los motores llegan a ~1000 rpm

// Tiempos (ms)
#define CMD_PERIOD_MS       100    // ciclo comando (33 ms) + respuesta del carrito (33 ms)
#define UI_PERIOD_MS        66     // refresco del OLED (~15 fps)
#define LINK_TIMEOUT_MS     1000   // sin telemetria -> "desconectado"
