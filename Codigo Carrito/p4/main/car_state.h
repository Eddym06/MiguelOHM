#pragma once
// Estado global del carrito. Cada campo tiene un solo escritor (el modulo que lo produce);
// los bloques multi-campo (GPS, LIDAR) se copian bajo car_lock().

#include <stdint.h>
#include <stdbool.h>
#include "esp_timer.h"
#include "nmea.h"
#include "spi_link.h"

static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

typedef struct {
    // --- sensores ---
    bool     imu_ok, compass_ok, ina_ok, lidar_ok, gps_uart_ok;
    float    pitch_deg, roll_deg, yaw_rate_cw_dps;
    float    heading_deg;            // fusion giroscopio + brujula
    bool     heading_valid;
    float    mag_x, mag_y, mag_z;    // uT
    float    vbat_v, current_a;
    uint8_t  batt_pct;
    gps_fix_t gps;
    uint32_t gps_gga_ms;             // now_ms() cuando llego el ultimo GGA
    uint16_t lidar_cm[LIDAR_SECTORS];
    uint32_t lidar_ms;
    // --- traccion ---
    float    rpm_l, rpm_r;           // con signo
    float    speed_kmh;
    float    servo_deg;
    int8_t   thr_applied, steer_applied;
    bool     brake, headlights, rgb_on;
    uint8_t  rgb_idx;
    uint8_t  enc_fault;              // bit0 izq, bit1 der
    // --- enlace ---
    uint8_t  link_mode;              // LINK_*
    bool     wifi_allowed;
    // --- navegacion ---
    uint8_t  nav_state;
    bool     wp_valid;
    double   wp_lat, wp_lon;
    float    wp_dist_m, wp_bearing_deg;
    // --- config / pruebas ---
    uint8_t  test_id;
    uint8_t  cfg_ack, cfg_key;
    float    cfg_val;
} car_state_t;

extern car_state_t g_car;

void car_state_init(void);
void car_lock(void);
void car_unlock(void);
