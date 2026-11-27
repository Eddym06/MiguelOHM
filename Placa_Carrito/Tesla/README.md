# Placa Tesla (carrito): guía del diseño

PCB del carrito con **un solo ESP32-S3** (módulo WROOM-1 N16R2) y un expansor de pines **PCA9555**. Este documento explica cómo está organizado el esquemático, qué cambió respecto a las versiones anteriores y, sobre todo, **qué implica para el código** (para Eddy).

Archivos relacionados:

| Archivo | Contenido |
|---|---|
| [ASIGNACION_PINES.md](ASIGNACION_PINES.md) | Qué va en cada GPIO del S3, en cada pin del PCA9555 y en el conector J20. |
| [CAMBIOS.md](CAMBIOS.md) | Registro detallado de los cambios de esta revisión (qué, por qué y cómo verificarlo). |
| [Tesla_BOM.csv](Tesla_BOM.csv) | Lista de materiales. |
| `Tesla.kicad_pro` | Proyecto de KiCad 10 (abrirlo desde aquí). |

## Estructura del esquemático

La hoja raíz (`Tesla.kicad_sch`) tiene dos cosas: los **conectores externos**, cada uno en su cuadro y unido con cable directo (sin etiquetas) a los pines de la hoja `Externo`, y la hoja `USB-C`. Todo lo demás vive dentro de `Externo`, donde las hojas se unen por etiquetas. Los pines de `Externo` se llaman como la señal (`SERVO_1`, `ENC_A1`, `NRF_CE`...) y por dentro se unen a la red `GPIOn` o `EXP_GPIOn` del bloque de control.

**En la raíz (lo que el usuario toca):**

| Cuadro | Conectores |
|---|---|
| Batería | J6 (borne WAGO 2604-1102, 12 V) |
| Motores | J4 y J5, un conector PH2.0 de 6 pines por motor JGB37-520 (motor y encoder juntos) |
| GPS e IMU | J10 (GPS M100 Pro) y J15 (GY-521) |
| Servos | J7 (dirección), J8 y J9 |
| LiDAR | J11 y R24 (pull-down de la señal de control) |
| Tira LED | J16 (WS2812) |
| Radio nRF24L01+ | U2 (módulo nRF24L01 con conector, fuera de la placa principal) |
| Luces | J21 y J24 (faros, uno por LED), J22 y J23 (intermitentes izquierdo y derecho): un conector PH2.0 de 2 pines por canal para los LED de 1 W |
| Expansión | J20 (socket hembra 1x16) |
| USB-C | hoja `usb.kicad_sch`: J3, protección ESD U13 y R61 y R62 (CC) |

**Dentro de `Externo`:**

| Hoja | Archivo | Contenido |
|---|---|---|
| Alimentacion | `alimentacion.kicad_sch` | Contenedor de: protección de batería (`alim_proteccion`), medición (`alim_medicion`) y dos bucks (`alim_buck_3v3` y `alim_buck_5v`; el de 5 V incluye el filtro del LiDAR). |
| Control ESP32 | `control.kicad_sch` | **Solo** el ESP32-S3 con su circuito mínimo (reset, BOOT, filtrado de 3V3) y el PCA9555 con su filtrado. Incluye los pull-ups que exige el PCA9555 en SCL, SDA e INT (4,7 kΩ, 4,7 kΩ y 10 kΩ). Los pull-downs de las salidas no van aquí: viven junto a la carga que protegen. |
| Drivers motores | `motores.kicad_sch` | DRV8424 y pull-downs de arranque de APH, BPH y nSleep. |
| Faros | `faros.kicad_sch` | Drivers del alumbrado (faros e intermitentes, AL8860; los LED van afuera) y adaptador 3,3 V a 5 V para la tira WS2812B (J16). |

Reglas de dibujo aplicadas a todas las hojas:

- En la raíz, cada señal va con cable directo del pin de `Externo` al pin del conector. Entre ICs y hojas internas se usan etiquetas (jerárquicas en el borde de cada hoja, globales entre pines internos, locales dentro de una hoja). Los cables solo unen un pasivo con su pin.
- Cada pasivo se conecta con cable corto, componente, cable corto y símbolo de alimentación. Nada se conecta directo a un nodo.
- Las flechas de 3V3, 5V y VM apuntan hacia arriba o de lado, nunca hacia abajo, y muestran su nombre.
- Los valores de componentes verticales van girados 90° junto al cuerpo.
- Cada grupo funcional va dentro de un marco punteado con título.
- Un solo GND en todo el proyecto.
- Las librerías oficiales de Espressif no se modifican.

### Conector de los motores (JGB37-520, J4 y J5)

Orden de pines del cable de 6 hilos con PH2.0 (de las imágenes del vendedor; comprobar con el motor):

| Pin | Señal | Cable |
|---|---|---|
| 1 | M1, motor + | rojo |
| 2 | GND del encoder | negro |
| 3 | C1, fase A | amarillo |
| 4 | C2, fase B | verde |
| 5 | VCC del encoder (3V3) | azul |
| 6 | M2, motor - | blanco |

