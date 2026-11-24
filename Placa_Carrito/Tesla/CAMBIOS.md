# Cambios de esta revisión (placa Tesla)

Resumen de lo hecho sobre `Placa_Carrito/Tesla/`, en orden. Todas las comprobaciones se hicieron con `kicad-cli` (KiCad 10): netlist exportada antes y después de cada cambio, ERC y DRC con paridad esquema-PCB.

## Verificación general

| Comprobación | Resultado |
|---|---|
| Redes (netlist) | 101 redes. Antes y después de reorganizar las hojas, las conexiones entre componentes son las mismas. Las únicas diferencias son las pedidas: pines de arranque sin conectar y GND y GNDPWR unidos. |
| ERC | Solo los 41 avisos de siempre de símbolo distinto de la librería (`lib_symbol_mismatch`). Sin errores. |
| DRC con paridad esquema-PCB | 1 aviso: la huella de J20 sigue siendo de pin macho (falta "Actualizar PCB desde esquema"). |

## Cambios de diseño

1. **Pines de arranque del S3 sin uso.** GPIO3, GPIO45 y GPIO46 se quitaron del conector de expansión y quedan sin conectar (con la X de no conectado). GPIO0 queda solo para el botón BOOT.
2. **Un solo GND.** Se unieron `GND` y `GNDPWR`: todos los símbolos `GNDPWR` pasaron a `GND`, se quitó el net tie `NT1`, y en la PCB las 44 referencias a `GNDPWR` pasaron a `GND` y se borró la huella de `NT1`. Si se quiere separar la masa de potencia y la lógica a propósito, habría que revertir este cambio.
3. **Control reducido a ESP32 y expansor.** Se sacó de `control.kicad_sch` todo lo que no es el S3, el PCA9555 y su circuito mínimo:
   - R21, R22 y R63 (pull-ups de I2C e INT) van a la hoja nueva `pullups_control.kicad_sch`.
   - R64, R65 y R66 (pull-downs de APH, BPH y nSleep) van a `motores.kicad_sch`.
   - J20 (conector de expansión) va a `comunicacion_usuario.kicad_sch`, ahora como **socket hembra** `PinSocket_1x16` para una placa encima, con I2C e INT en el conector.
4. **Radio nRF24 dentro de "Comunicacion con el usuario".** Desaparece `radio_nrf24.kicad_sch`; sus componentes (J12, FB1, C134 a C136 y R53 a R55) pasaron a un bloque de la hoja del usuario. `salidas_externas.kicad_sch` pasó a llamarse `comunicacion_usuario.kicad_sch`.
5. **Nombres GPIO en el control.** Los pines del S3 salen como `GPIOn` y los del expansor como `EXP_GPIO0` a `EXP_GPIO15` (antes APH, BPH, NRF_CE, LED_AP...). Los pines de la hoja Control en la raíz y las etiquetas de la raíz se renombraron igual. La librería oficial de Espressif no se tocó.
6. **Correcciones por datasheet.**
   - C131: 10 µF a 22 µF (el esquema de referencia del módulo usa 22 µF + 0,1 µF en 3V3).
   - R54: 100 kΩ a 10 kΩ (con 100 kΩ, CE del nRF24 quedaba a unos 1,65 V al arrancar contra el pull-up interno de ~100 kΩ del PCA9555).
7. **Conexiones nuevas en J20:** I2C (SDA, SCL), INT del expansor, GPIO2, GPIO36, GPIO37 y `EXP_GPIO10` a `EXP_GPIO15`, además de 3V3, 5V y GND.

## Cambios de estilo en todas las hojas

- Las conexiones entre ICs, conectores y hojas van por etiquetas; los cables solo unen un pasivo con su pin. La raíz ya no tiene cables entre bloques.
- Cada componente: cable corto, componente, cable corto y GND o alimentación. Se acabaron los capacitores sueltos conectados solo por símbolos de alimentación.
- Flechas de 3V3, 5V y VM arriba o de lado, con su nombre visible. GND hacia abajo siempre que fue posible.
- Valores de componentes verticales girados 90° junto al cuerpo.
- Cada hoja agrupada en bloques con marco punteado y título; "Comunicacion con el usuario" quedó en 8 grupos más USB-C y radio.
- Las hojas de la raíz se acercaron entre sí (papel A3).
- Las referencias de los símbolos de alimentación se hicieron únicas por hoja (antes había duplicados que provocaban errores de anotación al unir hojas).

## PCB (`Tesla.kicad_pcb`)

Solo cambios de nombres y rutas para mantener la paridad con el esquema, sin tocar posiciones ni pistas:

- Nombres de red actualizados (`GPIOn`, `EXP_GPIOn`, `NRF_VCC`, `USB_VBUS`, `VLED`...).
- Rutas de hoja de las huellas que cambiaron de hoja (radio, J20, R21, R22, R63, R64, R65 y R66).
- Valores de C131 (22 µF) y R54 (10 kΩ).
- Se quitó la huella de `NT1`.

## Documentación y archivos

- `README.md` (nuevo): guía del diseño, impacto en el firmware y cómo programar el PCA9555.
- `ASIGNACION_PINES.md`: reescrito con la nueva estructura, el mapa de `EXP_GPIO`, el pinout de J20 y la verificación con los datasheets.
- `Tesla_BOM.csv`: J20 como socket, C131 y R54 con su valor nuevo, `NT1` eliminado.
- `Tesla.kicad_pro`: hoja nueva en la lista de hojas, hoja del radio eliminada y aviso `multiple_net_names` ignorado (se usa a propósito cuando una etiqueta global y una jerárquica nombran la misma red).

## Qué no se tocó

- Las librerías de Espressif (`Componentes/`).
- El firmware (`Codigo Carrito/`): sigue siendo para P4 más S3. Ver `README.md` de esta carpeta para lo que hay que portar.
- Los archivos de otras placas (`Placa_Modular_S3_P4`, `Revision_Modulos`, control remoto).
