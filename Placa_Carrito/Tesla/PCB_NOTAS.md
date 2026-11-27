# Placa (PCB) del carrito Tesla: estado y criterios

Estado a 2026-11-26: **colocación preliminar sin rutar**. Están hechos el contorno, el apilado de 4 capas, las clases de red, las zonas de cobre, las zonas prohibidas de las antenas, las vías térmicas, la colocación de todas las piezas, la serigrafía completa (nombres, cuadros de etapas y leyendas de pines) y las protecciones TVS/ESD con el desacople repartido. DRC sin errores de colocación (solo quedan las pistas sin rutar y 4 avisos de agujeros de montaje). Falta rutar.

La placa se genera desde el esquema con `herramientas/pcb_build.py` (ver el encabezado del archivo). Cualquier cambio del esquema se refleja volviendo a ejecutarlo, o con "Actualizar PCB desde esquema" en KiCad, ya que las huellas conservan el enlace con los símbolos.

## 1. Apilado y proceso (JLCPCB)

| Capa | Uso |
|---|---|
| F.Cu | Señales, componentes y relleno de GND |
| In1.Cu (GND) | Plano de masa completo |
| In2.Cu (PWR) | VBAT en toda la placa; encima, zona de +5V (centro) y de +3V3 (zona del ESP32) |
| B.Cu | Señales y relleno de GND |

- 4 capas, 1,6 mm, cobre exterior de 1 oz. Apilado estándar de JLCPCB (JLC04161H-7628).
- Reglas de fabricación puestas: pista mínima 0,127 mm, separación mínima 0,127 mm, vía mínima 0,45/0,2 mm, anillo mínimo 0,125 mm, cobre a 0,3 mm del borde.
- Clases de red (en `Tesla.kicad_pro`): `Pot_12V` (VBAT, motores, D8) pista de 1,5 mm; `Pot_5V` 1,2 mm; `Pot_3V3` 0,6 mm; `Luces` 0,5 mm; `USB` 0,25 mm. Por defecto 0,2 mm con vía 0,6/0,3.
- Contorno: 130 x 170 mm con esquinas de radio 3 mm y 4 agujeros M3 para el chasis.

## 2. Dónde va cada cosa

Frente = borde superior; la placa se mira desde arriba.

| Zona | Piezas |
|---|---|
| Frente (centro) | `J7` servo de dirección, con los pines hacia afuera por el centro |
| Frente (lados) | `J22` intermitente izquierdo, `J21` y `J24` faros, `J23` intermitente derecho. Detrás de ellos, los 3 drivers AL8861 con su bobina y diodo |
| Lateral izquierdo | `U2` módulo nRF24 en su socket 2x4, con la antena pegada al borde. Más atrás `J8` servo 1 y `J5` motor B |
| Lateral derecho | `J9` servo 2, `J13` motor A, los LED del sentido de cada rueda al lado de su motor. Más abajo `J10` GPS |
| Trasera | `J6` batería, borne de resorte WAGO 2604-1102 (cables de hasta 4 mm², 20 A; el cable entra por el borde trasero), `J16` tira LED, `J11` LiDAR, `J2` USB UART y `J3` USB JTAG |
| Esquina trasera derecha | `U3` ESP32-S3 con la antena hacia el lateral derecho, `U14` PCA9555 y los botones BOOT y RESET encima |
| Centro | `J20` expansión (socket 2x8, doble hilera), `U1` driver de motores `DRV8424` con sus condensadores. `J15` IMU en el lado izquierdo, lejos de J20 (unos 50 mm), de los motores y de las bobinas |
| Trasera izquierda | Buck de 5 V (`U7`, `L5`) y buck de 3,3 V (`U6`, `L4`) junto a la batería y `D8` |

