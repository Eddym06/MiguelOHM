# Código del carrito (KAKATA)

Firmware ESP-IDF para los dos cerebros del carrito. Sin Arduino ni PlatformIO.

```
                 433 MHz                         SPI (50 Hz)                 WiFi propio
 Control remoto <-------> ESP32-P4  <=========================> ESP32-S3 <-------------> Celular
 (Placa_Control_Remoto/Codigo)   (cerebro)                          (puerta de enlace)        (página web)
                           · motores + PID + encoders
                           · servo, luces, tira del aleron
                           · BMI160, INA219, brújula, GPS, LIDAR
                           · navegación autónoma
```

| Carpeta | Contenido |
|---|---|
| `p4/` | Proyecto ESP-IDF del **ESP32-P4** (todo el tiempo real). |
| `s3/` | Proyecto ESP-IDF del **ESP32-S3** (WiFi, servidor web, página en `s3/main/web/index.html`). |
| `shared/` | Componente común: protocolo 433, enlace SPI P4↔S3 y tabla de ajustes. |
| `tests/` | Pruebas en la PC de la lógica pura (PID, mezcla, parsers, navegación…). |

## Compilar

Probado con **ESP-IDF 5.5.4 (P4)** y **5.3.1 (S3)**. En Windows, si el compilador falla con "Memoria insuficiente", limita los trabajos en paralelo: `ninja -j 2` dentro de `build/`.

```powershell
# P4  (ESP-IDF 5.5 o mayor)
cd p4;  idf.py set-target esp32p4;  idf.py build flash monitor
# S3  (ESP-IDF 5.3 o mayor)
cd s3;  idf.py set-target esp32s3;  idf.py build flash monitor
```

**ESP32-P4 revisión <3.0:** ESP-IDF 5.5 compila por defecto para chips ≥3.0 (v3.01). Si el log de arranque dice otra revisión, descomenta/agrega en `p4/sdkconfig.defaults` las dos líneas `ESP32P4_SELECTS_REV_LESS_V3` / `ESP32P4_REV_MIN_100` y repite `set-target`.

**Pruebas en la PC** (97 verificaciones: protocolo, SPI, PID, mezcla, NMEA, LIDAR, rumbo, navegación, tira trasera):
`bash tests/run_host_tests.sh`

## Pines: PENDIENTES DE LA PCB

Todos están en `-1` y **no hay que tocar nada más que estos dos archivos** cuando tu compañero tenga la PCB. Un módulo con algún pin en `-1` se desactiva y avisa en el log; el resto sigue funcionando.

**`p4/main/board_pins.h`**

| Función | Pin(es) | GPIO |
|---|---|---|
| Radio 433 | `PIN_RF_TX`, `PIN_RF_RX`, `PIN_RF_EN` | |
| Motor izquierdo (L298N canal A) | `PIN_MOT_L_EN` (PWM 5 kHz), `PIN_MOT_L_IN1`, `PIN_MOT_L_IN2` | |
| Motor derecho (canal B) | `PIN_MOT_R_EN`, `PIN_MOT_R_IN1`, `PIN_MOT_R_IN2` | |
| Encoders (cuadratura) | `PIN_ENC_L_A`, `PIN_ENC_L_B`, `PIN_ENC_R_A`, `PIN_ENC_R_B` | |
| Servo MG90 | `PIN_SERVO` | |
| Faros (base del NPN) | `PIN_HEADLIGHTS` | |
| RGB de las ruedas | `PIN_RGB_R`, `PIN_RGB_G`, `PIN_RGB_B` (+ `RGB_COMMON_ANODE`) | |
| Tira WS2812 del aleron | `PIN_SPOILER` | |
| I²C (BMI160, INA219, brújula) | `PIN_I2C_SDA`, `PIN_I2C_SCL` | |
| GPS | `PIN_GPS_RX` (y opcional `PIN_GPS_TX`) | |
| LIDAR | `PIN_LIDAR_RX` | |
| SPI al S3 (maestro) | `PIN_SPI_SCLK`, `PIN_SPI_MOSI`, `PIN_SPI_MISO`, `PIN_SPI_CS`, `PIN_SPI_HS` (entrada) | |

**`s3/main/s3_config.h`**

| Función | Pin(es) | GPIO |
|---|---|---|
| SPI al P4 (esclavo) | `PIN_SPI_SCLK`, `PIN_SPI_MOSI`, `PIN_SPI_MISO`, `PIN_SPI_CS`, `PIN_SPI_HS` (salida) | |

Cruce del SPI: SCLK↔SCLK, CS↔CS, MOSI(P4)→MOSI(S3), MISO(S3)→MISO(P4). `HS` es "datos listos" (S3→P4): recomendado, evita perder tramas.
Direcciones I²C (cambiables en `board_pins.h`): BMI160 `0x68`, INA219 `0x40`, brújula autodetectada (IST8310 `0x0E`/`0x0C`, QMC5883L `0x0D`, HMC5883L `0x1E`).

## Cómo funciona

**Exclusión 433 MHz ↔ WiFi.** El S3 enciende su WiFi (`KAKATA-CARRITO` / `kakata1234`, página en `http://192.168.4.1`) al arrancar. El P4 decide:
- Llega un comando del control por 433 primero → modo 433, el P4 le avisa al S3 y este **apaga el WiFi**.
- Un celular se conecta al WiFi primero → modo web, el P4 **apaga el radio 433** (EN en bajo y descarta lo recibido).
- 3 s sin comandos 433, o 1 s sin celular, y ambos vuelven a estar disponibles. Nunca están activos a la vez.

