// Pruebas en PC de la logica pura del carrito (sin hardware ni ESP-IDF).
// Compilar y correr:  tests/run_host_tests.sh   (o ver README)
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "rc433_protocol.h"
#include "spi_link.h"
#include "settings_table.h"
#include "pid.h"
#include "drive_mix.h"
#include "battery.h"
#include "nmea.h"
#include "ld_parser.h"
#include "heading.h"
#include "nav.h"
#include "spoiler_fx.h"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond) do { if (cond) { g_pass++; } else { g_fail++; printf("  FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define NEAR(a, b, tol) CHECK(fabs((double)(a) - (double)(b)) <= (tol))

static void test_protocol(void)
{
    printf("protocolo 433\n");
    uint8_t wire[RC433_WIRE_LEN];
    CHECK(rc433_build(RC433_TYPE_CMD, (uint8_t)(int8_t)-55, (uint8_t)(int8_t)90,
                      RC433_F_HEADLIGHTS | RC433_F_BRAKE | (5 << RC433_COLOR_SHIFT), wire) == RC433_WIRE_LEN);

    rc433_parser_t p;
    rc433_parser_init(&p);
    uint8_t raw[RC433_FRAME_LEN];
    rc433_result_t r = RC433_RES_NONE;
    // ruido previo + trama
    const uint8_t noise[] = {0x13, 0x77, 0x2D, 0x00, 0xFF};
    for (size_t i = 0; i < sizeof(noise); i++) rc433_parser_feed(&p, noise[i], 0, raw);
    for (int i = 0; i < RC433_WIRE_LEN; i++) {
        r = rc433_parser_feed(&p, wire[i], (uint32_t)i, raw);
    }
    CHECK(r == RC433_RES_CMD);
    CHECK((int8_t)raw[1] == -55);
    CHECK((int8_t)raw[2] == 90);
    CHECK(raw[3] & RC433_F_BRAKE);
    CHECK((raw[3] >> RC433_COLOR_SHIFT) == 5);

    // trama corrupta -> BAD
    rc433_build(RC433_TYPE_TELEM, 100, 160, 77, wire);
    wire[9] ^= 0x01;
    rc433_parser_init(&p);
    r = RC433_RES_NONE;
    for (int i = 0; i < RC433_WIRE_LEN; i++) {
        rc433_result_t x = rc433_parser_feed(&p, wire[i], (uint32_t)i, raw);
        if (x != RC433_RES_NONE) r = x;
    }
    CHECK(r == RC433_RES_BAD);

    // telemetria valida
    rc433_build(RC433_TYPE_TELEM, 124, 160, 77, wire);
    rc433_parser_init(&p);
    for (int i = 0; i < RC433_WIRE_LEN; i++) {
        rc433_result_t x = rc433_parser_feed(&p, wire[i], (uint32_t)i, raw);
        if (x != RC433_RES_NONE) r = x;
    }
    CHECK(r == RC433_RES_TELEM && raw[1] == 124 && raw[2] == 160 && raw[3] == 77);
    // 16 bytes a 4800 baud ~ 33 ms
    CHECK(RC433_AIRTIME_MS >= 33 && RC433_AIRTIME_MS <= 35);
}

static void test_spi(void)
{
    printf("enlace SPI\n");
    p4_to_s3_t a;
    memset(&a, 0, sizeof(a));
    a.magic = SPI_LINK_MAGIC;
    a.seq = 7;
    a.vbat_mv = 11900;
    a.lat_e7 = -123456789;
    a.lidar_cm[35] = 321;
    uint8_t buf[SPI_LINK_FRAME_SIZE];
    spi_link_seal(buf, &a, sizeof(a));
    p4_to_s3_t b;
    CHECK(spi_link_open(buf, &b, sizeof(b)));
    CHECK(b.vbat_mv == 11900 && b.lat_e7 == -123456789 && b.lidar_cm[35] == 321);
    buf[20] ^= 0x40;
    CHECK(!spi_link_open(buf, &b, sizeof(b)));

    s3_to_p4_t c;
    memset(&c, 0, sizeof(c));
    c.magic = SPI_LINK_MAGIC;
    c.throttle = -42;
    c.cfg_val = 1.25f;
    spi_link_seal(buf, &c, sizeof(c));
    s3_to_p4_t d;
    CHECK(spi_link_open(buf, &d, sizeof(d)) && d.throttle == -42 && d.cfg_val == 1.25f);
    printf("  tamanos: p4_to_s3=%zu s3_to_p4=%zu de %d\n", sizeof(a), sizeof(c), SPI_LINK_FRAME_SIZE);
    CHECK(SET_COUNT > 20);
}