- **Motores:** cada motor lleva 2 bornes WAGO 2060-453 (resorte tipo "pinza", SMD, paso 4 mm, 9 A, cable de 0,2 a 0,75 mm²) en fila, es decir 6 polos. El cable entra por el borde de la placa. El orden de los polos va de adelante hacia atrás: 1 M1, 2 GND del encoder, 3 C1, 4 C2, 5 VCC del encoder, 6 M2. Los hilos finos del JGB37-520 (cerca de 26 AWG) quedan por debajo del mínimo de 0,2 mm²: usa casquillos o estaña el extremo antes de apretar.
- **Servos:** headers de 3 pines en ángulo recto (2,54 mm) con los pines hacia afuera: uno al centro del frente y los otros dos en los laterales.

## 3. ESP32-S3-WROOM-1 (antena)

Criterios aplicados (datasheet v1.8, figura 11-1, y guía de Espressif):

- El módulo va en una esquina, con la antena pegada al borde lateral derecho, en una zona sin cobre ni componentes. Hay una zona prohibida para cobre, pistas y vías en todas las capas, de 8 mm de ancho por el largo de la antena (el datasheet marca 6 mm de zona de antena).
- Lo más cercano a la antena son los 2 bornes del motor A, a unos 22 mm hacia el frente, y ninguna pista pasa por debajo.
- El pad de masa del módulo lleva 9 vías (cuadrícula de 3 x 3, ya incluidas en la huella oficial de Espressif) hacia el plano de masa interior.
- Los pines de arranque (GPIO0, GPIO3, GPIO45, GPIO46) y EN quedan como en el esquema. BOOT y RESET están cerca del módulo.
- La antena del nRF24 también tiene una zona prohibida de 12 mm en el borde izquierdo. Las dos antenas quedan en lados opuestos, a más de 100 mm una de otra.

## 4. Vías y disipación según datasheet

- **DRV8424 (U1):** pad térmico de 3,3 x 9,7 mm con una matriz de 3 x 8 vías (huella de TI), según la figura "Example Board Layout" del PWP0028M (pads de 1,5 x 0,45 mm, paso 0,65 mm, 5,8 mm entre filas). Las vías se pasaron de 0,3/0,2 a 0,45/0,2 mm para cumplir el mínimo de JLCPCB.
- **AP64500 (U7):** 6 vías (2 x 3) bajo el pad expuesto de 2,41 x 3,3 mm.
- **SS10U45 (D8):** disipa unos 2 W a plena carga (0,43 V por 5 A). Su lengüeta es el ánodo (batería): deja un área de cobre grande en F.Cu y vías hacia In2.
- **Vías bajo pads:** si dejas vías abiertas bajo el pad térmico, el estaño se escurre. Pide a JLCPCB las vías tapadas ("tented") o rellenas.

## 5. Piezas: cambios por existencias y cálculos

Se consultó el catálogo de JLCPCB el 2026-11-26. Cada pieza tiene existencias de al menos 10 placas. Los cambios fueron:

