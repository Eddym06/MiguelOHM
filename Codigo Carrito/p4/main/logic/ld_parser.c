#include "ld_parser.h"
#include <string.h>

void ld_init(ld_parser_t *p)
{
    memset(p, 0, sizeof(*p));
    for (int i = 0; i < LD_SECTORS; i++) {
        p->sector_cm[i] = LD_NO_DATA;
        p->acc_cm[i] = LD_NO_DATA;
    }
    // tabla CRC8, polinomio 0x4D, sin reflejar
    for (int i = 0; i < 256; i++) {
        uint8_t c = (uint8_t)i;
        for (int k = 0; k < 8; k++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x4D) : (uint8_t)(c << 1);
        p->crc_table[i] = c;
    }
}

uint8_t ld_crc8(const ld_parser_t *p, const uint8_t *d, int n)
{
    uint8_t crc = 0;
    for (int i = 0; i < n; i++) crc = p->crc_table[(crc ^ d[i]) & 0xFF];
    return crc;
}

static void process_packet(ld_parser_t *p, uint32_t now_ms)
{
    const uint8_t *b = p->buf;
    p->speed_dps = (uint16_t)(b[2] | (b[3] << 8));
    uint16_t start = (uint16_t)(b[4] | (b[5] << 8));
    uint16_t end = (uint16_t)(b[42] | (b[43] << 8));

    // nueva vuelta: el angulo inicial retrocede
    if (start < p->last_start && p->packets > 0) {
        memcpy(p->sector_cm, p->acc_cm, sizeof(p->sector_cm));
        for (int i = 0; i < LD_SECTORS; i++) p->acc_cm[i] = LD_NO_DATA;
        p->revolutions++;
    }
    p->last_start = start;

    int32_t s = start, e = end;
    if (e < s) e += 36000;
    int32_t step = (e - s) / 11;

    for (int i = 0; i < 12; i++) {
        const uint8_t *pt = &b[6 + i * 3];
        uint16_t mm = (uint16_t)(pt[0] | (pt[1] << 8));
        uint8_t inten = pt[2];
        if (mm == 0 || inten == 0) continue;
        int32_t ang = (s + step * i) % 36000;  // centesimas de grado
        int sec = (int)(ang / 1000);           // 10 grados = 1000 centesimas
        if (sec >= LD_SECTORS) sec = LD_SECTORS - 1;
        uint32_t cm = (uint32_t)(mm / 10);
        if (cm > 0xFFFE) cm = 0xFFFE;
        if (p->acc_cm[sec] == LD_NO_DATA || cm < p->acc_cm[sec]) p->acc_cm[sec] = (uint16_t)cm;
    }
    p->packets++;
    p->last_packet_ms = now_ms;
}

bool ld_feed(ld_parser_t *p, uint8_t b, uint32_t now_ms)
{
    if (p->n == 0) {
        if (b == 0x54) p->buf[p->n++] = b;
        return false;
    }
    if (p->n == 1) {
        if (b == 0x2C) {
            p->buf[p->n++] = b;
        } else {
            p->n = (b == 0x54) ? 1 : 0;
            if (p->n == 1) p->buf[0] = b;
        }
        return false;
    }
    p->buf[p->n++] = b;
    if (p->n < LD_PACKET_LEN) return false;

    p->n = 0;
    if (ld_crc8(p, p->buf, LD_PACKET_LEN - 1) != p->buf[LD_PACKET_LEN - 1]) {
        p->crc_err++;
        return false;
    }
    process_packet(p, now_ms);
    return true;
}
