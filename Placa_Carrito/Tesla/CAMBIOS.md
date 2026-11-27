# Cambios de esta revisión (placa Tesla)

Resumen de lo hecho sobre `Placa_Carrito/Tesla/`, en orden. Todas las comprobaciones se hicieron con `kicad-cli` (KiCad 10): netlist exportada antes y después de cada cambio, ERC y DRC con paridad esquema-PCB.

## Verificación general

| Comprobación | Resultado |
|---|---|
| Redes (netlist) | 94 redes con al menos 2 conexiones tras mover los conectores a la raíz (101 antes, contando pines sueltos). Antes y después de reorganizar las hojas, las conexiones entre componentes son las mismas. Las únicas diferencias son las pedidas: pines de arranque sin conectar y GND y GNDPWR unidos. |
| ERC | Solo los avisos de símbolo distinto de la librería (`lib_symbol_mismatch`) y los de dos nombres en una misma red (a propósito). Sin errores. |
| DRC con paridad esquema-PCB | Desactualizado a propósito: falta "Actualizar PCB desde esquema" (J4, J5, J13, J14, J20 y los nombres de red nuevos). |

## Cambios de diseño

1. **Pines de arranque del S3 sin uso.** GPIO3, GPIO45 y GPIO46 se quitaron del conector de expansión y quedan sin conectar (con la X de no conectado). GPIO0 queda solo para el botón BOOT.
2. **Un solo GND.** Se unieron `GND` y `GNDPWR`: todos los símbolos `GNDPWR` pasaron a `GND`, se quitó el net tie `NT1`, y en la PCB las 44 referencias a `GNDPWR` pasaron a `GND` y se borró la huella de `NT1`. Si se quiere separar la masa de potencia y la lógica a propósito, habría que revertir este cambio.
3. **Control reducido a ESP32 y expansor.** Se sacó de `control.kicad_sch` todo lo que no es el S3, el PCA9555 y su circuito mínimo:
   - R21, R22 y R63 (pull-ups de I2C e INT) se sacaron primero a una hoja aparte, pero el PCA9555 los exige (SCL, SDA e INT son de colector abierto), así que **volvieron a `control.kicad_sch`** junto al expansor y la hoja intermedia `pullups_control.kicad_sch` se eliminó.
   - R64, R65 y R66 (pull-downs de APH, BPH y nSleep) van a `motores.kicad_sch`.
   - J20 (conector de expansión) va a `comunicacion_usuario.kicad_sch`, ahora como **socket hembra** `PinSocket_1x16` para una placa encima, con I2C e INT en el conector.
4. **Radio nRF24 dentro de "Comunicacion con el usuario".** Desaparece `radio_nrf24.kicad_sch`; sus componentes (J12, FB1, C134 a C136 y R53 a R55) pasaron a un bloque de la hoja del usuario. `salidas_externas.kicad_sch` pasó a llamarse `comunicacion_usuario.kicad_sch`.
5. **Nombres GPIO en el control.** Los pines del S3 salen como `GPIOn` y los del expansor como `EXP_GPIO0` a `EXP_GPIO15` (antes APH, BPH, NRF_CE, LED_AP...). Los pines de la hoja Control en la raíz y las etiquetas de la raíz se renombraron igual. La librería oficial de Espressif no se tocó.
   - En `control.kicad_sch`, la unión interna entre el S3 y el expansor para SCL (GPIO39), SDA (GPIO40) e INT (GPIO42) se hace con **etiquetas locales** del lado del PCA9555. La etiqueta jerárquica (la que sale de la hoja) queda una sola vez, del lado del S3, porque el bus también llega al GPS, la IMU, el INA219 y J20.
6. **Correcciones por datasheet.**
   - C131: 10 µF a 22 µF (el esquema de referencia del módulo usa 22 µF + 0,1 µF en 3V3).
   - R54: 100 kΩ a 10 kΩ (con 100 kΩ, CE del nRF24 quedaba a unos 1,65 V al arrancar contra el pull-up interno de ~100 kΩ del PCA9555).
7. **Conexiones nuevas en J20:** I2C (SDA, SCL), INT del expansor, GPIO2, GPIO36, GPIO37 y `EXP_GPIO10` a `EXP_GPIO15`, además de 3V3, 5V y GND.

## Cambios de estilo en todas las hojas

- Las conexiones entre ICs, conectores y hojas van por etiquetas; los cables solo unen un pasivo con su pin. En la raíz, los conectores externos van con cable directo a los pines de la hoja `Externo`.
- Cada componente: cable corto, componente, cable corto y GND o alimentación. Se acabaron los capacitores sueltos conectados solo por símbolos de alimentación.
- Flechas de 3V3, 5V y VM arriba o de lado, con su nombre visible. GND hacia abajo siempre que fue posible.
- Valores de componentes verticales girados 90° junto al cuerpo.
- Cada hoja agrupada en bloques con marco punteado y título; "Comunicacion con el usuario" quedó en 8 grupos más USB-C y radio.
- La raíz pasó a papel A1 para alojar todos los conectores en una columna.
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

