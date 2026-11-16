#pragma once
#include <stdint.h>
#include <stdbool.h>

// Navegacion autonoma hacia un punto GPS con esquive reactivo usando el LIDAR.
// Estados (NAVS_* en spi_link.h): IDLE, RUNNING, AVOID, ARRIVED, NO_GPS, BLOCKED.
// Marco del LIDAR: 36 sectores de 10 grados, sentido horario, 0 = frente del sensor.

typedef struct {
    float speed_pct;       // velocidad de crucero (0..100)
    float arrive_m;        // radio de llegada
    float steer_kp;        // 1.0 = giro maximo con 40 grados de error de rumbo
    float stop_cm;         // frena y retrocede si hay algo mas cerca al frente
    float avoid_cm;        // empieza a esquivar y a reducir velocidad
    float lidar_yaw_deg;   // giro de montaje del LIDAR respecto al frente del carrito
    bool  lidar_invert;    // true si el LIDAR gira en sentido antihorario
} nav_cfg_t;

typedef struct {
    bool     gps_valid;
    double   lat, lon;
    bool     heading_valid;
    float    heading_deg;
    bool     lidar_valid;
    const uint16_t *lidar_cm;   // LIDAR_SECTORS valores; 0xFFFF = sin eco
} nav_in_t;

typedef struct {
    float   throttle;      // -100..100
    float   steer;         // -100..100 (+ = derecha)
    uint8_t state;         // NAVS_*
    float   dist_m;
    float   bearing_deg;
} nav_out_t;

typedef struct {
    uint8_t state;         // IDLE / RUNNING / ARRIVED / NO_GPS / BLOCKED
    bool    has_wp;
    double  wp_lat, wp_lon;
    float   reverse_ms_left;
    float   reverse_steer;
    int     reverse_count;
    float   cooldown_s;
    float   no_gps_s;
    float   clear_s;
} nav_t;

void nav_init(nav_t *n);
void nav_set_target(nav_t *n, double lat, double lon);
void nav_cancel(nav_t *n);
void nav_step(nav_t *n, const nav_cfg_t *c, const nav_in_t *in, float dt_s, nav_out_t *out);

// Distancia (m) y rumbo inicial (grados 0..360) entre dos coordenadas.
void nav_geo(double lat1, double lon1, double lat2, double lon2, float *dist_m, float *bearing_deg);