Intercambiar M1 y M2 invierte el giro. El encoder (11 PPR, 3,3 a 5 V) se alimenta a 3,3 V; nunca conectar los 12 V del motor al VCC del encoder.

## Qué cambió y por qué importa al firmware

1. **Un solo microcontrolador.** El P4 salió de esta placa (vive en `../Placa_Modular_S3_P4`). Control, WiFi y web corren en el S3. El firmware actual de `Codigo Carrito/` está pensado para P4 más S3; hay que portarlo a un único S3 (tabla de pines en [ASIGNACION_PINES.md](ASIGNACION_PINES.md)).
2. **Pines de arranque sin uso.** GPIO3, GPIO45 y GPIO46 quedan sin conectar. GPIO0 es solo el botón BOOT: no se puede usar como entrada o salida del programa.
3. **Los nombres de los pines del bloque de control son GPIO.** En el firmware sigue habiendo nombres funcionales (`PIN_ENC_L_A`, etc.); la equivalencia con el GPIO está en la tabla de `ASIGNACION_PINES.md`.
4. **Expansor PCA9555.** Las señales lentas (CE e IRQ del nRF24, dirección y sleep de los motores, fallo del driver, LEDs de estado) ya no son pines del S3: se leen y escriben por I2C (ver siguiente sección).
5. **Bus I2C compartido** (GPIO39 SCL, GPIO40 SDA, pull-ups de 4,7 kΩ): GPS, IMU, INA219 y PCA9555 (0x20). **GPIO39, GPIO40 y GPIO42 (INT) no son pines libres** y no salen de la hoja de control. Por eso el GPS, la IMU, el INA219 y los pines I2C de J20 **no tienen bus I2C hacia el S3 todavía** (decisión pendiente: ver Pendiente).
6. **Radio nRF24** por SPI en GPIO10 (CSN), GPIO11 (MOSI), GPIO12 (SCK), GPIO13 (MISO). CE e IRQ van por el expansor.
7. **USB-C del S3** (hoja `USB-C`) para programar, JTAG y consola (GPIO19 D-, GPIO20 D+). UART0 (GPIO43 TX, GPIO44 RX) va a un segundo USB-C (J2, hoja `USB-UART`) con puente CH340C y auto-reset por DTR y RTS (EN y GPIO0).
8. **Motores y encoders** salen por un solo conector de 6 pines por motor (J4 y J5), con el encoder a 3,3 V.

## Programar el expansor PCA9555

Los 16 pines `EXP_GPIO0` a `EXP_GPIO15` no son GPIO del S3: son dos puertos de 8 bits que se controlan por I2C con el chip en la dirección **0x20** (A0, A1 y A2 a GND). Un bit del puerto 0 es `EXP_GPIO0` a `7`; uno del puerto 1 es `EXP_GPIO8` a `15`.

Registros (el primer byte de cada transferencia elige el registro):

| Reg | Función |
|---|---|
| 0x00 / 0x01 | Leer puerto 0 / puerto 1 (entradas) |
| 0x02 / 0x03 | Escribir salidas del puerto 0 / 1 |
| 0x04 / 0x05 | Invertir polaridad (se deja en 0) |
| 0x06 / 0x07 | Configuración del puerto 0 / 1: **1 = entrada, 0 = salida** |

Al encender, todos los pines son entradas con un pull-up interno de unos 100 kΩ. Por eso APH, BPH, nSleep y CE tienen pull-down de 10 kΩ: el driver arranca dormido y el nRF24 con CE en bajo.

Uso de cada pin en esta placa:

| Pin | Señal | Dirección |
|---|---|---|
| `EXP_GPIO0` | NRF_CE | salida |
| `EXP_GPIO1` | NRF_IRQ | entrada |
| `EXP_GPIO2`, `3`, `4` | APH, BPH, nSleep del DRV8424 | salida |
| `EXP_GPIO5` | nFAULT del DRV8424 | entrada |
| `EXP_GPIO6`, `7` | LED_AP, LED_AN | salida, **activos en bajo** |
| `EXP_GPIO8`, `9` | LED_BP, LED_BN | salida, **activos en bajo** |
| `EXP_GPIO10` a `15` | libres, salen por J20 | a elección |

Valores iniciales recomendados (se escriben antes de configurar la dirección, para que no haya pulsos): puerto 0 salidas en `0xC0` y puerto 1 en `0x03` (todo en bajo salvo los LEDs, que en alto quedan apagados). Configuración: puerto 0 en `0x22` y puerto 1 en `0xFC`.

Ejemplo con ESP-IDF 5.x (sin compilar; sirve de guía):

