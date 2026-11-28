# Datasheets

Carpeta solo para datasheets (y notas de pines de módulos sin datasheet) de lo que usa el carrito Tesla (`Placa_Carrito/Tesla`). Los PDF se descargaron de la página del fabricante o de una copia pública; antes de pedir piezas, confirma que la revisión sea la vigente.

## Descargados

| Componente (referencia en la placa) | Archivo |
|---|---|
| ESP32-S3-WROOM-1 N16R2 (U3) | `ESP32-S3-WROOM-1_datasheet_v1.8.pdf` |
| PCA9555 expansor I2C (U14) | `PCA9555_datasheet_TI.pdf` |
| DRV8424 driver de motores (U1) | `DRV8424_TI.pdf` |
| AP63200 buck ajustable de 3,3 V (U6); el mismo PDF cubre AP63200, AP63201, AP63203 y AP63205 | `AP63203_AP63205_Diodes.pdf` |
| AP64501 buck de 5 V y 5 A (anterior, sin existencias) | `AP64501_Diodes.pdf` |
| AP64500 buck de 5 V y 5 A (U7) | `AP64500_Diodes.pdf` |
| AL8860 driver de LED (anterior) | `AL8860_Diodes.pdf` |
| AL8861 driver de LED de los faros e intermitentes (U9, U15, U16) | `AL8861_Diodes.pdf` |
| CH340C puente USB a UART (U17) | `CH340C_WCH.pdf` |
| WAGO 2060-453 borne de resorte SMD (J5, J13) | `WAGO_2060-453.pdf` |
| 74AHCT1G125 adaptador de nivel de la tira (U10) | `74AHCT1G125_TI.pdf` (SN74AHCT1G125, equivalente al de Nexperia) |
| ULN2003A driver de LEDs RGB (U11) | `ULN2003A_TI.pdf` |
| SUD50P06-15 MOSFET de protección de polaridad (Q1) | `SUD50P06-15_Vishay.pdf` |
| SS14 diodo Schottky (D7) | `SS14_Vishay.pdf` (serie SS12 a SS16) |
| SS10U45 Schottky de protección contra batería al revés (D8) | `SS10U45_Yangjie.pdf` |
| nRF24L01+ radio (J12) | `nRF24L01P_Nordic.pdf` |
| MPU-6050 IMU en módulo GY-521 (J15) | `MPU6050_InvenSense.pdf` |
| WS2812B tira de LED (J16) | `WS2812B.pdf` |
| Servo SG90 (J7, J8, J9) | `SG90.pdf` |

## Motor JGB37-520 (J4, J5): sin datasheet del fabricante

Es el cable de 6 hilos con conector PH2.0 del motor con encoder (12 V, 800 RPM). Orden de pines según las imágenes del vendedor:

| Pin | Señal | Color del cable |
|---|---|---|
| 1 | M1, motor + | rojo |
| 2 | GND del encoder | negro |
| 3 | C1, fase A | amarillo |
| 4 | C2, fase B | verde |
| 5 | VCC del encoder (3,3 a 5 V) | azul |
| 6 | M2, motor - | blanco |

- Encoder Hall AB de 11 pulsos por vuelta del motor, salida cuadrada con pull-up integrado (se conecta directo al MCU), respuesta de 100 kHz.
- Intercambiar M1 y M2 invierte el giro.
- En la placa el encoder va a 3,3 V.
- Nunca conectar los 12 V del motor al VCC del encoder.
- Si al girar el motor A y B salen intercambiados, se corrige en el firmware (los colores y el orden pueden variar entre lotes; comprobar con el motor).

## Pendientes (no se pudo bajar el PDF)

| Componente | Dónde buscarlo |
|---|---|
| CH340C puente USB a UART (U17) | LCSC C84681, hoja del fabricante WCH (wch-ic.com) |
| MMBT3904 transistor NPN SOT-23 (Q1, Q2) | cualquier fabricante (Nexperia, onsemi) |
| USBLC6-2SC6 (U13, U18) | página del producto en st.com, código de documento cd00050750 |
| SMBJ15CA y SMBJ5.0CA (D8, D9, D10) | Littelfuse o Diodes Inc., serie SMBJ |
| Conectores JST PH (J4, J5), SH (J10) y ZH (J11) | catálogo ePH, eSH y eZH en jst-mfg.com |
| Molex KK-254 serie 6410 (J7 a J9, J16, J17) | dibujo de ventas de la referencia 22-23-2031 en molex.com |
| XT60 (J6) | Amass, XT60PW-M |
| Pulsador TL3342 (SW1, SW4) | E-Switch |
| Inductores Bourns SRN6045TA (L7) y Taiyo Yuden serie NR50 (L5, L6) | páginas de Bourns y Taiyo Yuden |
| Fusible SMD 2920 de 10 A (F1) | Littelfuse serie 2920 |
| Receptáculo USB-C HRO TYPE-C-31-M-12 (J3) | HRO / LCSC |
| GPS M100 Pro (J10) y LiDAR LDROBOT D500 (J11) | manual del fabricante (sin PDF público estable) |