| Pieza | Antes | Ahora | Por qué |
|---|---|---|---|
| Buck de 5 V (U7) | AP64501SP (C2071517) | **AP64500SP-13 (C2070920)** | AP64501 sin existencias. Mismo pinout salvo el pin 4: ahora RT/CLK con `R78` = 200 kΩ (500 kHz, RT = 100000/fsw), en lugar de `C140` (arranque suave, que el AP64500 trae interno de 4 ms). El resto del circuito del datasheet no cambia (115 k, 22,1 k, 15,8 k, 2,7 nF, 3 x 22 µF). Rizado de la bobina de 3,3 µH: 1,8 A (37 % de 5 A) |
| Buck de 3,3 V (U6) | AP63203WU (C780769) | **AP63200WU-7 (C2071868)** | AP63203 sin existencias. Misma familia y pinout. Se añadió el divisor del pin FB: 196 k (`R79`) y 62 k (`R80`), más 100 pF (`C146`) en paralelo con `R79`; da 0,8 x (1 + 196/62) = 3,33 V. Bobina `L4` de 6,8 µH (tabla del datasheet), rizado de 0,33 A |
| Drivers de LED (U9, U15, U16) | AL8860WT (70 en existencia) | **AL8861WT-7 (C581338)** | Mismo pinout y misma fórmula (corriente = 0,1 / Rs = 333 mA con 0,3 Ω) |
| Adaptador de nivel (U10) | SC-70 | **74AHCT1G125W5-7 (SOT-23-5)** | Existencias y soldadura más fácil. Mismo pinout |
| USB UART (U17) | CH340C C84681 (descontinuado) | **CH340C C7464026** | El primero aparece como "ya no se fabrica" |
| Pulsadores | TL3342 (sin existencias) | **TS-1187A-B-A-B (C318884)** | Pieza básica de JLCPCB |
| Servos y tira | KK-254 (Molex) | **Header de 2,54 mm en ángulo recto (C492411)** | Es el conector real del servo SG90 |
| Motores | JST PH de 6 pines | **WAGO 2060-453 x 2 por motor** | Borne de resorte tipo pinza, como pediste |
| Batería | XT60 vertical | **WAGO 2604-1102 (C3309286)** | El XT60 es muy grueso para la placa. Este borne de resorte admite 12-24 AWG (hasta 4 mm², 20 A) y se pone igual que los motores. La batería con XT60 se conecta cortando el conector y metiendo los cables pelados, o con un pigtail XT60 hembra |
| L5 (5 V) | FEXL0650A (huella dudosa) | **FXL0650-3R3-M (C485362)** | La huella KiCad corresponde a esta serie. 3,3 µH, Isat 10 A |
| L4 (3,3 V) | 4,7 µH 0805 | **FXL0530-6R8-M (C177249)** | 6,8 µH, Isat 4 A, DCR 90 mΩ |
| LED de estado y R17 a R20 | 1 kΩ | **330 Ω**, LED verde y rojo | Con 1 kΩ a 3,3 V el verde apenas se veía (0,4 mA). Con 330 Ω da 1,2 mA (verde) y 3,9 mA (rojo) |
| Condensadores de 22 µF | 10 V | **25 V (X5R 0805)** | A 5 V un X5R de 10 V pierde cerca de 60 % de capacidad por el sesgo DC |

Lista completa con códigos, existencias y precios en `Tesla_BOM.csv` (y el formato de JLCPCB en `Tesla_BOM_JLCPCB.csv`). Costo de componentes de una placa: unos 28 USD. Ojo: hay 46 tipos de pieza "extendida" y JLCPCB cobra un cargo de carga por cada una al ensamblar.

## 6. Protecciones, filtro de la antena nRF24 y desacople

- **TVS en la batería:** `D17` SMBJ15A (15 V de trabajo, 600 W) entre VBAT y GND, junto a `D8`. Recorta los picos de los motores y el desconectar la batería. La batería de 12 V (3S máx. 12,6 V) queda por debajo de los 15 V.
- **TVS en 5 V:** `D18` SMAJ5.0A pegado a la salida del buck de 5 V, por si un servo o la tira devuelven energía.
- **Arreglos ESD (SRV05-4):** `U19` (SERVO_DIR, SERVO_1, SERVO_2, TIRA), `U20` (GPS: SCL, SDA, RX, TX) y `U21` (los 4 encoders), cada uno junto a su conector. `U22` (USBLC6) protege las 2 líneas del LiDAR. Los USB ya tenían `U13` y `U18`.
- **Desacople del nRF24 (hoja "Filtro nRF24"):** solo condensadores, sin ferrita: `C147` de 22 µF y `C150` de 100 nF pegados al pin VCC del socket, en la misma orientación que el pin. Se quitó la ferrita y los condensadores de 10 µF y 1 µF.
- **Desacople repartido (hoja "Proteccion y desacople"):** VBAT `C151`-`C154` (22 µF + 100 nF), +5V `C155`-`C160` (22 µF y 100 nF cerca de cada servo, de la tira y del LiDAR) y +3V3 `C161`-`C164` (cerca de J20, del PCA9555, del GPS y del IMU). Así cada tramo largo de línea tiene un condensador cerca de su carga.

### Comprobación de las fuentes

