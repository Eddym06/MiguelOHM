# ECO Log — TESLA (Repo Miguel OHM / CIRCUITLY)

| Rev | Fecha | Cambios de Ingeniería |
|---|---|---|
| **v2.5** | 2026-11-27 | 6 capas `JLC06161H-3313`, ESP32-S3, BMI270, DRV8424, CH340B, SRV05/USBLC6 ESD, arte Bottom Tesla/Dr. STONE. |
| **v2.6** | 2026-11-28 | **Post-Auditoría Elephant-Goldfish #1 & #2:** (1) Corregida falla catastrófica de `U1 DRV8424EPWPR` (`SOP65P640X120-29N`): apertura `F.Mask` en Exposed Pad 29 + pasta windowpane 60% (2×4) + *solder mask dam* `0.14 mm` en Matte Black; (2) Eliminado `R1` (`15k`) en `nFAULT/GPIO7` (sube de `1.98V` inválido a `3.30V`); (3) Intercambiados `R3=10k / R4=15k` en `VREF` (`1.98V = 1.50A`, coincide con `TP11 2.0V`); (4) `U19` Pin 5 `VP` re-cableado de `+5V` a `+3V3` y Pin 6 desconectado de `TIRA_LED_5V`; (5) Eliminados 7 `SD03C` redundantes (`D22–D27, D29`); (6) `D19` a `SMAJ5.0A` (`C83329`) + `R79=191k` (`3.265V`); (7) `D8` pasta windowpane 60% (2×3) + VIPPO explícito en `LEEME_JLCPCB.txt`; (8) 7× MLCCs `VBAT` a Murata `10 µF 50V 0805` BASIC (`C440198`); (9) `U17` sincronizado a `CH340B` (`C81010`); (10) 4× vías de retorno `GND` en USB-C (`≤1.42mm`) + `FID1..FID3` + teardrops nativos + suavizado de 97 esquinas 90°/agudas (`0` esquinas 90°). |

## v2.7 (2026-11-28) — Derating C6 35V & Sincronización Total
- **C6 (`/Externo/VBAT`)**: actualizado de `100 µF 25 V` (`C2836443`, derating `50.4%`) a **`100 µF 35 V` (`C350012`, `MVK 35V100 M 6.3*8`, derating `36.0%`, `+10 V` margen sobre `SMBJ15A` `24.4 V`)**.
- **Documentación sincronizada:** regenerados `Tesla_JLCPCB_Gerbers.zip` (18 archivos, `PTH/NPTH` separados), `Tesla_v27_Assembly_Top.pdf`, `Tesla_v27_Fab_Drawing.pdf` y `Tesla_v27.step`; sello PCB `TESLA v2.7`; `0` errores ERC, `0/0/0` DRC, `non-oct=0, sharp-90=0, acute=0`.