## Ajustes estéticos hechos a mano en `control.kicad_sch`

Después de generar la hoja, se retocó a mano en KiCad. **No regenerar esta hoja con scripts: editarla sobre el archivo actual.** Los cambios, que se toman como criterio para las demás hojas:

- **Reset (EN):** la línea sale recta desde el pin, sin subir ni doblar. En orden desde el pin: C133, R52 (hacia arriba con 3V3 arriba) y SW4. C133 y SW4 cuelgan hacia abajo y comparten un solo GND.
- **Pull-ups del PCA9555 (R21, R22 y R63):** resistencias verticales hacia arriba, con la flecha de 3V3 siempre hacia arriba, y la etiqueta local pegada al nodo. Van escalonadas a lo largo del cable (5, 19 y 29 mm del pin) para que ninguna etiqueta cruce la resistencia siguiente.
- **Valores de componentes verticales:** girados 90° junto al cuerpo (a la izquierda; R63 a la derecha porque la izquierda estorba). El texto de BOOT S3 va pegado al botón.
- **Notas de la hoja:** varias notas cortas, una idea por línea, arriba a la izquierda de los marcos.

Tras estos retoques se comprobó de nuevo: las 101 redes son las mismas y el ERC solo da los 41 avisos de siempre.

## Reorganización: conectores externos en la raíz

Pedida para poder cablear la placa "como será de verdad": la raíz solo tiene lo que el usuario toca.

- Nueva hoja `Externo.kicad_sch` con todo el circuito (las hojas de antes). La raíz tiene esa hoja y los conectores.
- **Motores:** J4 y J5 pasan a un conector PH2.0 de 6 pines por motor (M1, GND del encoder, C1, C2, VCC del encoder a 3V3, M2), con el orden de los cables del JGB37-520. J13 y J14 (encoders de 4 pines) desaparecen.
- **Conectores movidos a la raíz con cable directo:** servos J7 a J9, LiDAR J11 (con R24), tira J16, UART J2, radio J12, expansión J20 y LEDs RGB J17. J6 (batería), J10 (GPS) y J15 (IMU) ya estaban.
- **USB-C:** hoja jerárquica `usb.kicad_sch` con J3, U13, R61 y R62; solo sale `USB_S3_DM` y `USB_S3_DP`.
- Quedan en `comunicacion_usuario.kicad_sch` solo el apoyo del radio (filtro de alimentación y pull-ups de CE, CSN e IRQ) y los LED de los faros. Se quitaron los marcos vacíos y las entradas de alimentación `+5V` y `5V_LIDAR` que ya no usa.
- Los títulos de las hojas decían "ESP32-P4"; ahora dicen ESP32-S3. En motores se movió el cuadro de pull-downs para que no se encime con el cajetín.
- Se compararon las redes antes y después de mover los conectores: las mismas conexiones.
- `Tesla_BOM.csv`: J4 y J5 con el conector de 6 pines; J13 y J14 eliminados.
- I2C: el GPS, la IMU y los pines I2C de J20 ya no están unidos al S3 (GPIO39, GPIO40 y GPIO42 no salen de Control). Pendiente de decidir.

## Alimentación en la jerarquía (entradas y salidas de rieles)

- La hoja `Alimentacion` **saca** `VM`, `+3V3`, `+5V` y `GND`; cada hoja consumidora **recibe** los rieles que usa en un bloque "Entradas de alimentacion" (etiqueta jerárquica, cablecito con giro y símbolo de alimentación).
- En `Externo` cada pin de riel lleva una etiqueta global del mismo nombre, así todos los `GND`, `+3V3`, `+5V` y `VM` quedan unidos. Sigue habiendo un solo `GND`.

## Alimentación: un solo buck de 5 V y menos condensadores

Se simplificó la alimentación según el datasheet de cada regulador:

- **Un solo buck de 5 V, AP64501SP-13 (5 A)** en lugar de dos AP63205 de 2 A. Corriente de 5 V en el peor caso: 3 servos atascados (~2 A), tira WS2812 (~1,8 A), LiDAR (~0,3 A) y GPS: unos 4,5 A. Se quitaron `alim_buck_5v_lidar.kicad_sch`, U8, L6 y sus condensadores.
- **Valores del AP64501 para 5 V** (Tabla 1 del datasheet DS41980 Rev. 5-2, `Datasheets/AP64501_Diodes.pdf`): R1 = 115 k y R2 = 22,1 k (Vout = 0,8 V x (1 + R1/R2) = 4,96 V), L = 3,3 uH (el datasheet usa 3,6 uH; admite de 1 a 10 uH con Isat al menos 35 % sobre la carga y DCR menor de 10 mohm), C1 = 10 uF, C2 = 3 x 22 uF, bootstrap de 100 nF entre BST y SW, compensación R5 = 15,8 k y C5 = 2,7 nF desde COMP a GND, arranque suave con Css = 10 nF (4 ms). EN va unido a VIN, la almohadilla térmica a GND. Se omiten los condensadores opcionales C4 y C6.
- **LiDAR:** va directo al riel de 5 V, sin filtro. El D500 (LD19) trae su propia electrónica y consume unos 180 a 300 mA con 4,5 a 5,5 V, así que se quitaron la ferrita FB2, el condensador C76 y los TVS D9 y D10. El riel `5V_LIDAR` ya no existe.
- **Buck de 3V3 (AP63203):** la salida queda con 2 x 22 uF, como en el circuito típico del datasheet. Se quitaron un condensador de 10 uF y cuatro de 0,1 uF.
- **Protección de batería:** se quitaron el 10 uF y el 0,1 uF de la salida; quedan el fusible, el MOSFET, el TVS y el electrolítico de 470 uF.
- Símbolo nuevo `AP64501SP` en `Tesla_Parts.kicad_sym`. Inductor elegido de JLCPCB: cjiang FEXL0650A-3R3M (C5378537): 3,3 uH, 11 A, Isat 12,5 A, DCR 12,5 mohm, cumple Isat y corriente de sobra (el datasheet del AP64501 recomienda DCR menor de 10 mohm solo para máxima eficiencia; con 12,5 mohm se pierden unos 0,3 W a 5 A). La ferrita del LiDAR es la Murata BLM21PG221SN1D (C85840, 220 ohm, 2 A).
- La hoja `Buck 5V servos y GPS` se llama ahora `Buck 5V`. En el PCB hay que actualizar desde el esquema (se quitan U8, L6 y varios condensadores y se añaden resistencias, condensadores y la ferrita).

## Protección contra batería al revés

- **D8, Schottky SS10U45 (Yangjie, JLCPCB C698773):** en serie con la batería, entre J6 y el pin `VBAT` de `Externo`. Cátodo hacia la placa, ánodo hacia el pin 1 de J6. 45 V y 10 A, VF = 0,43 V típica a 10 A (0,47 V máxima).
- **Pérdidas:** a 3 A son unos 1,3 W y a 8 A (pico con motores atascados) unos 3,4 W. Se pierde casi 0,4 V de la batería. Necesita cobre bajo la lengüeta.
- **Huella propia** `Tesla_Parts:TO-277_SS10U45`, hecha con el patrón de pads del datasheet: pad 1 = cátodo (pines 1 y 2), pad 2 = ánodo (lengüeta/CASE). El datasheet en `Datasheets/SS10U45_Yangjie.pdf`.
- J6 se movió 12,7 mm a la derecha para dar sitio al diodo. El `PWR_FLAG` queda en el lado de la placa.
- No hay fusible: ante un cortocircuito no hay nada que corte la corriente.

## Se quitó la hoja "Comunicacion con el usuario"

- Ya no hacía falta: la antenita nRF24 (módulo U2 en `Externo`) reemplaza al socket con su circuito de apoyo (filtro de alimentación, pull-ups de CE, CSN e IRQ). Se eliminaron `comunicacion_usuario.kicad_sch`, FB1, C134 a C136 y R53 a R55.
- Los LED de los faros, D5 y D6, pasaron a la hoja `Faros` (bajo el driver), así que `FAROS_POS` y `FAROS_NEG` ya no salen de la hoja.
- Las filas de señales que van a la raíz (incluidas `NRF_CE`, `NRF_CSN` y `NRF_IRQ`) quedaron en la columna a la derecha de Control.
- J12 (socket del radio) sigue en la raíz, sin su filtro ni sus pull-ups, y con `NRF_VCC` sin alimentar. Queda por decidir si se quita, ya que U2 lo reemplaza.
- En `Externo` los cuadros de Faros, Tira LED y LEDs RGB se compactaron como el resto, con las conexiones intactas.

## VM solo en el driver de motores

- `VM` (voltaje de motor) se usa únicamente en el driver DRV8424. Todo lo demás (faros, LEDs RGB con el puente J18, bucks) toma `VBAT`, la batería después del diodo D8.
- En `Faros` y `LEDs RGB y estado` el pin, la etiqueta y los símbolos de alimentación `VM` pasaron a `VBAT`, y en `Externo` sus entradas se unen a la etiqueta jerárquica `VBAT`, como el driver de motores. Antes `VM` de esas dos hojas no tenía ninguna fuente.
- El driver de motores sigue con su pin `VM`, unido a `VBAT` en `Externo`. Las hojas sin uso `alim_proteccion` y `alim_medicion` quedan en la carpeta pero fuera del proyecto.

## Luces: faros, intermitentes y tira en una sola hoja

