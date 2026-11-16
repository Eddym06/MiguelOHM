#pragma once
// =====================================================================================
//  PINES DEL ESP32-P4 DEL CARRITO  -  PENDIENTES DE LA PCB
//  Todo en PIN_NC (-1) hasta que se defina la PCB. Un modulo con algun pin en -1 se
//  desactiva y el firmware lo avisa en el log al arrancar; nada se bloquea.
// =====================================================================================

#define PIN_NC   (-1)
// mascara de bit de un pin (seguro aunque el pin sea -1 en tiempo de compilacion)
#define PIN_BIT(p)  (1ULL << ((p) < 0 ? 0 : (p)))

// ---- Radio 433 MHz (UART) ----
#define PIN_RF_TX        PIN_NC   // DAT del transmisor
#define PIN_RF_RX        PIN_NC   // DO del receptor
#define PIN_RF_EN        PIN_NC   // EN del transmisor
#define RF_EN_ACTIVE_HIGH 1
#define RF_UART_NUM      1

// ---- Puente H L298N (motor izquierdo = A, derecho = B) ----
#define PIN_MOT_L_EN     PIN_NC   // ENA (PWM 5 kHz)
#define PIN_MOT_L_IN1    PIN_NC
#define PIN_MOT_L_IN2    PIN_NC
#define PIN_MOT_R_EN     PIN_NC   // ENB (PWM 5 kHz)
#define PIN_MOT_R_IN1    PIN_NC   // IN3
#define PIN_MOT_R_IN2    PIN_NC   // IN4
#define MOTOR_PWM_HZ     5000

// ---- Encoders en cuadratura (canal A y B de cada motor) ----
#define PIN_ENC_L_A      PIN_NC
#define PIN_ENC_L_B      PIN_NC
#define PIN_ENC_R_A      PIN_NC
#define PIN_ENC_R_B      PIN_NC

// ---- Servo de direccion (MG90) ----
#define PIN_SERVO        PIN_NC

// ---- Luces ----
#define PIN_HEADLIGHTS   PIN_NC   // base del transistor NPN de los faros
#define PIN_RGB_R        PIN_NC   // LEDs RGB de las ruedas (4 en paralelo, comunes)
#define PIN_RGB_G        PIN_NC
#define PIN_RGB_B        PIN_NC
#define RGB_COMMON_ANODE 0        // 1 si el driver invierte la logica
#define PIN_SPOILER      PIN_NC   // DIN de la tira WS2812 del aleron

// ---- Bus I2C: BMI160, INA219, brujula del GPS ----
#define PIN_I2C_SDA      PIN_NC
#define PIN_I2C_SCL      PIN_NC
#define BMI160_ADDR      0x68     // 0x69 si SDO esta a 3V3
#define INA219_ADDR      0x40
#define INA219_SHUNT_MOHM 100     // resistencia shunt del modulo (R100 = 100 mohm)

// ---- GPS M100 Pro (UART, NMEA) ----
#define PIN_GPS_RX       PIN_NC   // RX del P4 <- TX del GPS
#define PIN_GPS_TX       PIN_NC   // TX del P4 -> RX del GPS (opcional)
#define GPS_UART_NUM     2

// ---- LIDAR D6 (UART 230400, solo se recibe) ----
#define PIN_LIDAR_RX     PIN_NC
#define LIDAR_UART_NUM   3
#define LIDAR_BAUD       230400

// ---- Enlace SPI con el ESP32-S3 (el P4 es maestro) ----
#define PIN_SPI_SCLK     PIN_NC
#define PIN_SPI_MOSI     PIN_NC
#define PIN_SPI_MISO     PIN_NC
#define PIN_SPI_CS       PIN_NC
#define PIN_SPI_HS       PIN_NC   // "datos listos" del S3 (entrada, opcional pero recomendado)
#define SPI_LINK_HZ      4000000

// ---- Canales LEDC (no cambiar salvo conflicto) ----
#define LEDC_CH_MOT_L    0
#define LEDC_CH_MOT_R    1
#define LEDC_CH_SERVO    2
#define LEDC_CH_RGB_R    3
#define LEDC_CH_RGB_G    4
#define LEDC_CH_RGB_B    5
