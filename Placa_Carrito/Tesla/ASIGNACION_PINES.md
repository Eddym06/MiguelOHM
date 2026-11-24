# Asignación de pines del carrito (Tesla): solo ESP32-S3 + PCA9555

## Estructura jerárquica

La hoja raíz une los bloques por etiquetas (nombre de red = nombre del pin del bloque de control):

- `Alimentacion` (protección de batería, medición y los tres bucks).
- `Control ESP32`: **solo** el ESP32-S3 (con su circuito mínimo: reset, BOOT y filtrado de 3V3) y el expansor PCA9555 (con su filtrado y A0-A2 a GND). Sin pull-ups ni pull-downs de otras funciones.
- `Pull-ups del control`: pull-ups del bus I2C (GPIO39 SCL, GPIO40 SDA) y de la interrupción del PCA9555 (GPIO42).
- `Drivers motores`: DRV8424 con sus pasivos y los pull-downs de arranque de APH, BPH y nSleep.
- `Faros`, `Tira LED`, `LEDs RGB y estado`.
- `Comunicacion con el usuario`: todo lo que ve y toca el usuario: entrada de batería, salidas a motores, servos, sensores, USB-C, UART, radio nRF24 y el conector de expansión J20 (socket hembra para una placa encima).

Todos los nombres de señal del bloque de control son GPIO: `GPIOn` para los pines del S3 y `EXP_GPIOn` (0 a 15) para los pines del expansor. Los nombres funcionales (ENC_A1, APH, LED_AP…) viven solo en las hojas de periféricos.
El P4 salió de este proyecto: vive en la placa modular `../Placa_Modular_S3_P4`.

## ESP32-S3 (WROOM-1 N16R2, U3): pines nativos

| GPIO | Uso | GPIO | Uso |
|---|---|---|---|
| 1 | VBAT_ADC (ADC1; ADC2 no sirve con WiFi) | 17 / 18 | AEN / BEN (PWM de los motores) |
| 4 / 5 | ENC_A1 / ENC_A2 | 21 | LUCES (faros, PWM) |
| 6 / 7 | ENC_B1 / ENC_B2 | 38 | LED_DATA (WS2812) |
| 8 / 9 / 14 | SERVO_DIR / SERVO_1 / SERVO_2 | 41 / 15 / 16 | RGB_R / RGB_G / RGB_B (PWM) |
| 10 / 11 / 12 / 13 | NRF_CSN / NRF_MOSI / NRF_SCK / NRF_MISO | 39 / 40 | I2C SCL / SDA |
| 19 / 20 | USB-C del S3 (D- / D+) | 47 / 48 | GPS_RX / GPS_TX |
| 43 / 44 | UART0 (J2) | 35 | LIDAR_RX |
| 0 | solo botón BOOT | 42 | interrupción del PCA9555 (INT) |
| 2, 36, 37 | libres (J20) | 3, 45, 46 | pines de arranque: sin uso, sin conectar |

- Pines de arranque del S3 (GPIO3, GPIO45, GPIO46): sin uso y sin conectar.
- GPIO35-37 quedan libres solo con módulos de PSRAM quad (R2) o sin PSRAM. Con el N16R8 (octal) no se pueden usar.

## PCA9555 (U14, I2C 0x20, mismo bus que el GPS y la IMU)

| Pin | Señal | Uso |
|---|---|---|
| P0.0 / P0.1 | EXP_GPIO0 / EXP_GPIO1 | NRF_CE / NRF_IRQ |
| P0.2 / P0.3 / P0.4 | EXP_GPIO2 / 3 / 4 | APH / BPH / nSleep (con pull-down en Drivers motores) |
| P0.5 | EXP_GPIO5 | nFault (entrada) |
| P0.6 / P0.7 | EXP_GPIO6 / 7 | LED_AP / LED_AN |
| P1.0 / P1.1 | EXP_GPIO8 / 9 | LED_BP / LED_BN |
| P1.2 a P1.7 | EXP_GPIO10 a 15 | libres (J20) |
| INT | GPIO42 | interrupción hacia el S3 |
| A0-A2 | GND | dirección 0x20 |

- Lo rápido o con PWM queda en pines nativos; lo lento (dirección, sleep, LEDs, CE del nRF24) va por I2C. Al encender, el PCA9555 deja todo como entrada con un pull-up interno de unos 100 kΩ, por eso APH, BPH y nSleep llevan pull-down de 10 k y el CE del nRF24 también (10 k, no 100 k).
- Cada acceso I2C tarda ~100 µs a 400 kHz: no sirve para PWM ni para señales rápidas.

## Conector J20 (socket hembra 1x16)

| Pines | Señal |
|---|---|
| 1 / 2 | 3V3 / GND |
| 3 / 4 / 5 | I2C SDA (GPIO40) / SCL (GPIO39) / INT (GPIO42) |
| 6 / 7 / 8 | GPIO2 / GPIO36 / GPIO37 |
| 9 a 14 | EXP_GPIO10 a EXP_GPIO15 |
| 15 / 16 | 5V / GND |

## Verificación contra los datasheets

- ESP32-S3-WROOM-1 v1.8: numeración de pines, EN con RC de 10 kΩ y 1 µF, GPIO0 al botón BOOT, strapping (GPIO0, 3, 45, 46) sin uso, filtrado de 3V3 con 22 µF y 0,1 µF (C131 pasó de 10 a 22 µF), IO35-37 libres en la variante de PSRAM quad.
- PCA9555 (TI): pull-ups obligatorios en SCL, SDA e INT (colector abierto), alimentación 2,3 a 5,5 V, dirección 0x20 con A0-A2 a GND, pull-up interno de ~100 kΩ en las E/S al encender (R54 pasó de 100 a 10 kΩ para que CE quede en bajo al arrancar).

## Notas de diseño

- El LiDAR (~20-25 KB/s por UART), el PID de las 2 ruedas, GPS, IMU, nRF24 y la web caben en un S3. Conviene fijar el control de motores a un núcleo y el WiFi al otro.
- LEDs RGB de ruedas: ULN2003A (SOIC-16, JLCPCB básica C7512). El puente J18 elige 12 V (1-2) o 5 V (2-3) para el común.
- USB-C del S3: solo datos (programar y consola). La placa se alimenta únicamente por la batería de 12 V.
- Un solo GND en todo el proyecto: se unificaron GND y GNDPWR y se quitó el net tie NT1. Los símbolos de alimentación (GND, +3V3, +5V, VM) son globales y comparten la red entre todas las hojas.
