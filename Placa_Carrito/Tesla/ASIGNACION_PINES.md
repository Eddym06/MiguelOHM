# Asignación de pines del carrito (Tesla): solo ESP32-S3 + PCA9555

Bloques del esquemático (la raíz no lleva etiquetas: los bloques se unen con cables):
`Alimentacion`, `Control ESP32` (S3 + PCA9555 + conector de pines libres), `Drivers motores`, `Faros`, `Tira LED`, `LEDs RGB y estado`,
`Radio nRF24` y `Salidas externas` (conectores). Dentro de cada hoja las redes locales van cableadas; solo hay etiquetas jerárquicas en el borde de la hoja.
El P4 salió de este proyecto: vive en la placa modular `../Placa_Modular_S3_P4`.

## ESP32-S3 (WROOM-1 N16R2, U3): pines nativos

| GPIO | Señal | GPIO | Señal |
|---|---|---|---|
| 1 | VBAT_ADC (ADC1; ADC2 no sirve con WiFi) | 17 / 18 | AEN / BEN (PWM de los motores) |
| 4 / 5 | ENC_A1 / ENC_A2 | 21 | LUCES (faros, PWM) |
| 6 / 7 | ENC_B1 / ENC_B2 | 38 | LED_DATA (WS2812) |
| 8 / 9 / 14 | SERVO_DIR / SERVO_1 / SERVO_2 | 41 / 15 / 16 | RGB_R / RGB_G / RGB_B (PWM) |
| 10 / 11 / 12 / 13 | NRF_CSN / NRF_MOSI / NRF_SCK / NRF_MISO | 39 / 40 | GPS_SCL / GPS_SDA (bus I2C) |
| 19 / 20 | USB-C del S3 (D- / D+) | 47 / 48 | GPS_RX / GPS_TX |
| 43 / 44 | UART0 (J2) | 35 | LIDAR_RX |
| 0 | BOOT | 42 | EXP_INT (interrupción del PCA9555) |
| 2, 36, 37 | libres (J20) | 3, 45, 46 | strapping: solo en J20, sin uso permanente |

- Los 36 GPIO del módulo están conectados: ninguno queda sin cablear.
- GPIO35-37 quedan libres solo con módulos de PSRAM quad (R2) o sin PSRAM. Con el N16R8 (octal) no se pueden usar.

## PCA9555 (U14, I2C 0x20, mismo bus que el GPS y la IMU)

| Pin | Señal | Pin | Señal |
|---|---|---|---|
| P0.0 | NRF_CE | P1.0 / P1.1 | LED_BP / LED_BN |
| P0.1 | NRF_IRQ (entrada) | P1.2-P1.7 | libres (EXP_IO12-17, J20) |
| P0.2 / P0.3 | APH / BPH (con pull-down) | INT | EXP_INT → GPIO42 del S3 |
| P0.4 | nSleep (con pull-down) | A0-A2 | a GND |
| P0.5 | nFault (entrada) | | |
| P0.6 / P0.7 | LED_AP / LED_AN | | |

- Lo rápido o con PWM queda en pines nativos; lo lento (dirección, sleep, LEDs, CE del nRF24) va por I2C. Al encender, el PCA9555 deja todo como entrada, por eso APH, BPH y nSleep llevan pull-down.
- Cada acceso I2C tarda ~100 µs a 400 kHz: no sirve para PWM ni para señales rápidas.

## Notas de diseño

- El LiDAR (~20-25 KB/s por UART), el PID de las 2 ruedas, GPS, IMU, nRF24 y la web caben en un S3. Conviene fijar el control de motores a un núcleo y el WiFi al otro.
- LEDs RGB de ruedas: ULN2003A (SOIC-16, JLCPCB básica C7512). El puente J18 elige 12 V (1-2) o 5 V (2-3) para el común.
- USB-C del S3: solo datos (programar y consola). La placa se alimenta únicamente por la batería de 12 V.