// Planta simple de un motor: primer orden con ganancia y constante de tiempo
static float plant_step(float rpm, float duty, float gain_rpm, float dt)
{
    return rpm + (gain_rpm * duty - rpm) / 0.15f * dt;
}

static void test_pid_and_sync(void)
{
    printf("PID + sincronia\n");
    const float rpm_max = 1000, dt = 0.02f;
    pid_ctl_t pl, pr;
    pid_init(&pl, 1.2f, 3.0f, 0, 0, 1);
    pid_init(&pr, 1.2f, 3.0f, 0, 0, 1);
    float rl = 0, rr = 0;
    float tgt = 500;
    // dos motores distintos: uno rinde 10 % menos
    for (int i = 0; i < 300; i++) {
        float sl = tgt, sr = tgt;
        sync_adjust(rl, rr, &sl, &sr, 0.5f, rpm_max);
        float dl = pid_step(&pl, sl / rpm_max, rl / rpm_max, 0.9f * sl / rpm_max, dt);
        float dr = pid_step(&pr, sr / rpm_max, rr / rpm_max, 0.9f * sr / rpm_max, dt);
        rl = plant_step(rl, dl, 1000, dt);
        rr = plant_step(rr, dr, 900, dt);
    }
    printf("  rpm L=%.1f R=%.1f (objetivo %.0f)\n", rl, rr, tgt);
    NEAR(rl, tgt, 8);
    NEAR(rr, tgt, 8);
    NEAR(rl - rr, 0, 8);

    // saturacion: objetivo inalcanzable no debe disparar el integrador (anti-windup)
    pid_ctl_t ps;
    pid_init(&ps, 1.2f, 3.0f, 0, 0, 1);
    for (int i = 0; i < 500; i++) pid_step(&ps, 2.0f, 0, 0, dt);
    CHECK(ps.integ < 1.5f);

    // sync_adjust: si la izquierda va mas rapida de lo pedido, baja su setpoint y sube el de la derecha
    float a = 500, b = 500;
    sync_adjust(560, 500, &a, &b, 0.5f, 1000);
    CHECK(a < 500 && b > 500);
    // si ambas siguen el pedido, no cambia
    a = 500; b = 400;
    sync_adjust(500, 400, &a, &b, 0.5f, 1000);
    NEAR(a, 500, 1e-3);
    NEAR(b, 400, 1e-3);
}

static void test_mix(void)
{
    printf("mezcla diferencial y rampas\n");
    mix_cfg_t c = {1000.0f, 0.35f};
    float l, r;
    drive_mix(50, 0, &c, &l, &r);
    NEAR(l, 500, 1e-3); NEAR(r, 500, 1e-3);
    drive_mix(50, 100, &c, &l, &r);          // giro derecha: izquierda mas rapida
    CHECK(l > r);
    NEAR(l, 675, 1e-2); NEAR(r, 325, 1e-2);
    drive_mix(100, 100, &c, &l, &r);         // saturacion: se escala, proporcion intacta
    NEAR(fmaxf(fabsf(l), fabsf(r)), 1000, 1e-2);
    NEAR(l / r, 1.35 / 0.65, 1e-3);
    drive_mix(-50, 100, &c, &l, &r);         // reversa, giro derecha: izquierda sigue siendo mayor en magnitud
    CHECK(fabsf(l) > fabsf(r) && l < 0 && r < 0);
    drive_mix(0, 100, &c, &l, &r);           // sin acelerador no gira sobre su eje
    NEAR(l, 0, 1e-6); NEAR(r, 0, 1e-6);

    NEAR(ramp_toward(0, 100, 10), 10, 1e-6);
    NEAR(ramp_toward(95, 100, 10), 100, 1e-6);
    NEAR(ramp_toward(0, -100, 10), -10, 1e-6);
    NEAR(duty_deadband(0.5f, 0.1f), 0.55, 1e-6);
    NEAR(duty_deadband(-1.0f, 0.1f), -1.0, 1e-6);
    NEAR(duty_deadband(0.0f, 0.1f), 0, 1e-6);
}

