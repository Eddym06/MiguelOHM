# Asignación de pines del carrito (Tesla): solo ESP32-S3 + PCA9555

## Estructura jerárquica

La raíz tiene los conectores externos (con cable directo a la hoja `Externo`) y la hoja `USB-C`; el detalle está en [README.md](README.md#estructura-del-esquemático). Dentro de `Externo` los bloques se unen por etiquetas (nombre de red = nombre del pin del bloque de control):

- `Alimentacion` (protección de batería, medición y los dos bucks (3V3 y 5 V)).
- `Control ESP32`: **solo** el ESP32-S3 (con su circuito mínimo: reset, BOOT y filtrado de 3V3) y el expansor PCA9555 (con su filtrado y A0-A2 a GND). Incluye los pull-ups que exige el PCA9555 en SCL (GPIO39), SDA (GPIO40) e INT (GPIO42). Sin pull-downs de otras funciones.
- `Drivers motores`: DRV8424 con sus pasivos y los pull-downs de arranque de APH, BPH y nSleep.
- `Faros`, `Tira LED`, `LEDs RGB y estado`.

Todos los nombres de señal del bloque de control son GPIO: `GPIOn` para los pines del S3 y `EXP_GPIOn` (0 a 15) para los pines del expansor. Los nombres funcionales (ENC_A1, APH, LED_AP…) viven solo en las hojas de periféricos.
El P4 salió de este proyecto: vive en la placa modular `../Placa_Modular_S3_P4`.

## Alimentación en la jerarquía (entradas y salidas de rieles)

Los rieles entran y salen de cada bloque por pines de hoja explícitos, no solo por símbolos de alimentación globales:

- `Alimentacion` **saca** `VM`, `+3V3`, `+5V` y `GND` (bloque "Salidas de alimentacion" en su hoja; el buck de 3V3, el de 5 V, la medición y la protección reciben `GND` y `VM` por pines).
- Cada hoja consumidora **recibe** solo los rieles que usa, en su bloque "Entradas de alimentacion" (etiqueta jerárquica -> cable -> símbolo de alimentación): Control (`+3V3`, `GND`), Drivers motores y Faros (`VM`, `GND`), Tira LED (`+5V`, `GND`), LEDs RGB (`+3V3`, `+5V`, `VM`, `GND`), Comunicación con el usuario (`+3V3`, `+5V`, `GND`).
- En la hoja raíz cada pin de riel lleva una etiqueta global del mismo nombre, así todos los `GND`, `+3V3`, `+5V` y `VM` quedan unidos en toda la jerarquía. Sigue habiendo un solo `GND`.

## ESP32-S3 (WROOM-1 N16R2, U3): pines nativos

| GPIO | Uso | GPIO | Uso |
|---|---|---|---|
| 1 | VBAT_ADC (ADC1; ADC2 no sirve con WiFi) | 17 / 18 | AEN / BEN (PWM de los motores) |
| 4 / 5 | ENC_A1 / ENC_A2 | 21 | LUCES (faros, PWM) |
| 6 / 7 | ENC_B1 / ENC_B2 | 38 | TIRA_LED_3V3 (WS2812) |
| 8 / 9 / 14 | SERVO_DIR / SERVO_1 / SERVO_2 | 41 / 15 / 16 | RGB_R / RGB_G / RGB_B (PWM) |
| 10 / 11 / 12 / 13 | NRF_CSN / NRF_MOSI / NRF_SCK / NRF_MISO | 39 / 40 | I2C SCL / SDA |
| 19 / 20 | USB-C del S3 (D- / D+) | 47 / 48 | GPS_RX / GPS_TX |
| 43 / 44 | UART0 (J2) | 35 | LIDAR_RX |
| 0 | solo botón BOOT | 42 | interrupción del PCA9555 (INT) |
| 2, 36, 37 | libres (J20) | 3, 45, 46 | pines de arranque: sin uso, sin conectar |

- Pines de arranque del S3 (GPIO3, GPIO45, GPIO46): sin uso y sin conectar.
- GPIO35-37 quedan libres solo con módulos de PSRAM quad (R2) o sin PSRAM. Con el N16R8 (octal) no se pueden usar.
- **GPIO39, GPIO40 y GPIO42 no son pines libres:** los usa el expansor PCA9555 (GPIO39 = SCL, GPIO40 = SDA e INT en GPIO42). Estos tres pines NO salen de la hoja Control: no hay pines GPIO39/40/42 en la jerarquía. Por eso el GPS, la IMU, el INA219 y J20 quedan sin bus I2C hacia el S3 (hay que decidir cómo darles I2C). No se pueden usar como GPIO ni para PWM. Las únicas salidas nativas libres para PWM son GPIO2, GPIO36 y GPIO37 (en J20).

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
| 3 / 4 / 5 | SDA / SCL / INT: sin conexión al S3 (GPIO40, GPIO39 y GPIO42 están ocupados por el PCA9555 y no salen de Control) |
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

## Conectores de la raíz

| Conector | Señales |
|---|---|
| J4 / J5 (motor A / B, PH2.0 de 6 pines) | 1 M1 (AOUT1 / BOUT1), 2 GND encoder, 3 C1 (ENC_A1 / ENC_B1), 4 C2 (ENC_A2 / ENC_B2), 5 VCC encoder (3V3), 6 M2 (AOUT2 / BOUT2) |
| J7 / J8 / J9 (servos) | GND, 5V, SERVO_DIR (GPIO8) / SERVO_1 (GPIO9) / SERVO_2 (GPIO14) |
| J11 (LiDAR) | 1 LIDAR_RX (GPIO35), 2 control (R24 a GND), 3 GND, 4 +5V (consume unos 300 mA) |
| J16 (tira WS2812) | GND, 5V, TIRA_LED_5V |
| J2 (UART0) | 1 TXD0 (GPIO43), 2 RXD0 (GPIO44), 3 GND |
| J12 (nRF24, socket 2x4) | 1 GND, 2 NRF_VCC, 3 CE (EXP_GPIO0), 4 CSN (GPIO10), 5 SCK (GPIO12), 6 MOSI (GPIO11), 7 MISO (GPIO13), 8 IRQ (EXP_GPIO1) |
| J17 (LEDs RGB ruedas) | 1 VLED, 2 R, 3 G, 4 B |
| J3 (USB-C, hoja `USB-C`) | D+ = GPIO20, D- = GPIO19, CC con 5,1 kΩ a GND, VBUS solo para el ESD |

## Actualización: GPIO de la interfaz externa

ENC_A1/A2/B1/B2 = GPIO4/5/6/17; SERVO_DIR/1/2 = GPIO1/36/37; NRF_MISO/MOSI/SCK/CSN = GPIO21/18/38/41; LED de estado = EXP_GPIO6 a 9. Esta sección sustituye a las asignaciones anteriores de esas señales. J20 pierde GPIO36 y GPIO37 (pines 7 y 8 sin conexión).

## Actualización: servos, LiDAR, radio y USB

SERVO_DIR/1/2 = GPIO21/35/36; LIDAR_RX = GPIO37; LIDAR_PWM = EXP_GPIO3; NRF_MISO = GPIO12, NRF_MOSI = GPIO41, NRF_SCK = GPIO38, NRF_CSN = EXP_GPIO2, NRF_CE = EXP_GPIO0, NRF_IRQ = EXP_GPIO1; USB nativo (JTAG) = GPIO19 y GPIO20; USB UART (CH340C) = GPIO43 (TX del S3), GPIO44 (RX del S3), con EN y GPIO0 para el auto-reset.

## Asignación vigente tras la revisión del remapeo

Motores: ENC_A1/A2 = GPIO15/16, ENC_B1/B2 = GPIO13/14; driver DRV8424: AEN = GPIO1, APH = GPIO2, BEN = GPIO4, BPH = GPIO5, nSLEEP = GPIO6, nFAULT = GPIO7. Faros: TIRA = GPIO8, INT_DER = GPIO9, INT_IZQ = GPIO10, LUCES = GPIO11. Servos: SERVO_DIR = GPIO12, SERVO_1 = GPIO35, SERVO_2 = GPIO41. GPS: I2C GPIO17/18, UART GPIO47/48. LiDAR: RX = GPIO21, PWM = EXP_GPIO7. Radio: MOSI = GPIO36, MISO = GPIO37, SCK = GPIO38, CSN = EXP_GPIO6, CE = EXP_GPIO0, IRQ = EXP_GPIO1. USB nativo = GPIO19/20; USB UART = GPIO43/44. LED de estado = EXP_GPIO2 a 5. Esta sección sustituye a las asignaciones anteriores.
