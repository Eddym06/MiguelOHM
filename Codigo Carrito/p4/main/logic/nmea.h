#pragma once
#include <stdint.h>
#include <stdbool.h>

// Parser NMEA 0183 minimo: GGA (posicion, satelites, hdop) y RMC (velocidad, rumbo).
typedef struct {
    bool     valid;          // fix valido (GGA calidad > 0)
    double   lat, lon;       // grados decimales
    float    alt_m;
    uint8_t  sats;
    uint8_t  quality;        // 0 sin fix, 1 GPS, 2 DGPS, 4/5 RTK...
    float    hdop;
    float    speed_kmh;
    float    course_deg;
    bool     course_valid;   // RMC en estado A
    uint32_t last_gga_ms;
} gps_fix_t;

typedef struct {
    char     line[100];
    uint8_t  len;
    bool     in_line;
    gps_fix_t fix;
    uint32_t ok;             // sentencias validas
    uint32_t bad;            // checksum invalido
} nmea_t;

void nmea_init(nmea_t *n);
// Alimenta un byte. Devuelve true cuando se completo y proceso una sentencia valida.
bool nmea_feed(nmea_t *n, uint8_t byte, uint32_t now_ms);
// Convierte ddmm.mmmm (o dddmm.mmmm) + hemisferio a grados decimales.
double nmea_coord(const char *s, char hemi);
