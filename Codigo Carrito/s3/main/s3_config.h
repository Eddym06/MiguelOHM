#pragma once
// =====================================================================================
//  CONFIGURACION DEL ESP32-S3 (puerta de enlace WiFi). Pines PENDIENTES DE LA PCB.
// =====================================================================================

#define PIN_NC   (-1)
// mascara de bit de un pin (segura aunque el pin sea -1 en tiempo de compilacion)
#define PIN_BIT(p)  (1ULL << ((p) < 0 ? 0 : (p)))

// WiFi propio del carrito: el celular se conecta a esta red y abre http://192.168.4.1
#define AP_SSID        "KAKATA-CARRITO"
#define AP_PASSWORD    "kakata1234"      // minimo 8 caracteres
#define AP_CHANNEL     6
#define AP_MAX_CLIENTS 2

// Enlace SPI con el P4 (el S3 es esclavo)
#define PIN_SPI_SCLK   PIN_NC
#define PIN_SPI_MOSI   PIN_NC
#define PIN_SPI_MISO   PIN_NC
#define PIN_SPI_CS     PIN_NC
#define PIN_SPI_HS     PIN_NC    // "datos listos": sube cuando la trama esta preparada (hacia el P4)
