#pragma once
#include <stdbool.h>
#include "settings_table.h"

// Ajustes persistentes (NVS). Lectura barata y segura desde cualquier tarea.
void  settings_init(void);
float settings_get(setting_id_t id);
// Limita al rango de la tabla y guarda en NVS. Devuelve el valor realmente aplicado.
float settings_set(setting_id_t id, float value);
void  settings_reset_all(void);

#define CFG(NAME) settings_get(SET_##NAME)
