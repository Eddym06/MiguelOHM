# Módulos ESP32-S3 oficiales de Espressif

Copiados de [espressif/kicad-libraries](https://github.com/espressif/kicad-libraries) (versión 4.0.0, 2026-11-16, licencia en `LICENSE_espressif_kicad-libraries.md`).
Cada módulo trae la flash y la PSRAM dentro; solo se alimenta a 3,3 V y se conecta EN y los GPIO.

| Símbolo | Huella | Memoria | Antena |
|---|---|---|---|
| `ESP32-S3-WROOM-1-N16R8` | `ESP32-S3-WROOM-1` | 16 MB flash + 8 MB PSRAM octal | Impresa en el módulo |
| `ESP32-S3-WROOM-1U-N16R8` | `ESP32-S3-WROOM-1U` | 16 MB flash + 8 MB PSRAM octal | Conector u.FL |
| `ESP32-S3-WROOM-2-N32R16V` | `ESP32-S3-WROOM-2` | 32 MB flash + 16 MB PSRAM octal (1,8 V) | Impresa en el módulo |

- Con PSRAM octal (R8 y R16V) los GPIO35, GPIO36 y GPIO37 quedan ocupados por el módulo.
- La huella incluye el área prohibida de la antena (`Antenna Area`): no pongas cobre ni componentes debajo.
- El nombre de la variante (N16R8, etc.) solo cambia el valor del símbolo; las huellas son las mismas para todas las variantes de flash y PSRAM de cada módulo.
- Los modelos 3D están en `Espressif_S3.3dshapes/`.
