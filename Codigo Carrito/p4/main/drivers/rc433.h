#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "rc433_protocol.h"

// Radio 433 MHz (transmisor + receptor ASK por UART, linea invertida, 4800 baud).
typedef void (*rc433_cmd_cb_t)(const rc433_cmd_t *cmd);

void rc433_start(rc433_cmd_cb_t on_cmd);
// Enciende/apaga la radio (EN del transmisor y descarte de lo recibido).
void rc433_set_enabled(bool enabled);
bool rc433_enabled(void);
// Responde al control con la telemetria. Ignorado si la radio esta apagada.
void rc433_send_telem(const rc433_telem_t *t);
uint32_t rc433_rx_ok(void);
uint32_t rc433_rx_bad(void);
