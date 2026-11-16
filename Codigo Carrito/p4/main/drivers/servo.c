#include "servo.h"
#include "board_pins.h"
#include "settings.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "servo";

#define SERVO_HZ    50
#define SERVO_BITS  LEDC_TIMER_14_BIT
#define SERVO_MAX   16383

static bool s_ok;

void servo_init(void)
{
    if (PIN_SERVO < 0) {
        ESP_LOGW(TAG, "pin del servo sin definir: servo desactivado");
        return;
    }
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = SERVO_BITS,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = SERVO_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    ledc_channel_config_t ch = {
        .gpio_num = PIN_SERVO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)LEDC_CH_SERVO,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
    s_ok = true;
    servo_set_angle(0);
}

bool servo_present(void)
{
    return s_ok;
}

void servo_set_angle(float deg)
{
    if (!s_ok) return;
    if (CFG(SERVO_INVERT) > 0.5f) deg = -deg;
    float us = CFG(SERVO_CENTER_US) + deg * CFG(SERVO_US_PER_DEG);
    float lo = CFG(SERVO_MIN_US), hi = CFG(SERVO_MAX_US);
    if (us < lo) us = lo;
    if (us > hi) us = hi;
    uint32_t duty = (uint32_t)(us / 20000.0f * SERVO_MAX + 0.5f);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)LEDC_CH_SERVO, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)LEDC_CH_SERVO);
}