static void test_battery(void)
{
    printf("bateria\n");
    CHECK(battery_percent_li(12.6f, 3) == 100);
    CHECK(battery_percent_li(9.0f, 3) == 0);
    uint8_t p = battery_percent_li(11.1f, 3);
    CHECK(p >= 10 && p <= 15);
    CHECK(battery_percent_li(4.2f, 1) == 100);
}

static void test_nmea(void)
{
    printf("NMEA\n");
    nmea_t n;
    nmea_init(&n);
    const char *gga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    const char *rmc = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
    bool got = false;
    for (const char *p = gga; *p; p++) got |= nmea_feed(&n, (uint8_t)*p, 1000);
    CHECK(got);
    CHECK(n.fix.valid && n.fix.sats == 8 && n.fix.quality == 1);
    NEAR(n.fix.lat, 48.1173, 1e-4);
    NEAR(n.fix.lon, 11.516667, 1e-4);
    NEAR(n.fix.hdop, 0.9, 1e-3);
    got = false;
    for (const char *p = rmc; *p; p++) got |= nmea_feed(&n, (uint8_t)*p, 1100);
    CHECK(got);
    NEAR(n.fix.speed_kmh, 22.4 * 1.852, 1e-2);
    NEAR(n.fix.course_deg, 84.4, 1e-2);
    CHECK(n.fix.course_valid);

    // hemisferios sur / oeste
    NEAR(nmea_coord("3355.5000", 'S'), -(33 + 55.5 / 60.0), 1e-6);
    NEAR(nmea_coord("07012.0000", 'W'), -(70 + 12.0 / 60.0), 1e-6);

    // checksum malo se descarta
    const char *bad = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*00\r\n";
    uint32_t okBefore = n.ok;
    for (const char *p = bad; *p; p++) nmea_feed(&n, (uint8_t)*p, 1200);
    CHECK(n.ok == okBefore && n.bad == 1);
}

static void ld_make_packet(const ld_parser_t *p, uint8_t out[LD_PACKET_LEN], uint16_t start, uint16_t end,
                           uint16_t mm, uint8_t inten)
{
    memset(out, 0, LD_PACKET_LEN);
    out[0] = 0x54; out[1] = 0x2C;
    out[2] = 0x10; out[3] = 0x0E;                       // 3600 grados/s
    out[4] = (uint8_t)(start & 0xFF); out[5] = (uint8_t)(start >> 8);
    for (int i = 0; i < 12; i++) {
        out[6 + i * 3] = (uint8_t)(mm & 0xFF);
        out[7 + i * 3] = (uint8_t)(mm >> 8);
        out[8 + i * 3] = inten;
    }
    out[42] = (uint8_t)(end & 0xFF); out[43] = (uint8_t)(end >> 8);
    out[46] = ld_crc8(p, out, LD_PACKET_LEN - 1);
}

static void test_lidar(void)
{
    printf("LIDAR\n");
    ld_parser_t p;
    ld_init(&p);
    CHECK(p.crc_table[1] == 0x4D && p.crc_table[2] == 0x9A && p.crc_table[3] == 0xD7);

    uint8_t pkt[LD_PACKET_LEN];
    bool ok = false;
    // paquete A al final de la vuelta (350 a 361 grados), distancia 500 mm
    ld_make_packet(&p, pkt, 35000, 36100, 500, 100);
    for (int i = 0; i < 3; i++) ld_feed(&p, 0xAA, 0);  // basura previa
    for (int i = 0; i < LD_PACKET_LEN; i++) ok = ld_feed(&p, pkt[i], 10);
    CHECK(ok && p.packets == 1);
    // paquete B empieza otra vuelta -> publica la anterior
    ld_make_packet(&p, pkt, 0, 1100, 2000, 100);
    for (int i = 0; i < LD_PACKET_LEN; i++) ok = ld_feed(&p, pkt[i], 20);
    CHECK(ok && p.revolutions == 1);
    CHECK(p.sector_cm[35] == 50);
    CHECK(p.sector_cm[18] == LD_NO_DATA);
    CHECK(p.speed_dps == 3600);

    // CRC malo
    ld_make_packet(&p, pkt, 3000, 4100, 700, 100);
    pkt[10] ^= 0xFF;
    uint32_t before = p.packets;
    for (int i = 0; i < LD_PACKET_LEN; i++) ld_feed(&p, pkt[i], 30);
    CHECK(p.packets == before && p.crc_err == 1);
}

