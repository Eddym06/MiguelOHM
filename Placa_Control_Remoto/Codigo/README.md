# Código del control remoto (KAKATA RC433)

Firmware PlatformIO (Arduino) para el ESP32-S3-WROOM-1U-N16R2 del PCB `KAKATA_RC433_V1`.
Compilar y subir: `pio run -t upload` (el USB nativo IO19/IO20 hace de puerto serie).

## Controles

| Control | Acción |
|---|---|
| JOY0 (eje vertical, IO4) | Acelerar / retroceder, proporcional a lo que se mueve |
| JOY1 (eje horizontal, IO1) | Giro; el servo del carrito sigue este valor |
| Botón JOY1 (IO46) | Alterna **MANUAL** ↔ **GYRO** (control solo por inclinación) |
| BTN1 (IO9) | Faros delanteros on/off |
| BTN3 (IO12) | Corto: siguiente color RGB. 2 s: apaga/enciende los RGB |
| BTN4 (IO11) | Corto: siguiente pantalla. 2 s: apaga/enciende el OLED |
| Botón JOY0 (IO3) | **Freno motor** mientras se mantiene presionado (el OLED muestra FRENO) |
| BTN2, BTNL1-4 | Sin uso por ahora |

En modo GYRO la posición de la mano al activarlo es el neutro: inclinar hacia adelante/atrás
acelera/retrocede y a los lados gira (zona muerta 6°, máximo a 30°).

## Pantallas del OLED

Barra superior fija: icono wifi (enlace RF; sin enlace parpadea con una barra diagonal),
modo (MANUAL/GYRO) y batería con porcentaje.

0. **Tablero**: velocidad, volante que gira con el servo, barra de acelerador, chips FARO/RGB.
1. **Velocímetro**: aguja tipo odómetro.
2. **Velocidad + RPM**: velocímetro y cuentarrevoluciones lado a lado (RPM máx. 1000, zona roja el último 20 %).
3. **Controles**: posición de los ejes y valores en %.
4. **Inclinación**: horizonte artificial con roll/pitch del MPU-6050.
5. **Enlace RF**: TX/RX/errores, calidad de señal, batería del control y del carrito.

Los cambios (faros, color, modo) muestran un aviso emergente breve.

## Configuración guardada

Al apagar el control se conservan (memoria NVS, se guarda 1.5 s después del último cambio):
faros, RGB (on/off y color), pantalla actual y OLED on/off. El modo GYRO **no** se guarda:
siempre arranca en MANUAL por seguridad.

## LEDs del control

LED1 enlace (parpadea sin enlace) · LED2 modo GYRO · LED3 faros · LED4 RGB on · LED5 transmitiendo · LED6 batería baja.

## Enlace RF 433 MHz

Definido en [include/protocol.h](include/protocol.h). **Copiar ese archivo al proyecto del carrito.**
UART 4800 baud con la línea invertida, trama con preámbulo + sincronía + Manchester + CRC8.

- Control → carrito, 10 Hz (ciclo de 100 ms: comando + respuesta del carrito): acelerador (-100..100), giro (-100..100), flags (faros, RGB, GYRO, FRENO) y color.
  Siempre se envía el estado completo, así un paquete perdido se corrige con el siguiente.
- Carrito → control: debe responder con telemetría (velocidad km/h ×10, RPM ÷ 4, batería %) tras CADA comando (10 Hz), justo después de recibirlo.
  Sin telemetría por 1 s el control marca "desconectado".
- El carrito debe detener los motores si pasan >500 ms sin comando válido (failsafe).

## Supuestos a verificar en el hardware

- Batería de **1 celda Li-ion/LiPo** (4.2 V máx.). Si es otra, cambiar `batteryPercent()` en `main.cpp`.
  Ajustar `VBAT_CAL` en `config.h` comparando con un multímetro.
- Sentido de los ejes: si adelante/derecha salen invertidos, usar `JOY0_Y_INVERT`, `JOY1_X_INVERT`,
  `GYRO_INVERT_FWD`, `GYRO_INVERT_STEER` o `GYRO_SWAP_AXES` en `config.h`.
- `RF_EN_ACTIVE_HIGH`: se asume que el pin EN del transmisor es activo en alto.
- IO45 (LED1) e IO46 (botón JOY1) son pines de strapping; con el pull-up del botón y el LED por el 74HC04 no deberían afectar el arranque.
