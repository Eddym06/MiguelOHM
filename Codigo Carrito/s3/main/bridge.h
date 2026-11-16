#pragma once
// Estado compartido entre la tarea SPI (P4) y el servidor web.
#include <stdbool.h>
#include <stdint.h>
#include "spi_link.h"

void bridge_init(void);

// --- lado SPI ---
void bridge_set_status(const p4_to_s3_t *st);   // llego una trama valida del P4
void bridge_fill_cmd(s3_to_p4_t *out);           // trama a enviar al P4 (aplica los tiempos de seguridad)

// --- lado web ---
bool bridge_get_status(p4_to_s3_t *out, uint32_t *age_ms);
void bridge_set_ctl(int throttle, int steer, bool brake);
void bridge_set_lights(bool headlights, bool rgb_on, int color);
void bridge_set_auto(bool on);
void bridge_nav_goto(int32_t lat_e7, int32_t lon_e7);
void bridge_nav_cancel(void);
void bridge_test(int id, int value, int ms);
// Cola de operaciones de configuracion (el P4 las atiende de una en una)
bool bridge_cfg_push(int op, int key, float val);
bool bridge_cfg_pop_reply(int *key, float *val);

// --- supervisor ---
void bridge_set_sta(bool connected);
void bridge_set_ws_clients(int n);
bool bridge_wifi_allowed(void);   // el P4 permite tener el WiFi encendido
