#pragma once
#include <stdint.h>
#include <stdbool.h>

// Efecto de la tira trasera del aleron, estilo luces de pilotos de F1:
//   - acelerando: se llena de afuera hacia el centro (verde -> rojo -> azul) segun el acelerador
//   - reversa: se llena en blanco
//   - frenando: toda la tira en rojo intenso
//   - en reposo: luz de posicion roja tenue

typedef struct { uint8_t r, g, b; } rgb8_t;

// throttle_pct: -100..100. brightness: 0..1.
void spoiler_fx_render(rgb8_t *leds, int n, float throttle_pct, bool brake, float brightness);
// Barrido de prueba (progress 0..1 recorre la tira).
void spoiler_fx_sweep(rgb8_t *leds, int n, float progress, float brightness);
