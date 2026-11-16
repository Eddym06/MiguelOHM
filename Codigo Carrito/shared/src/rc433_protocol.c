#include "rc433_protocol.h"

const rc433_color_t RC433_COLORS[RC433_COLOR_COUNT] = {
    {"ROJO",     255,   0,   0},
    {"NARANJA",  255,  90,   0},
    {"AMARILLO", 255, 200,   0},
    {"VERDE",      0, 255,   0},
    {"CIAN",       0, 255, 255},
    {"AZUL",       0,   0, 255},
    {"VIOLETA",  150,   0, 255},
    {"BLANCO",   255, 255, 255},
    {"ARCOIRIS",   0,   0,   0},
};

uint8_t rc433_crc8(const uint8_t *d, uint8_t n)
{
    uint8_t c = 0;
    while (n--) {
        c ^= *d++;
        for (uint8_t i = 0; i < 8; i++) {
            c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x07) : (uint8_t)(c << 1);
        }
    }
    return c;
}

// Manchester: bit 1 -> 10, bit 0 -> 01
uint16_t rc433_manchester_encode(uint8_t b)
{
    uint16_t v = 0;
    for (int i = 7; i >= 0; i--) {
        v = (uint16_t)((v << 2) | (((b >> i) & 1) ? 0x2 : 0x1));
    }
    return v;
}

bool rc433_manchester_decode(uint16_t v, uint8_t *out)
{
    uint8_t b = 0;
    for (int i = 7; i >= 0; i--) {
        uint8_t p = (v >> (2 * i)) & 0x3;
        if (p == 0x2) {
            b |= (uint8_t)(1 << i);
        } else if (p != 0x1) {
            return false;
        }
    }
    *out = b;
    return true;
}

size_t rc433_build(uint8_t type, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t out[RC433_WIRE_LEN])
{
    uint8_t raw[RC433_FRAME_LEN] = {type, b1, b2, b3, 0};
    raw[RC433_FRAME_LEN - 1] = rc433_crc8(raw, RC433_FRAME_LEN - 1);

    size_t k = 0;
    for (int i = 0; i < RC433_PREAMBLE_LEN; i++) out[k++] = RC433_PREAMBLE_BYTE;
    out[k++] = RC433_SYNC0;
    out[k++] = RC433_SYNC1;
    for (int i = 0; i < RC433_FRAME_LEN; i++) {
        uint16_t m = rc433_manchester_encode(raw[i]);
        out[k++] = (uint8_t)(m >> 8);
        out[k++] = (uint8_t)(m & 0xFF);
    }
    return k;
}

enum { ST_SEARCH = 0, ST_PAYLOAD = 1 };

void rc433_parser_init(rc433_parser_t *p)
{
    p->state = ST_SEARCH;
    p->prev = 0;
    p->n = 0;
    p->start_ms = 0;
}

rc433_result_t rc433_parser_feed(rc433_parser_t *p, uint8_t b, uint32_t now_ms,
                                 uint8_t raw[RC433_FRAME_LEN])
{
    if (p->state == ST_PAYLOAD && (now_ms - p->start_ms) > 100) {
        p->state = ST_SEARCH;  // trama incompleta: descartar
        p->prev = 0;
    }

    if (p->state == ST_SEARCH) {
        if (p->prev == RC433_SYNC0 && b == RC433_SYNC1) {
            p->state = ST_PAYLOAD;
            p->n = 0;
            p->start_ms = now_ms;
        }
        p->prev = b;
        return RC433_RES_NONE;
    }

    p->buf[p->n++] = b;
    if (p->n < 2 * RC433_FRAME_LEN) return RC433_RES_NONE;

    p->state = ST_SEARCH;
    p->prev = 0;

    for (int i = 0; i < RC433_FRAME_LEN; i++) {
        uint16_t v = (uint16_t)((p->buf[2 * i] << 8) | p->buf[2 * i + 1]);
        if (!rc433_manchester_decode(v, &raw[i])) return RC433_RES_BAD;
    }
    if (rc433_crc8(raw, RC433_FRAME_LEN - 1) != raw[RC433_FRAME_LEN - 1]) return RC433_RES_BAD;
    if (raw[0] == RC433_TYPE_CMD) return RC433_RES_CMD;
    if (raw[0] == RC433_TYPE_TELEM) return RC433_RES_TELEM;
    return RC433_RES_BAD;
}