```c
#include "driver/i2c_master.h"

static i2c_master_dev_handle_t pca;

static void pca_write(uint8_t reg, uint8_t val) {
    uint8_t b[2] = { reg, val };
    i2c_master_transmit(pca, b, 2, 100);
}
static uint8_t pca_read(uint8_t reg) {
    uint8_t v = 0;
    i2c_master_transmit_receive(pca, &reg, 1, &v, 1, 100);
    return v;
}

void pca_init(void) {
    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t bc = {
        .i2c_port = I2C_NUM_0, .sda_io_num = GPIO_NUM_40, .scl_io_num = GPIO_NUM_39,
        .clk_source = I2C_CLK_SRC_DEFAULT, .glitch_ignore_cnt = 7,
    };
    i2c_new_master_bus(&bc, &bus);
    i2c_device_config_t dc = { .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                               .device_address = 0x20, .scl_speed_hz = 400000 };
    i2c_master_bus_add_device(bus, &dc, &pca);

    pca_write(0x02, 0xC0);   // CE/APH/BPH/nSleep en 0, LEDs apagados
    pca_write(0x03, 0x03);   // LEDs del puerto 1 apagados
    pca_write(0x06, 0x22);   // puerto 0: bits 1 (IRQ) y 5 (nFAULT) son entradas
    pca_write(0x07, 0xFC);   // puerto 1: solo los bits 0 y 1 son salidas
}
```

Para cambiar un solo pin se guarda una copia del registro de salida en una variable y se modifica solo ese bit (por ejemplo `out0 |= 1 << 4; pca_write(0x02, out0);`). Para leer el fallo del driver: `pca_read(0x00) & (1 << 5)`.

Interrupción: `INT` va a **GPIO42** y baja cuando cambia alguna entrada. Se apaga al leer los registros 0x00 y 0x01. Configurar GPIO42 como entrada con flanco de bajada y leer los dos puertos en el manejador.

Límites: cada acceso tarda unos 100 µs a 400 kHz. Sirve para CE, dirección, sleep y LEDs; **no** para PWM ni señales rápidas (AEN y BEN, los PWM de los motores, están en pines nativos del S3).

## Verificación contra los datasheets

Se contrastó el diseño con el datasheet del ESP32-S3-WROOM-1 v1.8 y el del PCA9555 de TI. Detalle en [ASIGNACION_PINES.md](ASIGNACION_PINES.md#verificación-contra-los-datasheets). Correcciones hechas: C131 de 10 a 22 µF (filtrado de 3V3 del módulo) y R54 de 100 kΩ a 10 kΩ (pull-down de CE frente al pull-up interno del PCA9555).

## Pendiente

- En KiCad: **Actualizar PCB desde esquema** con la opción de actualizar huellas. Cambiaron J4 y J5 (ahora PH2.0 de 6 pines), J13 y J14 desaparecieron, J20 pasa a socket hembra y los nombres de red cambiaron con la hoja `Externo` y la hoja `USB-C`. Hasta entonces el DRC con paridad marca conflictos de red.
- Decidir cómo llega el I2C al GPS, la IMU y el INA219 (GPIO39 y GPIO40 están ocupados por el PCA9555 y no salen de Control).
- Rutear la PCB (hoy solo tiene las huellas colocadas) y revisar las reglas del DRC de U1 (anillos y taladros de las vías internas).
- Portar el firmware a un único S3 (`Codigo Carrito/`), con el driver del PCA9555 de arriba.
- Datasheets: carpeta `Datasheets/` en la raíz del repositorio (faltan algunos conectores, el GPS y el LiDAR; ver su README).

## Alimentación en la jerarquía (entradas y salidas de rieles)

- La hoja `Alimentacion` **saca** `VM`, `+3V3`, `+5V` y `GND` (bloque "Salidas de alimentacion"; el buck de 3V3, el de 5 V, la medición y la protección reciben `GND` y `VM` por pines).
- Cada hoja consumidora **recibe** solo los rieles que usa en su bloque "Entradas de alimentacion" (etiqueta jerárquica, un cablecito con giro y el símbolo de alimentación): Control (`+3V3`, `GND`), Drivers motores y Faros (`VM`, `GND`), Tira LED (`+5V`, `GND`), LEDs RGB (`+3V3`, `+5V`, `VM`, `GND`).
- En `Externo` cada pin de riel lleva una etiqueta global del mismo nombre, así todos los `GND`, `+3V3`, `+5V` y `VM` quedan unidos en toda la jerarquía. Sigue habiendo un solo `GND`.
- Los conectores de la raíz traen sus propios símbolos de alimentación (+5V, GND, +3V3...), que también son globales.

## Placa (PCB)

La placa ya tiene contorno de 130 x 170 mm, 4 capas, clases de red, zonas de cobre, zonas prohibidas de las antenas y todas las piezas colocadas (sin rutar). El detalle de la colocación, los cálculos, los cambios de piezas por existencias en JLCPCB y lo pendiente está en [PCB_NOTAS.md](PCB_NOTAS.md). La lista de piezas con códigos LCSC, existencias y precios está en `Tesla_BOM.csv` y `Tesla_BOM_JLCPCB.csv`. Se regenera con `herramientas/pcb_build.py`.