- La hoja `Tira LED` se unió a `Faros` (`tira_led.kicad_sch` se eliminó). `Faros` recibe `VBAT`, `+5V` y `GND`, y tiene los pines `LUCES`, `TIRA_LED_3V3`, `TIRA_LED_5V`, `INT_IZQ` e `INT_DER`.
- **Intermitentes:** dos copias del driver de los faros (AL8860, 350 mA): U15 con D13 (izquierdo) y U16 con D14 (derecho), con sus pasivos (C141 a C144, R70 a R73, L8, L9, D11, D12). Las dos señales de control, `INT_IZQ` e `INT_DER`, quedan sin GPIO asignado. La hoja pasó a A3.
- **LED de 1 W:** JLCPCB solo tiene los de 1 W en SMD (3030 y similares); los de patas que se doblan no están en su catálogo. Pendiente elegir (ver BOM: D5, D6, D13 y D14).
- **Entradas de alimentación:** se quitó el bloque "Entradas de alimentacion" de todas las hojas. Los carriles positivos entran por etiquetas jerárquicas en el circuito y GND por una etiqueta junto a un símbolo de GND.

## Los LED de las luces son externos

- Los LED de los faros y de los intermitentes salen de la placa: ya no están en la hoja `Faros` (D5, D6, D13 y D14 se quitaron). En la raíz hay un conector JST PH de 2 pines por canal: J21 (faros, 2 LED en serie), J22 (intermitente izquierdo) y J23 (intermitente derecho). Pin 1 = positivo, pin 2 = negativo. Unidos con cable directo a la hoja `Externo`, como el resto de los conectores.
- La hoja `Faros` entrega `FAROS_POS`, `FAROS_NEG`, `INT_IZQ_POS`, `INT_IZQ_NEG`, `INT_DER_POS` e `INT_DER_NEG`. La tira WS2812B ya estaba afuera (J16).
- En `Externo`, las etiquetas `+3V3` y `+5V` de la hoja `Alimentacion` y de `Faros` pasaron a etiquetas globales: como jerárquicas daban error porque la raíz no tiene esos pines.

## Radio afuera, sin LED RGB y GPIO en la interfaz

- **Radio:** el módulo nRF24L01 (U2) salió de `Externo` y está en la raíz, en su cuadro, con cable directo a los pines de `Externo` y alimentación `+3V3` y `GND`. J12 (el socket) se quitó.
- **LEDs RGB:** se eliminaron, porque se usa la tira. Se quitaron U11, J17, J18, R56 a R58 y C137. La hoja `leds_rgb` pasó a `LEDs de estado` (`leds_estado.kicad_sch`) y solo conserva los 4 LED de estado (dos por rueda): `LED_A_ADELANTE`, `LED_A_ATRAS`, `LED_B_ADELANTE` y `LED_B_ATRAS` (antes `LED_AP`, `LED_AN`, `LED_BP` y `LED_BN`). Solo recibe `+3V3`.
- **Nombres GPIO en la interfaz:** los pines de `Externo` con GPIO ya asignado se llaman como el GPIO (GPIO43, GPIO44, GPIO47, GPIO48, GPIO35, GPIO19, GPIO20, EXP_GPIO0, EXP_GPIO1), y en la raíz el cable que los une al conector lleva una etiqueta local con la función (`S3_TXD0`, `GPS_RX`, `LIDAR_RX`, `NRF_CE`...). Los pines sin GPIO asignado siguen con su nombre funcional hasta que se asignen.
- Se quitó la etiqueta `GPIO10` de la fila de `NRF_CSN`: chocaba con `BPH` del driver de motores.

## LEDs de estado fuera de la jerarquía

- **LED de estado en la raíz:** la hoja `LEDs de estado` (`leds_estado.kicad_sch`) se eliminó. Los 4 LED (D1 a D4) y sus resistencias de 1 kΩ (R17 a R20) están ahora en la raíz, debajo de la hoja USB-C, al lado del cuadro `Externo`. Cada fila va directa a un pin de `Externo` y a `+3V3`, sin etiquetas.
- **Interfaz:** `Externo` tiene 4 pines nuevos (`LED_A_ADELANTE`, `LED_A_ATRAS`, `LED_B_ADELANTE`, `LED_B_ATRAS`), con su fila de etiqueta local y etiqueta jerárquica dentro. Siguen sin GPIO asignado.

## Faros delanteros: un conector por LED

- Los faros tenían un solo conector (J21) para los dos LED. Ahora hay uno por LED: J21 y J24, JST PH de 2 pines. Siguen en serie para que reciban la misma corriente (333 mA): `FAROS_POS` a J21 pin 1, J21 pin 2 a J24 pin 1 y J24 pin 2 a `FAROS_NEG`.

## Interfaz `Externo` solo con nombres GPIO

