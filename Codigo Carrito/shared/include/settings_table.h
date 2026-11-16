#pragma once
// Tabla unica de ajustes del carrito. La usan:
//   - el P4 (valores por defecto, limites y guardado en NVS),
//   - el S3 (envia nombre/unidad/limites a la pagina web para armar el menu de configuracion).
//
// X(ID, "etiqueta", "unidad", defecto, minimo, maximo, paso)

#define SETTINGS_TABLE(X) \
    X(SERVO_CENTER_US,   "Servo: pulso de centro",        "us",     1500, 1000, 2000, 1)     \
    X(SERVO_US_PER_DEG,  "Servo: us por grado",           "us/deg", 10,   5,    20,   0.1)   \
    X(SERVO_MIN_US,      "Servo: pulso minimo",           "us",     800,  500,  1500, 10)    \
    X(SERVO_MAX_US,      "Servo: pulso maximo",           "us",     2200, 1500, 2500, 10)    \
    X(SERVO_RANGE_DEG,   "Servo: giro maximo por lado",   "deg",    30,   5,    60,   1)     \
    X(SERVO_TRIM_DEG,    "Servo: ajuste de centro",       "deg",    0,    -15,  15,   0.5)   \
    X(SERVO_INVERT,      "Servo: invertir (0/1)",         "",       0,    0,    1,    1)     \
    X(MOTOR_L_INVERT,    "Motor izq: invertir (0/1)",     "",       0,    0,    1,    1)     \
    X(MOTOR_R_INVERT,    "Motor der: invertir (0/1)",     "",       0,    0,    1,    1)     \
    X(ENC_L_INVERT,      "Encoder izq: invertir (0/1)",   "",       0,    0,    1,    1)     \
    X(ENC_R_INVERT,      "Encoder der: invertir (0/1)",   "",       0,    0,    1,    1)     \
    X(RPM_MAX,           "RPM maximas de rueda",          "rpm",    1000, 100,  3000, 10)    \
    X(ENC_CPR,           "Cuentas encoder por vuelta",    "cnt",    440,  10,   5000, 1)     \
    X(WHEEL_MM,          "Diametro de rueda",             "mm",     65,   20,   200,  0.5)   \
    X(PID_KP,            "PID: Kp",                       "",       1.2,  0,    10,   0.05)  \
    X(PID_KI,            "PID: Ki",                       "",       3.0,  0,    20,   0.1)   \
    X(PID_KD,            "PID: Kd",                       "",       0.0,  0,    2,    0.01)  \
    X(PID_FF,            "PID: avance (feed-forward)",    "",       0.9,  0,    1.5,  0.01)  \
    X(SYNC_KP,           "Sincronia entre motores: Kp",   "",       0.5,  0,    3,    0.05)  \
    X(DIFF_GAIN,         "Giro por motor (diferencial)",  "",       0.35, 0,    1,    0.01)  \
    X(RAMP_PCT_S,        "Rampa de aceleracion",          "%/s",    300,  50,   1000, 10)    \
    X(MIN_DUTY_PCT,      "PWM minimo para arrancar",      "%",      12,   0,    40,   1)     \
    X(FAILSAFE_MS,       "Failsafe sin comandos",         "ms",     500,  200,  3000, 50)    \
    X(BATT_CELLS,        "Celdas de la bateria (Li-ion)", "S",      3,    1,    6,    1)     \
    X(RGB_BRIGHT,        "Brillo RGB ruedas",             "%",      100,  0,    100,  1)     \
    X(SPOILER_LEDS,      "Leds de la tira trasera",       "",       8,    1,    32,   1)     \
    X(SPOILER_BRIGHT,    "Brillo tira trasera",           "%",      40,   1,    100,  1)     \
    X(COMPASS_DECL_DEG,  "Declinacion magnetica",         "deg",    0,    -30,  30,   0.1)   \
    X(COMPASS_ROT_DEG,   "Brujula: giro de montaje",      "deg",    0,    -180, 180,  1)     \
    X(COMPASS_INVERT,    "Brujula: invertir sentido",     "",       0,    0,    1,    1)     \
    X(MAG_OFF_X,         "Brujula: offset X",             "uT",     0,    -500, 500,  0.1)   \
    X(MAG_OFF_Y,         "Brujula: offset Y",             "uT",     0,    -500, 500,  0.1)   \
    X(NAV_SPEED_PCT,     "Autonomo: velocidad",           "%",      40,   10,   100,  1)     \
    X(NAV_ARRIVE_M,      "Autonomo: radio de llegada",    "m",      3,    1,    10,   0.5)   \
    X(NAV_STEER_KP,      "Autonomo: ganancia de giro",    "",       1.0,  0.2,  3,    0.1)   \
    X(LIDAR_STOP_CM,     "LIDAR: distancia de parada",    "cm",     35,   10,   150,  1)     \
    X(LIDAR_AVOID_CM,    "LIDAR: distancia de esquive",   "cm",     100,  30,   300,  5)     \
    X(LIDAR_YAW_DEG,     "LIDAR: giro de montaje",        "deg",    0,    -180, 180,  1)     \
    X(LIDAR_INVERT,      "LIDAR: invertir sentido",       "",       0,    0,    1,    1)

typedef enum {
#define X(id, ...) SET_##id,
    SETTINGS_TABLE(X)
#undef X
    SET_COUNT
} setting_id_t;