| Línea | Fuente | Consumo en el peor caso | Margen |
|---|---|---|---|
| +3V3 | AP63200, 2 A | ESP32-S3 en TX ~0,5 A, nRF24 ~0,03 A, PCA9555, IMU, GPS (~0,1 A), encoders (2 x ~0,02 A), LED de estado: cerca de 0,8 a 1,1 A | Sobra más del 40 % |
| +5V | AP64500, 5 A | 3 servos (0,25 A normal, ~0,7 A bloqueado cada uno), LiDAR ~0,15 A, tira LED, expansión y el buck de 3,3 V (~1 A / 5 V x 3,3 V = 0,7 A) | Aguanta los 3 servos en bloqueo + LiDAR + buck. Una tira LED larga a todo brillo puede pasar de los 5 A: limita el brillo por software o usa una alimentación aparte |
| VBAT | Batería + `D8` (10 A) | Motores ~2 x 2 A, faros, bucks (el de 5 V toma ~2 A a 12 V con 5 A de salida): cerca de 8 A pico | D8 (SS10U45, 10 A) y las pistas de 1,5 mm con el plano de VBAT lo soportan |

Los componentes de cada buck (condensador de entrada, bobina, bootstrap, condensadores de salida y divisor de realimentación) se colocan en primer lugar pegados al circuito integrado, y cada etapa tiene su cuadro en la serigrafía.

## 7. Serigrafía

- Los condensadores de desacople van pegados al pin que desacoplan (el pad de su red y el de GND más cercanos) y con la orientación que acerca sus dos pads a esos pines, como en los servos.
- `UART` y `JTAG` van en la cara inferior (B.SilkS), bajo cada conector USB. Los demás nombres van en la cara superior.

- Todos los nombres de pieza están en F.SilkS (1 mm, trazo de 0,2 mm; 0,8 mm en componentes de 0402), puestos por un algoritmo que comprueba que ninguno queda bajo una pieza, sobre un pad, sobre otro texto, fuera del borde ni cruzando un cuadro. Todos los nombres caben y están en F.SilkS. Los textos siempre van horizontales salvo cuando no caben (GPS, de paso fino); las leyendas de un mismo conector comparten orientación y alineación.
- **Cuadros de etapas** con el título dentro: BUCK 5V, BUCK 3V3, DRIVER MOTORES, DRIVER FARO, DRIVER INT IZQ, DRIVER INT DER, EXPANSOR I/O, DESACOPLE nRF24, NIVEL TIRA LED, USB UART, USB JTAG, LED RUEDA A/B y TVS VBAT.
- **Nombre y leyendas:** cada conector tiene su rótulo (SERVO DIR, MOTOR A, BATERIA 12V, TIRA LED, LIDAR, GPS, IMU...) y cada pin su leyenda: servos (- + S), tira (DAT 5V GND), motores (M1 GND C1 C2 3V3 M2), batería (+ -), faros e intermitentes (+ -), GPS, LiDAR e IMU.
- Los bornes de motor y de batería no llevan ningún conector encima: el borne WAGO se suelda directo en la placa y el cable pelado se mete en su resorte. Los hilos finos del motor (menos de 0,2 mm²) necesitan casquillo o estaño.

## 8. Modelos 3D

- DRV8424: el modelo se giró (-90, 0, 0) para que quede plano y alineado con sus pads. La apertura de máscara del pad térmico es de 3,1 x 4,05 mm (antes era del largo completo).
- WAGO 2060 y 2604: modelos 3D sencillos (cajas del tamaño real) en `Tesla_Parts.3dshapes`.

## 9. Pendiente

1. Rutar la placa (comenzar por alimentación y USB), ajustando las zonas de la capa In2.
2. Afinar a mano la colocación de los condensadores de desacople pin por pin al rutar.
3. Revisar la serigrafía en KiCad: quedan unos avisos de serigrafía sobre cobre (JLCPCB recorta lo que cae sobre un pad).
4. Pares diferenciales USB (90 Ω): calcular ancho y separación con el apilado final de JLCPCB (capa 3313 o 7628) antes de rutar `USB_S3_DM`/`DP` y `UD_M`/`UD_P`.
5. Verificar el mecanismo (altura del socket J20 con los condensadores de 7,7 mm, entrada de los cables en los bornes y largo del módulo nRF24).
6. Los 4 agujeros de montaje no están en el esquema (aviso "huella extra" esperado).