static void test_heading(void)
{
    printf("rumbo y brujula\n");
    NEAR(wrap360(-10), 350, 1e-4);
    NEAR(wrap180(190), -170, 1e-4);
    heading_t h;
    heading_init(&h, 0.05f);
    heading_mag(&h, 350);
    NEAR(h.yaw_deg, 350, 1e-4);
    heading_gyro(&h, 20, 1.0f);
    NEAR(h.yaw_deg, 10, 1e-3);
    // la brujula tira del rumbo por el camino corto (cruce de 0/360)
    heading_mag(&h, 350);
    CHECK(h.yaw_deg < 10 && h.yaw_deg > 0);
    for (int i = 0; i < 400; i++) heading_mag(&h, 350);
    NEAR(wrap180(h.yaw_deg - 350), 0, 0.5);

    // brujula plana (X adelante, Y derecha): norte adelante -> 0, este adelante -> 90
    NEAR(compass_heading(30, 0, 0, 0, 0, 0, 0, 0, 0, false), 0, 1e-3);
    NEAR(compass_heading(0, -30, 0, 0, 0, 0, 0, 0, 0, false), 90, 1e-3);
    NEAR(compass_heading(0, -30, 0, 0, 0, 0, 0, 5, 10, false), 105, 1e-3);
    NEAR(compass_heading(40, 3, 0, 0, 0, 10, 3, 0, 0, false), 0, 1e-3);   // offsets hard-iron
}

static nav_cfg_t default_nav_cfg(void)
{
    nav_cfg_t c = {40.0f, 3.0f, 1.0f, 35.0f, 100.0f, 0.0f, false};
    return c;
}