- Todo pin de `Externo` que va a un GPIO del S3 se llama GPIO, sin nombres de función. Los GPIO sin asignar llevan un nombre provisional único (`GPIOX1` a `GPIOX15`; no pueden repetirse, porque dos pines con el mismo nombre quedarían unidos en un solo net). El nombre de la función va en una etiqueta local sobre el cable de la raíz. Al asignar el GPIO real solo hay que renombrar ese pin y su etiqueta jerárquica.
- Provisionales: `GPIOX1` a `GPIOX4` = ENC_A1, ENC_A2, ENC_B1, ENC_B2; `GPIOX5` = SERVO_DIR; `GPIOX6` y `GPIOX7` = SERVO_1 y SERVO_2; `GPIOX8` a `GPIOX11` = NRF_MISO, NRF_MOSI, NRF_SCK, NRF_CSN; `GPIOX12` a `GPIOX15` = LED_A_ADELANTE, LED_A_ATRAS, LED_B_ADELANTE, LED_B_ATRAS.
- Las filas sin GPIO quedan abiertas por el lado de Control (sin etiqueta local).
- **Errores corregidos:** la señal del servo J7 estaba etiquetada `ENC_A1` y quedaba unida al encoder A; ahora es `SERVO_DIR`. El motor B tenía M1 y M2 invertidos respecto al motor A (pin 1 en `BOUT2`); ahora pin 1 = `BOUT1` y pin 6 = `BOUT2`, igual que J13. La etiqueta `GND` jerárquica suelta en la raíz (J7) pasó a global. Se añadió la fila `GPIO2` que faltaba en `Externo` (hacia J20) y se quitaron 3 etiquetas `+5V` sueltas en `Externo`.

## GPIO reales en la interfaz (se acabaron los GPIOX)

- Los provisionales `GPIOX1` a `GPIOX15` se reemplazaron por el pin real, tanto en el cuadro `Externo` de la raíz como dentro de `Externo` (etiqueta local a la izquierda, unida por nombre al pin de Control, y etiqueta jerárquica a la derecha). El nombre de la función queda como etiqueta local en el cable de la raíz.
- Asignación: ENC_A1 = GPIO4, ENC_A2 = GPIO5, ENC_B1 = GPIO6, ENC_B2 = GPIO17; SERVO_DIR = GPIO1, SERVO_1 = GPIO36, SERVO_2 = GPIO37; NRF_MISO = GPIO21, NRF_MOSI = GPIO18, NRF_SCK = GPIO38, NRF_CSN = GPIO41; LED de estado A adelante, A atrás, B adelante, B atrás = EXP_GPIO6, 7, 8, 9 (expansor PCA9555).
- Los GPIO 7 a 16 ya los usaban los drivers de motores y las luces, así que no quedaban pines nativos libres para todo. Para los 3 servos se usaron GPIO1, GPIO36 y GPIO37. Por eso J20 perdió los pines 7 y 8 (antes GPIO36 y GPIO37), que quedan marcados como sin conexión; en J20 siguen GPIO2 y EXP_GPIO10 a 15.

## Interfaz de `Externo`: etiqueta jerárquica directa desde Control

- Siguiendo lo que hiciste con GPIO4: cada señal externa sale de su pin de Control con una etiqueta jerárquica del mismo nombre (GPIO y EXP_GPIO), directo al pin del cuadro `Externo` de la raíz. Se eliminaron las filas intermedias (cable, etiqueta local y etiqueta jerárquica a la derecha). Afecta a 30 señales: GPIO1, 2, 5, 6, 17, 18, 19, 20, 21, 35, 36, 37, 38, 41, 43, 44, 47, 48 y EXP_GPIO0, 1, 6 a 15 (GPIO4 ya estaba así). La conectividad no cambió: la netlist es idéntica.

## Conflictos de GPIO resueltos en la raíz

- **GPS (J10):** GPS_RX y GPS_TX vuelven a GPIO47 y GPIO48. GPIO19 y GPIO20 son el USB-C nativo del S3 y no se pueden compartir. GPS_SCL y GPS_SDA se quedan en GPIO17 y GPIO18, y comparten el bus con la IMU (J15), que se conectó: pin 1 `+3V3`, pin 2 `GND`, pin 3 `GPS_SCL`, pin 4 `GPS_SDA`.
- **Radio:** GPIO18 pasó a ser SDA del GPS, así que NRF_MOSI = GPIO41 y NRF_CSN = EXP_GPIO2 (por el expansor PCA9555). NRF_MISO = GPIO21 y NRF_SCK = GPIO38 no cambian.
- **J20:** el driver de motores usa GPIO2, así que J20 pierde ese pin (pin 6 sin conexión). Quedan en J20 solo EXP_GPIO10 a 15, `+3V3`, `GND` y `+5V`.

## Raíz: alimentación directa a `Externo`, sin etiquetas

- Todos los conectores y módulos externos toman `+5V`, `+3V3` y `GND` con cable directo a su propio pin del cuadro `Externo`, como el GPS (J10). Se quitaron de la raíz todas las etiquetas globales y los símbolos de alimentación (menos el PWR_FLAG de `VBAT`).
- Hay 25 pines nuevos en `Externo`, repetidos por nombre (`+5V`, `+3V3`, `GND`, y `GPIO17` y `GPIO18` para la IMU J15). Pines con el mismo nombre quedan en el mismo net. Dentro de `Externo` se añadió una etiqueta jerárquica `+3V3` en el riel de Control (antes solo salían `+5V` y `GND`).
- Casos especiales: los 4 LED de estado comparten un solo `+3V3` con un riel vertical; J11 (LiDAR) comparte `GND` con la resistencia R24; el `GND` del módulo del radio da la vuelta por arriba del símbolo.
- La netlist exportada no cambió: son las mismas conexiones que antes de este cambio.

