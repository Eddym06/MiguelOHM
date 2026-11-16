#pragma once
// Protocolo RF 433 MHz control <-> carrito (version C de Control Remoto/Codigo/include/protocol.h).
// Ambos archivos deben mantenerse identicos en valores.
//
// Capa fisica: UART 4800 baud 8N1 con la linea INVERTIDA (reposo = LOW = sin portadora).
// Trama: [0xAA x4][0x2D 0xD4][5 bytes Manchester: type,b1,b2,b3,crc8]  -> 16 bytes, ~33 ms.
// Ciclo de 100 ms: el control manda el comando y el carrito responde con telemetria.

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RC433_BAUD            4800
#define RC433_PREAMBLE_BYTE   0xAA
#define RC433_PREAMBLE_LEN    4
#define RC433_SYNC0           0x2D
#define RC433_SYNC1           0xD4
#define RC433_FRAME_LEN       5
#define RC433_WIRE_LEN        (RC433_PREAMBLE_LEN + 2 + 2 * RC433_FRAME_LEN)
#define RC433_AIRTIME_MS      ((RC433_WIRE_LEN * 10 * 1000) / RC433_BAUD + 1)

#define RC433_TYPE_CMD        0xC1
#define RC433_TYPE_TELEM      0xE1

#define RC433_F_HEADLIGHTS    0x01
#define RC433_F_RGB_ON        0x02
#define RC433_F_GYRO          0x04
#define RC433_F_BRAKE         0x08
#define RC433_COLOR_SHIFT     4

#define RC433_COLOR_COUNT     9
#define RC433_COLOR_RAINBOW   8

typedef struct {
    int8_t  throttle;   // -100..100
    int8_t  steer;      // -100..100
    uint8_t flags;
} rc433_cmd_t;

typedef struct {
    uint8_t speed_x10;  // km/h * 10
    uint8_t rpm_div4;   // rpm / 4
    uint8_t batt_pct;   // 0xFF = desconocida
} rc433_telem_t;

typedef struct {
    const char *name;
    uint8_t r, g, b;
} rc433_color_t;

extern const rc433_color_t RC433_COLORS[RC433_COLOR_COUNT];

uint8_t  rc433_crc8(const uint8_t *d, uint8_t n);
uint16_t rc433_manchester_encode(uint8_t b);
bool     rc433_manchester_decode(uint16_t v, uint8_t *out);

// Arma la trama completa lista para el UART. Devuelve RC433_WIRE_LEN.
size_t rc433_build(uint8_t type, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t out[RC433_WIRE_LEN]);

typedef enum {
    RC433_RES_NONE = 0,
    RC433_RES_CMD,
    RC433_RES_TELEM,
    RC433_RES_BAD,
} rc433_result_t;

typedef struct {
    uint8_t  state;
    uint8_t  prev;
    uint8_t  buf[2 * RC433_FRAME_LEN];
    uint8_t  n;
    uint32_t start_ms;
} rc433_parser_t;

void rc433_parser_init(rc433_parser_t *p);

// Alimenta un byte del UART. Cuando devuelve CMD/TELEM, raw[] trae los 5 bytes decodificados.
rc433_result_t rc433_parser_feed(rc433_parser_t *p, uint8_t b, uint32_t now_ms,
                                 uint8_t raw[RC433_FRAME_LEN]);

#ifdef __cplusplus
}
#endif
