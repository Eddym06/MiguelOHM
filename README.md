# MiguelOHM

Proyecto de la materia de **Microcontroladores**: un carrito controlado a distancia, con su propio
control remoto, navegación autónoma por GPS y una página web de control desde el celular.

```
 Control remoto ──433 MHz──► ESP32-P4 ══SPI══ ESP32-S3 ──WiFi──► Celular (página web)
 (ESP32-S3 + OLED)           (cerebro del carrito)  (puerta de enlace)
```

## Qué hace

- **Control remoto** con dos joysticks, modo giroscopio, freno motor, control de luces y un OLED con
  6 pantallas (velocímetro, RPM, inclinación, estado del enlace…).
- **Carrito** con dos motores con encoder (PID de velocidad, giro por motor + servo), faros, LEDs RGB en
  las ruedas, tira LED trasera estilo F1, GPS, LIDAR, IMU y medición de batería.
- **Página web** servida por el propio carrito: control táctil, mapa con brújula, ajustes, pruebas de
  componentes y modo autónomo (el carrito va a un punto marcado en el mapa esquivando obstáculos).
- El enlace 433 MHz y el WiFi **nunca están activos a la vez**: el primero en conectarse apaga al otro.

## Estructura

| Carpeta | Contenido |
|---|---|
| [Placa_Control_Remoto/](Placa_Control_Remoto/) | PCB del control (KiCad: `KAKATA_RC433_V1`) y su firmware en [Codigo/](Placa_Control_Remoto/Codigo/README.md) (PlatformIO, Arduino, ESP32-S3). |
| [Codigo Carrito/](Codigo%20Carrito/README.md) | Firmware del carrito en ESP-IDF: `p4/` (cerebro), `s3/` (WiFi + web), `shared/` (protocolos) y `tests/`. |
| [Diseño Caja 3D/](Diseño%20Caja%203D/) | Cajas para impresión 3D (FreeCAD). |
| [Placa_Carrito/](Placa_Carrito/) | Diseño mecánico del carrito. |
| [Documentación/](Documentación/) | Documento del proyecto y registro de cambios. |

## Empezar

- **Control remoto:** abrir `Placa_Control_Remoto/Codigo` con PlatformIO y `pio run -t upload`.
- **Carrito:** necesita ESP-IDF 5.5 (P4) y 5.3 o superior (S3). Instrucciones, tabla de pines pendientes y
  lista de lo que falta validar en [Codigo Carrito/README.md](Codigo%20Carrito/README.md).
- **Pruebas de la lógica del carrito en la PC:** `bash "Codigo Carrito/tests/run_host_tests.sh"`.

## Estado

| Parte | Estado |
|---|---|
| Firmware del control remoto | Compila. Pendiente de probar en la PCB. |
| Firmware del carrito (P4 y S3) | Compila; lógica pura probada en la PC (97 verificaciones). Pines pendientes de la PCB del carrito. |
| Página web | Revisada en navegador con datos simulados. Pendiente de probar con el carrito real. |

Antes de las pruebas con hardware, revisar la sección «Sin validar en hardware» del README del carrito
(sentido de ejes y motores, encoders, PID, protocolo del LIDAR y del GPS).

## Simulación

[Modelos y resultados por placa](Simulación/README.md).