## Hoja USB-C: GND directo

- La hoja `USB-C` tenía su `GND` por símbolos de alimentación. Ahora lo recibe por un pin `GND` de la hoja, con cable directo a un pin `GND` del cuadro `Externo`, como el resto de los externos. La netlist exportada no cambió.

## BOM al día con el esquema

- Se corrigió `Tesla_BOM.csv` contra la netlist: J4 pasó a J13 (motor A) y se quitaron C85, C90, F1, Q1, R27, R28 y R29, que ya no están en el esquema (protección con MOSFET y fusible, divisor de batería). Quedan 101 componentes en el esquema y todos aparecen en la BOM.

## Raíz: bloques externos compactados

- El cuadro `Externo` de la raíz pasó de 508 mm de alto a 400 mm. Los bloques de la derecha (servos, LiDAR, tira y UART, radio, expansión J20, USB-C y LED de estado) se apilaron justo debajo de los motores, con 6,35 mm entre bloques, y los pines de `Externo` bajaron con ellos.
- Los conectores se acercaron 25,4 mm al cuadro (los cables son más cortos). Se rehicieron los marcos punteados y sus títulos, ya sin el espacio vacío que tenían, y la nota de pines del motor JGB37-520 quedó al lado de J13.
- Solo se movió la posición: la netlist exportada es idéntica (103 conexiones) y ERC queda igual.

## Segundo USB-C (puente CH340C a UART0) y bloques pegados a `Externo`

- **UART por USB:** el header J2 (TX RX GND) se quitó. Ahora hay una hoja `USB-UART` (`usb_uart.kicad_sch`) con un segundo USB-C (J2), protección ESD USBLC6-2SC6 (U18), puente CH340C (U17, alimentado a 3,3 V con V3 unido a VCC, C145) y el circuito de auto-reset de siempre: dos transistores MMBT3904 (Q1, Q2) con R76 y R77 de 10 kΩ. DTR maneja GPIO0 (modo de arranque) y RTS maneja EN (reinicio), así que se programa sin pulsar BOOT ni RESET. Igual que el USB nativo, el puente toma su alimentación de la placa (batería) y el VBUS del conector solo va al ESD.
- **Pines nuevos:** el TXD del CH340C va a GPIO44 y el RXD a GPIO43 (UART0 del S3). `EN` y `GPIO0` salen de Control por etiquetas jerárquicas nuevas; `Control` y `Externo` tienen pines `EN` y `GPIO0`, y en la raíz van directo a la hoja `USB-UART`.
- **Los dos USB:** USB-C nativo del S3 (hoja `USB-C`, GPIO19 y GPIO20) y USB-C por UART (hoja `USB-UART`), como los módulos S3 del mercado.
- **Compactado:** los conectores quedaron a unos 11 mm de `Externo` (como J8, J9 y J11) y los bloques se apilaron con un título sobre cada uno. El cuadro `Externo` de la raíz mide ahora 360 mm de alto (antes 400). Se quitaron los marcos punteados viejos, que ya no coincidían con los bloques.
- **LiDAR:** J11 queda con el pin 2 (PWM) sin conectar y sin R24, como decidiste (velocidad fija). BOM al día contra la netlist: se añadieron U17, U18, Q1, Q2, R74 a R77 y C145, y se quitaron R24 y el header J2.

## Alimentación por USB, GPIO renumerados y bloques de nuevo enlazados

- **La placa se alimenta por cualquiera de los dos USB-C.** El VBUS de cada USB-C sube por un pin nuevo de su hoja hasta `Externo`, y ahí pasa por un diodo Schottky SS14 hacia `VBAT` (D15 para el USB JTAG y D16 para el USB UART). Los dos USB y la batería se pueden conectar a la vez: los diodos los separan, y D8 impide que el USB cargue la batería. Con solo USB, `VBAT` queda en unos 4,6 V: alcanza para los 3,3 V y para programar, pero el 5 V (servos y LiDAR) queda justo y el driver de motores no arranca.
- **Nombres de los USB:** `USB JTAG (USB nativo del S3)` (GPIO19 y GPIO20) y `USB UART (CH340C)` (GPIO43 y GPIO44, más EN y GPIO0 para la programación automática). Los dos cuelgan del borde inferior de `Externo` con cable directo.
- **GPIO:** como GPIO19 y GPIO20 son obligatorios para el USB nativo, los servos siguen a continuación: SERVO_DIR = GPIO21, SERVO_1 = GPIO35, SERVO_2 = GPIO36, y LIDAR_RX pasó a GPIO37. NRF_MISO quedó en GPIO12 (liberado por SERVO_DIR). `LIDAR_PWM` sale por `EXP_GPIO3` (el pin PWM del LiDAR ya no queda al aire; el firmware lo deja en alta impedancia o lo baja a 0 para velocidad fija).
- **Enlazado y piezas repuestas:** el módulo nRF24 (U2) a la izquierda con sus 6 señales, `+3V3` y `GND` directos a `Externo`; los 4 LED de estado y J20 vueltos a colocar al lado derecho; el cuadro `Externo` mide ahora 194 mm de alto.
- J16 (tira): el orden de pines queda como lo cableaste, 1 DATA, 2 +5V, 3 GND.

