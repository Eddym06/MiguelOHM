# Plan de Pruebas y Bring-Up — TESLA v2.7 (CIRCUITLY)

## 1. Inspección Visual y Pre-Requisitos
- Confirmar en pedido JLCPCB: **6 capas (`JLC06161H-3313`) + Matte Black + ENIG + VIPPO (`Epoxy Filled & Capped` IPC-4761 Type VII)** para `U1` (`DRV8424EPWPR`, 24 vías 0.20mm) y `D8` (`SS10U45`, 12 vías).
- Instalar **fusible automotriz DC externo de 10–15 A (`32 V DC`)** en serie en el cable positivo de la batería LiPo 3S hacia `J6`.

## 2. Pruebas en Frío (Sin Alimentación)
- `TP10 (VBAT)` vs `GND` `> 10 kΩ`; `+5V` vs `GND` `> 1 kΩ`; `+3V3` vs `GND` `> 1 kΩ`.
- Verificar que NO existe corto en `U1` (`DRV8424`) entre `AOUT1/AOUT2/BOUT1/BOUT2` y `GND`/`VBAT`.

## 3. Bring-Up Seguro (`≤ 6 W`: `12.0 V / 0.50 A`)
1. Alimentar `J6` con **`12.0 V / 0.50 A` (`6 W` máx)** sin motores ni servos conectados.
2. Verificar tensiones:
   - `TP10 (VBAT)` ≈ `11.7 V` (post-`D8 SS10U45`).
   - `+5V` (`U7 AP64500`) = **`5.00 V ± 2%`**.
   - `+3V3` (`U6 AP63200`, `R79=191k/R80=62k`) = **`3.265 V ± 2%`** (`D19 SMAJ5.0A` sin fuga).
   - `TP11 (VREF DRV8424)` (`R3=10k / R4=15k`) = **`1.98 V ± 2%`** (`I_trip = 1.50 A`).
   - `GPIO7 (nFAULT)` sin falla = **`3.30 V`** (`R1` eliminado; `V_IH > 2.475 V` PASS).
3. Conectar USB-C (`J3` nativo S3 y `J2` `CH340B`), verificar enumeración y consola serie. **Nota USB-only:** la topología heredada `D15/D16` alimenta `/Externo/VBAT` (`170 µF`) desde `VBUS`; al probar solo por USB-C (sin batería), mantener motores y tira LED desconectados para evitar corriente de *inrush* excesiva en el puerto USB del PC.
4. Probar BMI270 (`SPI`), PCA9555 (`I2C`), 3× Servos (`U19 VP = +3V3`), tira LED 5V (`U10` + `D28 PESD5V0S1BA`), faros `AL8861` y motores `DRV8424`.

## 4. Mapa de Exclusión de Conformal Coating (Keepout Map)
Enmascarar antes de barnizar: `J2/J3` (USB-C), `J5/J13/J6` (WAGO), `J7..J11/J15/J16/J20..J24`, `U2` (zócalo nRF24), pulsadores `SW1/SW4`, y pads `TP*`.
