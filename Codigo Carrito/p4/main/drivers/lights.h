#pragma once
#include <stdbool.h>
#include <stdint.h>

// Faros (NPN), LEDs RGB de las ruedas (PWM) y tira WS2812 del aleron.
void lights_init(void);

// Estado pedido por el control remoto o la web
void lights_set(bool headlights, bool rgb_on, uint8_t rgb_idx);

// Llamar cada ~20 ms: actualiza RGB (arcoiris animado), tira trasera y faros.
// throttle_pct: -100..100 aplicado. brake: freno activo.
void lights_tick(float throttle_pct, bool brake, uint32_t now_ms);

// Pruebas desde la web: durante `ms` se ignora el estado normal. kind: ver TEST_* en spi_link.h
void lights_test(int kind, int value, uint32_t ms, uint32_t now_ms);
