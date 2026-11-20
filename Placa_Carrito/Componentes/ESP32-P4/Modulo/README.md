# Módulo ESP32-P4 (Waveshare / Spotpear), 25 x 25 mm castellated

Módulo comercial con el ESP32-P4, 32 MB de PSRAM en el encapsulado, 16 MB de flash Nor (QSPI) y un ESP32-C6 a bordo (Wi-Fi 6 y BT 5.3).
Con este módulo no hace falta diseñar la flash, los cristales, el buck de 1,2 V ni la radio C6.

| Archivo | Contenido |
|---|---|
| `Waveshare_P4.kicad_sym` | Símbolo `ESP32-P4-Module-N16R32` con los 88 pines. |
| `Waveshare_P4.pretty/ESP32-P4-Module.kicad_mod` | Huella de 88 pads (1,5 x 0,7 mm, paso 1,0 mm), cuerpo 25 x 25 mm. |
| `Waveshare_P4.3dshapes/esp32-p4-module_20250730.stp` | Modelo 3D del fabricante. |

## Origen de los datos

- Geometría de los pads: DXF «PCB Land Pattern» del fabricante (`ESP32-P4-Module-PCB-Land-Pattern.zip`).
- Nombres de pin: tabla «Pinout Description» del wiki del módulo (copia en spotpear.com; waveshare.com bloquea las descargas automáticas).
- Modelo 3D: `ESP32-P4-Module-3D.zip` del mismo wiki. Origen del STEP en la esquina inferior izquierda; el offset (-12,5 / -12,5 / 0) lo centra.

## Verificar antes de fabricar

1. **Numeración de los pads.** El DXF no trae los números. Se asumió el pad 1 junto al triángulo de la serigrafía del fabricante (arriba a la izquierda) y numeración antihoraria: izquierda 1-22, abajo 23-44, derecha 45-66, arriba 67-88. Compara con la figura «Pinout Definition» del wiki y con el módulo real.
2. **Pin 51.** La tabla del wiki dice «GPIO25 Ground pin»; el esquemático del fabricante lo da como GPIO25 y así quedó.
3. **Antena.** El módulo no trae antena impresa: el pad 2 (`LNA_OUT`) saca la señal del C6 (50 ohm). Conéctalo al conector u.FL (J1) con una pista de impedancia controlada, o pide la versión con IPEX.
4. **Alimentación.** `ESP_3V3` (pines 85 y 86) y `VBAT` (84) a 3,3 V; `ESP_EN` (87) ya trae pull-up en el módulo.
5. **Arranque.** GPIO35 y GPIO36 son los pines de strapping para modo descarga (ver nota del wiki).
