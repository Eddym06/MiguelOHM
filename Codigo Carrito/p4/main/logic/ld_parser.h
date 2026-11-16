#pragma once
#include <stdint.h>
#include <stdbool.h>

// Parser del LIDAR LDROBOT serie D (D6 / LD06 / LD19): UART 230400 8N1.
// Paquete de 47 bytes: 0x54 0x2C, velocidad, angulo inicial, 12 puntos (dist mm + intensidad),
// angulo final, timestamp, CRC8 (polinomio 0x4D).
// Se mantiene la distancia minima por sector de 10 grados (sentido horario, 0 = frente del sensor).

#define LD_SECTORS     36
#define LD_PACKET_LEN  47
#define LD_NO_DATA     0xFFFF

typedef struct {
    uint8_t  buf[LD_PACKET_LEN];
    uint8_t  n;
    uint16_t sector_cm[LD_SECTORS];   // ultima vuelta completa
    uint16_t acc_cm[LD_SECTORS];      // vuelta en curso
    uint16_t last_start;              // angulo inicial del paquete anterior (centesimas de grado)
    uint32_t packets;
    uint32_t crc_err;
    uint32_t revolutions;
    uint32_t last_packet_ms;
    uint16_t speed_dps;
    uint8_t  crc_table[256];
} ld_parser_t;

void ld_init(ld_parser_t *p);
// true cuando se completo un paquete valido
bool ld_feed(ld_parser_t *p, uint8_t b, uint32_t now_ms);
uint8_t ld_crc8(const ld_parser_t *p, const uint8_t *d, int n);
