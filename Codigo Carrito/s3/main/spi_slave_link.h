#pragma once

// Enlace SPI con el P4 (el S3 es esclavo). Intercambia una trama fija cada vez que el P4 la pide.
void spi_slave_link_start(void);