## Revisión del remapeo de GPIO

Se revisó el remapeo contra la netlist. Estaba bien en motores (ENC en GPIO13 a 16, driver en GPIO1, 2, 4, 5, 6 y 7), faros (GPIO8 a 11), GPS (GPIO17 y 18 para I2C, GPIO47 y 48 para UART) y SPI del radio (GPIO36, 37 y 38). Se corrigieron estos errores:
- **USB nativo:** DM y DP estaban conectados a `EXP_GPIO9` y `EXP_GPIO8`, es decir, a pines del expansor PCA9555. El USB del S3 solo funciona en GPIO19 (D-) y GPIO20 (D+); vuelven a ir ahí.
- **USB UART:** TXD y RXD del CH340C estaban en `EXP_GPIO7` y `EXP_GPIO6`. El arranque por UART del S3 (esptool) usa UART0, GPIO43 y GPIO44, así que vuelven a esos pines.
- **Servos en GPIO19 y GPIO20:** chocaban con el USB nativo. Ahora SERVO_DIR = GPIO12, SERVO_1 = GPIO35 y SERVO_2 = GPIO41.
- **SERVO_DIR y LIDAR_RX en GPIO21:** estaban en cortocircuito. LIDAR_RX queda en GPIO21.
- **Radio:** NRF_CE estaba en GPIO44 (UART0). Ahora NRF_CE = EXP_GPIO0 y NRF_CSN = EXP_GPIO6 (por el expansor, lo que libera GPIO41).
- **LIDAR_PWM:** pasó de GPIO35 a `EXP_GPIO7` para liberar un pin del S3 para los servos.
- **Pines sobrantes:** `EXP_GPIO8` y `EXP_GPIO9` quedan libres, sin salir de Control.
Sin conexión y sin tocar: GPIO3, GPIO45 y GPIO46 (pines de arranque). GPIO35 a 37 y GPIO47/48 son utilizables en el módulo N16R2.

## Diodos de VBUS dentro de las hojas USB y revisión de tus pines

- **Diodos:** D15 y D16 (SS14) estaban en `Externo` por comodidad mía; ahora están dentro de su hoja USB (D15 en `USB JTAG`, D16 en `USB UART`), entre el VBUS del conector y una etiqueta jerárquica `VBAT`. Cada hoja saca un pin `VBAT` y en la raíz va directo a un pin `VBAT` de `Externo` (el mismo nombre que el de la batería, así que quedan en el mismo net). `Externo` ya no tiene los diodos.
- **GPIO corregidos de tu último acomodo:** los pines de datos del USB nativo estaban en GPIO43 y GPIO44, y los del puente UART en GPIO19 y GPIO20, al revés. El USB nativo solo funciona en GPIO19 y GPIO20, y el arranque por UART solo en GPIO43 y GPIO44, así que se intercambiaron.
- **LiDAR:** LIDAR_RX estaba en `EXP_GPIO2` (expansor). Un UART a 230400 baudios no se puede leer por el PCA9555, así que ahora es GPIO41 y NRF_CSN pasó a `EXP_GPIO2`.

## Hojas USB rehechas y radio en pines nativos

- **Hojas USB:** se rehicieron todos los cables de `USB JTAG` y `USB UART`. VBUS va con cable directo del conector al ESD y al diodo SS14 (D15 y D16, dentro de cada hoja) y de ahí a `VBAT`. Los datos van con cable directo del conector al ESD y, en la hoja UART, de ahí al CH340C. Los `GND` ahora salen en horizontal. Los marcos y los textos se ordenaron.
- **Radio:** el SCK del nRF24 estaba en `EXP_GPIO0` (el expansor es demasiado lento para SPI). Ahora SCK = GPIO37 (MOSI = GPIO38 y MISO = GPIO41) y LIDAR_PWM = `EXP_GPIO0`.
- **Expansión:** `EXP_GPIO8` y `EXP_GPIO9` (únicos libres del expansor) salen por los pines 3 y 4 de J20.

## Hoja `USB UART`: auto-reset ordenado