static void test_nav(void)
{
    printf("navegacion\n");
    nav_cfg_t cfg = default_nav_cfg();
    nav_t n;
    nav_out_t o;
    uint16_t clear[LIDAR_SECTORS];
    for (int i = 0; i < LIDAR_SECTORS; i++) clear[i] = LIDAR_NO_DATA;
    nav_in_t in = {true, -33.0, -70.0, true, 0.0f, true, clear};

    float d, b;
    nav_geo(0, 0, 0.0009, 0, &d, &b);               // ~100 m al norte
    NEAR(d, 100.07, 0.2);
    NEAR(b, 0, 1e-3);
    nav_geo(0, 0, 0, 0.0009, &d, &b);
    NEAR(b, 90, 1e-3);

    // punto 50 m al norte, mirando al norte: avanza recto
    nav_init(&n);
    nav_set_target(&n, in.lat + 0.00045, in.lon);
    nav_step(&n, &cfg, &in, 0.05f, &o);
    CHECK(o.state == NAVS_RUNNING);
    CHECK(o.throttle > 30);
    NEAR(o.steer, 0, 3);
    NEAR(o.dist_m, 50, 1);

    // punto al este, mirando al norte: gira a la derecha y reduce velocidad
    nav_set_target(&n, in.lat, in.lon + 0.0005);
    nav_step(&n, &cfg, &in, 0.05f, &o);
    CHECK(o.steer > 90);
    CHECK(o.throttle < 25 && o.throttle > 0);
    // al oeste: gira a la izquierda
    nav_set_target(&n, in.lat, in.lon - 0.0005);
    nav_step(&n, &cfg, &in, 0.05f, &o);
    CHECK(o.steer < -90);

    // llegada
    nav_set_target(&n, in.lat + 0.00001, in.lon);
    nav_step(&n, &cfg, &in, 0.05f, &o);
    CHECK(o.state == NAVS_ARRIVED && n.state == NAVS_ARRIVED && o.throttle == 0);

    // obstaculo a la derecha, a 70 cm: esquiva hacia la izquierda y frena
    nav_set_target(&n, in.lat + 0.00045, in.lon);
    uint16_t obs[LIDAR_SECTORS];
    for (int i = 0; i < LIDAR_SECTORS; i++) obs[i] = LIDAR_NO_DATA;
    obs[3] = 70; obs[4] = 70;                        // 30..50 grados a la derecha
    in.lidar_cm = obs;
    nav_step(&n, &cfg, &in, 0.05f, &o);
    CHECK(o.steer < -5);

    // obstaculo de frente a 20 cm: retrocede y apunta hacia el lado mas libre (izquierda libre)
    for (int i = 0; i < LIDAR_SECTORS; i++) obs[i] = LIDAR_NO_DATA;
    obs[0] = 20; obs[35] = 20;
    for (int i = 6; i <= 9; i++) obs[i] = 40;        // derecha cerrada
    nav_set_target(&n, in.lat + 0.00045, in.lon);
    nav_step(&n, &cfg, &in, 0.05f, &o);
    CHECK(o.throttle < 0 && o.state == NAVS_AVOID);
    CHECK(o.steer > 0);                              // en reversa, volante a la derecha = nariz a la izquierda
    // la maniobra dura ~1.1 s y luego retoma
    for (int i = 0; i < 30; i++) nav_step(&n, &cfg, &in, 0.05f, &o);
    CHECK(n.reverse_ms_left <= 0);

    // sin GPS: se detiene y a los 5 s aborta
    nav_set_target(&n, in.lat + 0.00045, in.lon);
    in.lidar_cm = clear;
    in.gps_valid = false;
    nav_step(&n, &cfg, &in, 0.05f, &o);
    CHECK(o.throttle == 0 && o.state == NAVS_RUNNING);
    for (int i = 0; i < 110; i++) nav_step(&n, &cfg, &in, 0.05f, &o);
    CHECK(n.state == NAVS_NO_GPS);

    // bloqueado tras demasiados intentos
    in.gps_valid = true;
    for (int i = 0; i < LIDAR_SECTORS; i++) obs[i] = 20;
    in.lidar_cm = obs;
    nav_set_target(&n, in.lat + 0.00045, in.lon);
    for (int i = 0; i < 400 && n.state == NAVS_RUNNING; i++) nav_step(&n, &cfg, &in, 0.05f, &o);
    CHECK(n.state == NAVS_BLOCKED);

    nav_cancel(&n);
    nav_step(&n, &cfg, &in, 0.05f, &o);
    CHECK(o.state == NAVS_IDLE && o.throttle == 0);
}

static void test_spoiler(void)
{
    printf("tira trasera\n");
    rgb8_t led[8];
    spoiler_fx_render(led, 8, 100, false, 1.0f);
    int lit = 0;
    for (int i = 0; i < 8; i++) lit += (led[i].r | led[i].g | led[i].b) != 0;
    CHECK(lit == 8);
    CHECK(led[0].g == 255 && led[3].b == 255);            // verde afuera, azul al centro

    spoiler_fx_render(led, 8, 50, false, 1.0f);
    CHECK((led[0].g | led[0].r) != 0 && (led[7].g | led[7].r) != 0);
    CHECK(led[3].r == 0 && led[3].g == 0 && led[3].b == 0);   // centro apagado a medio acelerador

    spoiler_fx_render(led, 8, 0, true, 1.0f);
    CHECK(led[0].r == 255 && led[4].r == 255 && led[4].g == 0);

    spoiler_fx_render(led, 8, 0, false, 1.0f);
    CHECK(led[0].r > 0 && led[0].r < 30);                 // posicion tenue

    spoiler_fx_render(led, 8, -80, false, 1.0f);
    CHECK(led[0].r == led[0].g && led[0].g == led[0].b && led[0].r > 0);   // reversa en blanco

    spoiler_fx_sweep(led, 8, 0.5f, 1.0f);
    CHECK(led[0].r > 0 && led[7].r == 0);
}

int main(void)
{
    test_protocol();
    test_spi();
    test_pid_and_sync();
    test_mix();
    test_battery();
    test_nmea();
    test_lidar();
    test_heading();
    test_nav();
    test_spoiler();
    printf("\n%d verificaciones OK, %d fallos\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
