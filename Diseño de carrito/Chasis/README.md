# Chasis del carrito MiguelOHM

Modelo paramétrico en FreeCAD 26.3 del chasis completo: marco de perfil 2020, tracción trasera con dos JGB37-520,
dirección Ackermann con servo, zonas de sensores / lógica / potencia y alerón estilo F1 con canal para la tira WS2812.

**Medidas generales:** 350 × 272 × 190 mm · batalla 230 mm · vía 234 mm · distancia al suelo 26 mm (punto más bajo: tornillo del kingpin).

## Archivos

| Archivo | Qué es |
|---|---|
| `params.json` | **Todas** las medidas. Es lo único que hay que editar. |
| `build_chasis.py` | Genera el ensamble desde `params.json`. |
| `validar.py` | Revisa el ensamble y muestra una tabla PASA/FALLA. |
| `colorear_por_zona.FCMacro` | Colorea el modelo por zona dentro de FreeCAD. |
| `carrito_ensamble.FCStd` | El ensamble (73 sólidos, en grupos por etapa). |
| `stl/` | Una STL por pieza impresa. |
| `export/carrito_completo.step` | Ensamble en STEP, para quien no use FreeCAD. |
| `export/placa_base_corte.dxf` | Contorno y agujeros de la placa base, para corte láser. |
| `export/validacion.txt` | Resultado de la última validación. |

## Regenerar

En una terminal, desde esta carpeta (`freecadcmd.exe` está en la carpeta `bin` de FreeCAD):

```
freecadcmd build_chasis.py
freecadcmd build_partdesign.py
freecadcmd validar.py
```

`build_partdesign.py` rehace las 30 piezas propias como cuerpos de **Part Design** con 84 croquis totalmente
restringidos (pad, pocket, redondeos), compara cada volumen con la pieza original y la reemplaza. El alerón usa un
perfil NACA invertido (4 % de curvatura, espesor 8 mm).

Para construir solo hasta una etapa: `set ETAPA=3` (Windows) o `ETAPA=3` (Linux) antes del comando.
Si ejecutas `build_chasis.py` desde FreeCAD (Macro → Macros… → Ejecutar), el modelo queda coloreado por zona.
Si abres el `.FCStd` directamente, ejecuta `colorear_por_zona.FCMacro` y guarda.

Colores: sensores azul · lógica violeta · potencia rojo · alerón índigo · motores y servo naranja · estructura gris.

## Sistema de coordenadas

X=0 en la cara delantera del marco, +X hacia atrás. Y=0 en el centro, +Y hacia la izquierda. Z=0 en el suelo.

## Etapas

1. **Marco:** 2 perfiles 2020 × 300, 3 travesaños × 130, 8 escuadras y placa base de 300 × 170 × 3 en acrílico.
2. **Tracción:** motores JGB37-520 colgados bajo los perfiles, soporte impreso con **holgura de 2,5 mm** (cuna Ø39,5, centrador Ø14,5 y ranuras M3 de ±1,25 mm), ruedas de Ø85 con acople hexagonal de 6 mm.
3. **Dirección:** bandeja delantera con horquillas, manguetas con 2 rodamientos 626ZZ, kingpin M4, brazos Ackermann a 18,3°, barra de dirección y servo MG996R invertido. Giro de ±25° comprobado sin choques.
4. **Zonas:** morro con 2 faros de 20 mm, mástil del LiDAR D500 (base a 155 mm), PCB de 100 × 80 sobre separadores de 10 mm, mástil del GPS M100 PRO (a 180 mm de los motores), cuna de la batería colgada, driver L298N, interruptor, portafusible, 4 LED RGB de esquina y antena.
5. **Alerón F1:** pieza única de 190 mm de envergadura con perfil invertido, flap a 20°, barra con canal de 180 × 10,5 × 3 mm para la tira WS2812 y pilón tipo cuello de cisne con paso de cable interno.

## Piezas a imprimir

| STL | Cantidad | Nota |
|---|---|---|
| `Soporte_motor_A.stl` | 2 | La segunda, espejada en el slicer (eje Y). Acostada sobre la placa, relleno ≥40 %. |
| `Mangueta_Izq.stl` | 2 | La segunda, espejada. Relleno ≥50 %, los rodamientos entran a presión. |
| `Bandeja_delantera.stl` | 1 | Con soportes bajo las orejas. |
| `Barra_direccion.stl` | 1 | |
| `Morro.stl` | 1 | |
| `Mastil_lidar.stl` | 1 | |
| `Mastil_GPS.stl` | 1 | |
| `Cuna_bateria.stl` | 1 | |
| `Aleron.stl` | 1 | Imprimir boca abajo con soportes. |
| `Soporte_LED_DIzq.stl` | 2 | Los 4 soportes LED son iguales. |
| `Soporte_LED_TIzq.stl` | 2 | |
| `Soporte_antena.stl` | 1 | |

Todas caben en una cama de 220 × 220 mm.

## Piezas a comprar

- Perfil de aluminio 2020: 2 × 300 mm y 3 × 130 mm, más 8 escuadras de 20 mm.
- Unas 45 tuercas en T M5 con tornillos M5 × 8/10 (marco, soportes, bandeja, cuna, morro y placa).
- Acrílico de 3 mm, 300 × 170 mm, cortado con `placa_base_corte.dxf`.
- 4 rodamientos 626ZZ, 2 tornillos M4 × 55 con tuerca autoblocante, 2 ejes de 6 mm (varilla o tornillo M6) de unos 25 mm.
- 4 acoples hexagonales para eje de 6 mm.
- Tornillería M3: 6 por motor (12 en total), separadores de 10 mm para la PCB y de 3 mm para el driver.
- Servo MG996R (el esquemático usa SG90; ver nota).

## Valores por verificar con calibrador

Ruedas (Ø85 × 38), batería (105 × 34 × 25), largo de la caja reductora del motor (24 mm; depende de la relación,
va de 19 a 29) y patrón de agujeros del LiDAR. Cambia el valor en `params.json`, regenera y valida.

## Notas

- **Servo:** con dos motores de 37 mm y batería 3S el carrito pesa más de 1 kg; un SG90 no alcanza para la dirección. El soporte está hecho para un MG996R. Para volver al SG90, cambia `"servo"` en `params.json`.
- **Driver:** `"driver_externo": true` reserva espacio para un módulo L298N. Si se usa el DRV8424 de la PCB, pon `false`.
- **LiDAR:** nada del carrito sube por encima de Z=152 mm, para que el alerón y la antena no tapen el escaneo a 360°.
- Medición de cajas envolventes: FreeCAD 26.3 (OCCT 8) invierte cuál de `BoundBox` y `optimalBoundingBox()` es la exacta respecto a la 1.1. Los scripts usan la intersección de las dos.