**Enlace 433 MHz** (`shared/include/rc433_protocol.h`, idéntico al del control): ciclo de 100 ms, el control manda el comando (~33 ms de aire) y el carrito responde **de inmediato** con la telemetría (velocidad, RPM/4, batería). Comandos: acelerador, giro, faros, RGB y color, GYRO (solo informativo), **FRENO**.

**Tracción** (`p4/main/drive.c`):
- Acelerador y giro → RPM objetivo de cada rueda; el giro se hace con el **servo y también con diferencia de velocidad** entre motores (`Giro por motor`, 0.35 por defecto).
- **PID por rueda** con avance (feed-forward) y anti-windup, más una **sincronía cruzada** que corrige a una rueda si se adelanta a la otra respecto a lo pedido.
- Rampa de aceleración, PWM mínimo para vencer la fricción, y protección para no invertir el motor "a la fuerza" si aún gira al revés.
- **Freno motor**: `IN1=IN2=1` con EN al 100 % mientras se mantiene el botón de JOY0.
- **Acelerador suelto → rueda libre** (`IN=00`, EN=0): desacelera por inercia.
- **Failsafe**: sin comandos por 500 ms los motores quedan en rueda libre.
- Si un encoder no cuenta o está invertido, esa rueda pasa a lazo abierto y se avisa en el log.

**Luces** (`drivers/lights.c`): faros por NPN; RGB de ruedas por PWM (9 colores, el 9.º es arcoíris animado); **tira WS2812 estilo F1**: al acelerar se llena de afuera hacia el centro (verde → rojo → azul), en reversa en blanco, al frenar toda en rojo, en reposo una luz de posición tenue.

**Sensores:** BMI160 (inclinación y giro), brújula del GPS, INA219 (batería, % para Li-ion de `Celdas` S), GPS M100 Pro (NMEA, prueba varios baudios hasta encontrar tramas), LIDAR D6 (protocolo LDROBOT serie D, 36 sectores de 10°). El rumbo mezcla giroscopio y brújula.

**Navegación autónoma:** destino GPS → rumbo y distancia, giro proporcional al error de rumbo, velocidad que baja en curvas y cerca del destino. Con el LIDAR esquiva obstáculos (repulsión lateral y reducción de velocidad) y, si hay algo a menos de `Distancia de parada`, retrocede girando hacia el lado más libre. Se cancela si se toca el acelerador o el freno, se pierde el WiFi, o el GPS falta 5 s.

**Página web** (`s3/main/web/index.html`, un solo archivo sin dependencias):
- **Control:** dos mandos táctiles (acelerador vertical, giro horizontal), freno, faros, RGB y colores.
- **Mapa:** radio de 10, 15 o 20 m, brújula, rastro, puntos del LIDAR; tocar el mapa marca el destino, «IR AL PUNTO» lo envía. Funciona **sin internet** (cuadrícula local); con la casilla «Mapa en línea» carga mosaicos de OpenStreetMap si el celular tiene datos.
- **Ajustes:** los 38 valores de `shared/include/settings_table.h` (servo, PID, giro por motor, rampa, failsafe, brillo, navegación, LIDAR…), guardados en NVS.
- **Pruebas:** motores (lazo abierto y PID), servo, luces, tira trasera, calibración de brújula y giroscopio, y lectura en vivo de todos los sensores.

## Sin validar en hardware (revisar al tener la PCB)

La compilación de ambos proyectos y las pruebas de la lógica pasan, pero **nada se ha probado con el carrito real**:

- **Sentido de ejes y motores:** `Motor/Encoder: invertir`, `Servo: invertir` y `Brújula: invertir/giro` en Ajustes. El signo del giroscopio asume el BMI160 plano con Z hacia arriba; comprobar en Pruebas que el rumbo **crece al girar en sentido horario**.
- **Encoders:** `Cuentas por vuelta` = 440 es un valor de ejemplo (11 pulsos × 4 × reducción 10). Ajustar a tu reducción real, y `Diámetro de rueda` para los km/h. RPM máx. = 1000.
- **PID:** ganancias iniciales razonables, pero hay que afinarlas con el carrito en el suelo.
- **GPS M100 Pro:** se asume NMEA por UART. Si no llegan tramas, revisar baudios y que el modo NMEA esté activo. La brújula se autodetecta entre IST8310/QMC5883L/HMC5883L; si no aparece, el log lo dice.
- **LIDAR D6:** se asume el protocolo LDROBOT (0x54 0x2C, 230400 baudios). Si el D6 usa otro, solo cambia `ld_parser.c`. Orientación en `LIDAR: giro de montaje / invertir`.
- **INA219:** shunt de 100 mΩ (max ~3.2 A). Si el módulo trae otro, `INA219_SHUNT_MOHM`; con motores grandes puede saturar.
- **Batería:** se asume Li-ion/LiPo de `Celdas` S (3 por defecto = 12 V). Para plomo u otra química, cambiar `battery.c`.
- **Radio 433:** 4800 baud con Manchester (~2400 bps útiles) es conservador para módulos ASK baratos; el EN del transmisor se asume activo en alto.
- **Autónomo:** probar primero con el carrito en alto o en espacio abierto y a baja velocidad; el LIDAR no cubre la parte trasera al retroceder.
- **P4:** el soporte de ESP32-P4 en ESP-IDF es reciente; revisar la revisión del chip (ver arriba).