- Los dos transistores (Q1 y Q2) con sus resistencias (R76 y R77) estaban sueltos, lejos del resto de la hoja, con las etiquetas `RTS` y `DTR` encima de los números de pin. Ahora están en un segundo marco, "Auto-reset: DTR y RTS manejan EN y GPIO0", con una nota que explica para qué sirve, las etiquetas en horizontal y los textos pegados a cada pieza.
- Se añadió la marca de no conexión que faltaba en el pin R232 del CH340C. ERC queda sin errores.

## `USB UART` en un solo marco

- Los transistores del auto-reset (Q1, Q2, R76 y R77) se conservan, porque permiten programar el S3 desde el USB UART sin pulsar BOOT ni RESET, pero ahora están dentro del mismo marco que el resto de la hoja, con el mismo estilo de título y de letras que la hoja `USB JTAG`. Una nota corta explica su función.

## J20 sin pines al aire

- Los pines 5 a 8 de J20 quedaban sin conexión. Ahora el pin 5 va a `+3V3`, el 6 a `+5V`, el 7 a `VBAT` (la batería de 12 V, después del diodo D8) y el 8 a `GND`, con cable directo a pines nuevos de `Externo`. Con los pines 1, 2, 15 y 16 que ya llevaban alimentación, la placa hija toma `+3V3` (pines 1 y 5), `+5V` (6 y 15), `VBAT` (7) y `GND` (2, 8 y 16). Pines de datos: 3 y 4 (`EXP_GPIO8` y `EXP_GPIO9`) y 9 a 14 (`EXP_GPIO10` a `EXP_GPIO15`).

## `USB JTAG` como copia de `USB UART`

- La hoja `USB JTAG` se rehízo a partir de la parte izquierda de `USB UART`: mismas posiciones del conector, las resistencias de 5,1 kΩ, el ESD, el diodo y los `GND`, con el mismo estilo de marco y de letras. Las salidas del ESD van directo a `USB_S3_DM` y `USB_S3_DP`. Referencias: J3, U13, R61, R62 y D15. La netlist exportada es la misma que antes (USB nativo en GPIO19 y GPIO20, VBUS por D15 hacia `VBAT`).

## Piezas verificadas en JLCPCB y primera versión de la placa

- Se buscó cada pieza en el catálogo de JLCPCB y se cambiaron las que no tenían existencias: buck de 5 V AP64501 por AP64500 (con RT de 200 kΩ en lugar del condensador de arranque suave), buck de 3,3 V AP63203 por AP63200 (con divisor de 196 k y 62 k y 100 pF), drivers de LED AL8860 por AL8861, adaptador de nivel en SOT-23-5, CH340C con otro código, pulsadores TS-1187A. Los bornes de los motores son WAGO 2060-453 (resorte, 2 por motor), la batería es un borne WAGO 2604-1102 (cables de hasta 4 mm²), el conector de expansión J20 es un socket 2x8 y los servos y la tira usan headers de 2,54 mm. R17 a R20 pasaron a 330 Ω y los condensadores de 22 µF a 25 V. Detalle y cálculos en [PCB_NOTAS.md](PCB_NOTAS.md).
- Cada símbolo tiene ahora el campo `LCSC` con su código y la huella correcta. Se añadió la huella `WAGO_2060-453_x2_6P_SMD` (según el datasheet) y se pasaron las vías del DRV8424 a 0,45/0,2 mm.
- Nueva `Tesla.kicad_pcb`: 120 x 150 mm, 4 capas, antena del ESP32 en el lateral derecho con zona prohibida, motores en los laterales, servos uno al frente y dos en los lados, USB y batería atrás. Sin rutar.
- `Tesla_BOM.csv` y `Tesla_BOM_JLCPCB.csv` regenerados con códigos, existencias y precios (unos 24 USD de componentes por placa).

## 2026-11-26 (placa)

- Nueva hoja "Filtro nRF24" (solo condensadores: 22 µF + 100 nF) y nueva hoja "Proteccion y desacople": TVS SMBJ15A en VBAT y SMAJ5.0A en +5V, 12 condensadores repartidos y arreglos ESD (SRV05-4 x3, USBLC6 x1) en las líneas que salen de la placa.
- Placa regenerada: socket de expansión 2x8, batería con borne WAGO, IMU alejado de la expansión, cuadros de etapas y serigrafía completa, DRV8424 con modelo 3D y máscara corregidos, modelos 3D de los bornes WAGO. Los cambios en el generador están en `herramientas/` (`pcb_build.py`, `silk_build.py`, `partsdb.py`).

## 2026-11-26 (tarde): diseno manual del usuario incorporado
- Se leyo el PCB guardado por el usuario y se registro en `herramientas/ajustes_usuario.json` (piezas movidas, textos y referencias de serigrafia).
- `pcb_build.py` ahora aplica esos ajustes; `Tesla.kicad_pcb` se regenero identico a lo hecho a mano, con zonas rellenadas y puntos de pin 1 recolocados junto a las piezas movidas.
- Eliminada la zona prohibida de antena del ESP32 (la antena sale del contorno).
