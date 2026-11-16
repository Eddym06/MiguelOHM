#pragma once
#include <stdbool.h>

void wifi_ap_init(void);                 // crea y arranca el punto de acceso
void wifi_ap_set_enabled(bool enabled);  // apaga/enciende la radio WiFi
bool wifi_ap_enabled(void);
int  wifi_ap_sta_count(void);            // celulares asociados
