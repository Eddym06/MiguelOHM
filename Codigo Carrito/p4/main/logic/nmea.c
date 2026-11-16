#include "nmea.h"
#include <stdlib.h>
#include <string.h>

void nmea_init(nmea_t *n)
{
    memset(n, 0, sizeof(*n));
}

double nmea_coord(const char *s, char hemi)
{
    if (!s || !*s) return 0;
    double v = atof(s);
    int deg = (int)(v / 100.0);
    double min = v - deg * 100.0;
    double d = deg + min / 60.0;
    if (hemi == 'S' || hemi == 'W') d = -d;
    return d;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool process_line(nmea_t *n, char *line, uint32_t now_ms)
{
    // line: "$GNGGA,...*hh"
    char *star = strchr(line, '*');
    if (!star || line[0] != '$') return false;
    uint8_t cs = 0;
    for (char *p = line + 1; p < star; p++) cs ^= (uint8_t)*p;
    int hi = hexval(star[1]), lo = hexval(star[2]);
    if (hi < 0 || lo < 0 || cs != (uint8_t)(hi * 16 + lo)) {
        n->bad++;
        return false;
    }
    *star = '\0';

    char *f[20];
    int nf = 0;
    f[nf++] = line + 1;
    for (char *p = line + 1; *p && nf < 20; p++) {
        if (*p == ',') {
            *p = '\0';
            f[nf++] = p + 1;
        }
    }
    if (strlen(f[0]) < 5) return false;
    const char *type = f[0] + 2;  // salta el talker (GP, GN, GL...)

    if (strcmp(type, "GGA") == 0 && nf >= 10) {
        n->fix.quality = (uint8_t)atoi(f[6]);
        n->fix.valid = n->fix.quality > 0;
        n->fix.sats = (uint8_t)atoi(f[7]);
        n->fix.hdop = (float)atof(f[8]);
        n->fix.alt_m = (float)atof(f[9]);
        if (n->fix.valid && f[2][0] && f[4][0]) {
            n->fix.lat = nmea_coord(f[2], f[3][0]);
            n->fix.lon = nmea_coord(f[4], f[5][0]);
        }
        n->fix.last_gga_ms = now_ms;
        n->ok++;
        return true;
    }
    if (strcmp(type, "RMC") == 0 && nf >= 9) {
        bool a = f[2][0] == 'A';
        n->fix.course_valid = a;
        if (a) {
            n->fix.speed_kmh = (float)(atof(f[7]) * 1.852);
            if (f[8][0]) n->fix.course_deg = (float)atof(f[8]);
        } else {
            n->fix.speed_kmh = 0;
        }
        n->ok++;
        return true;
    }
    return false;
}

bool nmea_feed(nmea_t *n, uint8_t b, uint32_t now_ms)
{
    if (b == '$') {
        n->in_line = true;
        n->len = 0;
    }
    if (!n->in_line) return false;

    if (b == '\r' || b == '\n') {
        n->in_line = false;
        if (n->len < 6) return false;
        n->line[n->len] = '\0';
        return process_line(n, n->line, now_ms);
    }
    if (n->len < sizeof(n->line) - 1) {
        n->line[n->len++] = (char)b;
    } else {
        n->in_line = false;  // linea demasiado larga: descartar
    }
    return false;
}
