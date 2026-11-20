# Placa modular ESP32-S3 + ESP32-P4

Proyecto KiCad nuevo (`Placa_Modular_S3_P4.kicad_pro`). Lleva los dos módulos en una sola placa con todos sus pines cableados a conectores, para usarla como cerebro de proyectos futuros. No está enrutada: solo esquemático (y un PCB vacío).
Todo va cableado: la raíz no tiene etiquetas y la hoja de control solo usa etiquetas jerárquicas para los dos VBUS.

| Hoja | Contenido |
|---|---|
| `Alimentacion` | Entrada de 5-24 V (J1) o USB-C, con tres diodos SS14 en OR, y buck AP63203 a 3,3 V / 2 A. |
| `Control ESP32` | Módulo P4 Waveshare (U4) y S3-WROOM-1 N16R8 (U5), reset y BOOT, enlace SPI entre ellos (P4 maestro GPIO26-30, S3 esclavo GPIO12/11/13/10/14), dos USB-C con ESD y todos los conectores. |

| Conector | Pines |
|---|---|
| J4 | P4: GPIO0-13, 20-23, 31-33 |
| J5 | P4: GPIO39-54 y los de strapping 34/37/38 (sin uso permanente) |
| J6 | P4: MIPI DSI/CSI, USB HS, ESP_LDO_VO4 y los pines del C6 |
| J7 | S3: GPIO1-9, 15-18, 21, 38-48 (3/45/46 son de strapping) |
| J8 | u.FL con la antena del C6 (LNA_OUT) |

- No salen: GPIO24/25 del P4 y GPIO19/20 del S3 (USB-C), GPIO35/36 del P4 (BOOT y su pull-up) y GPIO35-37 del S3 (PSRAM octal).
- Los GPIO39-48 del P4 salen a 1,8 V hasta que el firmware sube `ESP_LDO_VO4` a 3,3 V. GPIO0/1 pueden tener el cristal de 32 kHz del módulo.
- Librerías: `../Componentes` (símbolos, huellas y 3D de ambos módulos).
