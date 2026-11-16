#include "car_state.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

car_state_t g_car;
static SemaphoreHandle_t s_lock;

void car_state_init(void)
{
    memset(&g_car, 0, sizeof(g_car));
    for (int i = 0; i < LIDAR_SECTORS; i++) g_car.lidar_cm[i] = LIDAR_NO_DATA;
    g_car.rgb_on = true;
    g_car.wifi_allowed = true;
    s_lock = xSemaphoreCreateMutex();
}

void car_lock(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
}

void car_unlock(void)
{
    xSemaphoreGive(s_lock);
}
