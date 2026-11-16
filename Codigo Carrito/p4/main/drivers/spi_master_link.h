#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "spi_link.h"

// Enlace SPI con el ESP32-S3 (puerta de enlace WiFi/web). El P4 es el maestro y
// intercambia una trama fija cada SPI_LINK_PERIOD_MS.
void spi_link_master_start(void);
// Ultima orden valida recibida del S3. age_ms = tiempo desde que llego. false si nunca llego.
bool spi_link_master_get(s3_to_p4_t *out, uint32_t *age_ms);