## 10. Ajustes hechos a mano en KiCad (26-nov-2026)

El diseno de partida es el que dejo el usuario guardado en KiCad, y `herramientas/pcb_build.py` lo reproduce leyendo `herramientas/ajustes_usuario.json` (se genera con `herramientas/snap_user.py <Tesla.kicad_pcb> ajustes_usuario.json`):

- Posiciones y giros de piezas movidas a mano: U3 (ESP32-S3, desplazado 6,5 mm a la derecha, **la antena queda fuera del contorno**), C131, C132, R52, C163, C159 (girado), U22, U20.
- Con la antena fuera de la placa se elimino la zona prohibida `Antena_ESP32` (la del nRF24 se mantiene).
- Toda la serigrafia de texto es la del usuario: referencias pegadas a su pieza (varias a 0,3-0,5 mm), nombres de J16/J11/J10 en la cara inferior (BATERIA 12V, TIRA LED, LIDAR, GPS M100-PRO) y leyendas de pines de J11/J10 reescritas.
- Pendiente de revisar: DRC avisa de 23 textos con altura menor a 0,8 mm y 11 con grosor menor a 0,08 mm (JLCPCB no los imprime legibles) y de 3 rayas de serigrafia del ESP32 cortadas por el borde.
- Tras mover piezas hay que rellenar zonas (tecla B en KiCad); el archivo entregado ya viene rellenado.

## 11. Puntos de prueba (TP1 a TP9) y compactacion

Solo van los que no se pueden medir directo en un conector (VBAT, salidas de motor, tira LED e I2C se miden en sus bornes/pines). Estan en la hoja Externo (zona derecha), con la huella `TestPoint_Pad_D1.5mm` (no entran en el BOM) y su silk lleva nombre y voltaje esperado:

| TP | Silk | Red | Cerca de |
|---|---|---|---|
| TP1, TP2, TP3 | GND 0V | GND | buck 5V, buck 3V3, expansion J20 |
| TP4 | 5V BUCK 5V | +5V | salida del buck 5V |
| TP5 | 3V3 BUCK 3.3V | +3V3 | salida del buck 3V3 |
| TP6 | EN 3.3V | EN | boton RESET |
| TP7 | BOOT 3.3V | GPIO0 | boton BOOT (0 V = modo descarga) |
| TP8, TP9 | TX 3.3V, RX 3.3V | GPIO43, GPIO44 | USB UART |

Se generan con `herramientas/tp.py` y `pcb_build.py`.

Compactacion (`compact_all` en `pcb_build.py`): cada R, C y Q se acerca lo mas posible a los pads de su misma red (hasta 4,5 mm de busqueda, 4 giros, sin salirse de su cuadro de etapa) y despues se corrigen los solapes. Las referencias se pegan a su pieza (0,15 a 1,4 mm) reduciendo el texto de 1,0 hasta 0,5 mm (0,4 mm si no cabe) antes de alejarlas. Se respetan las piezas que el usuario movio a mano (U3, C131, C132, R52, C163, C159, U22, U20, R21, R22) y sus referencias de `HANDREF`.

## 12. Empaquetado de etapas (estilo del usuario)

`herramientas/sa_pack.py` empaqueta cada etapa por recocido simulado: minimiza el area del grupo y la longitud de los cables entre pads (giros de 90 grados, intercambios y desplazamientos), con 0,12 mm entre piezas y sin salirse de su zona. Va despues de `compact_all` en `pcb_build.py`. Las piezas movidas a mano (lista `locked` en `ajustes_usuario.json`) y sus cuadros y titulos no se tocan. Textos: referencias de 0,6 a 0,4 mm, titulos de etapa de 0,8 mm, nombres de conectores de 0,9 mm.
