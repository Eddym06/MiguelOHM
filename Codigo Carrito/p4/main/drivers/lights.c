#include "lights.h"
#include <math.h>
#include <string.h>
#include "board_pins.h"
#include "settings.h"
#include "spoiler_fx.h"
#include "rc433_protocol.h"
#include "spi_link.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_log.h"

static const char *TAG = "lights";

#define SPOILER_MAX_LEDS 32

static bool s_headlights;
static bool s_rgb_on = true;
static uint8_t s_rgb_idx;

static bool s_rgb_ok, s_spoiler_ok, s_head_ok;
static rmt_channel_handle_t s_rmt;
static rmt_encoder_handle_t s_enc;
static uint8_t s_px[SPOILER_MAX_LEDS * 3];     // orden GRB, tal como lo pide el WS2812

// prueba en curso
static int s_test_kind;
static int s_test_value;
static uint32_t s_test_until, s_test_start;

// ---------------------------------------------------------------- init
static void rgb_pwm_init(void)
{
    // mismo timer de 5 kHz / 10 bits que los motores (se configura aqui por si motors_init no corrio)
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = MOTOR_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t);
    const int pins[3] = {PIN_RGB_R, PIN_RGB_G, PIN_RGB_B};
    const int chs[3] = {LEDC_CH_RGB_R, LEDC_CH_RGB_G, LEDC_CH_RGB_B};
    for (int i = 0; i < 3; i++) {
        ledc_channel_config_t ch = {
            .gpio_num = pins[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = (ledc_channel_t)chs[i],
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = RGB_COMMON_ANODE ? 1023 : 0,
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ch));
    }
    s_rgb_ok = true;
}

static void spoiler_init(void)
{
    rmt_tx_channel_config_t tx = {
        .gpio_num = PIN_SPOILER,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,     // 0.1 us por tick
        .mem_block_symbols = 48,
        .trans_queue_depth = 2,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx, &s_rmt));
    rmt_bytes_encoder_config_t be = {
        .bit0 = {.level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9},   // 0.3 us / 0.9 us
        .bit1 = {.level0 = 1, .duration0 = 9, .level1 = 0, .duration1 = 3},   // 0.9 us / 0.3 us
        .flags.msb_first = 1,
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&be, &s_enc));
    ESP_ERROR_CHECK(rmt_enable(s_rmt));
    s_spoiler_ok = true;
}

void lights_init(void)
{
    if (PIN_HEADLIGHTS >= 0) {
        gpio_config_t io = {.pin_bit_mask = PIN_BIT(PIN_HEADLIGHTS), .mode = GPIO_MODE_OUTPUT};
        ESP_ERROR_CHECK(gpio_config(&io));
        gpio_set_level(PIN_HEADLIGHTS, 0);
        s_head_ok = true;
    } else {
        ESP_LOGW(TAG, "pin de faros sin definir");
    }
    if (PIN_RGB_R >= 0 && PIN_RGB_G >= 0 && PIN_RGB_B >= 0) {
        rgb_pwm_init();
    } else {
        ESP_LOGW(TAG, "pines RGB sin definir: LEDs de ruedas desactivados");
    }
    if (PIN_SPOILER >= 0) {
        spoiler_init();
    } else {
        ESP_LOGW(TAG, "pin de la tira del aleron sin definir");
    }
}

void lights_set(bool headlights, bool rgb_on, uint8_t rgb_idx)
{
    s_headlights = headlights;
    s_rgb_on = rgb_on;
    s_rgb_idx = rgb_idx < RC433_COLOR_COUNT ? rgb_idx : 0;
}

// ---------------------------------------------------------------- salida
static void rgb_write(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_rgb_ok) return;
    float k = CFG(RGB_BRIGHT) / 100.0f;
    const uint8_t v[3] = {r, g, b};
    const int chs[3] = {LEDC_CH_RGB_R, LEDC_CH_RGB_G, LEDC_CH_RGB_B};
    for (int i = 0; i < 3; i++) {
        uint32_t d = (uint32_t)(v[i] / 255.0f * k * 1023.0f + 0.5f);
        if (RGB_COMMON_ANODE) d = 1023 - d;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)chs[i], d);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)chs[i]);
    }
}

static void hsv_to_rgb(float h, uint8_t *r, uint8_t *g, uint8_t *b)
{
    h = fmodf(h, 360.0f);
    float c = 1.0f, x = 1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f);
    float rr = 0, gg = 0, bb = 0;
    if (h < 60) { rr = c; gg = x; }
    else if (h < 120) { rr = x; gg = c; }
    else if (h < 180) { gg = c; bb = x; }
    else if (h < 240) { gg = x; bb = c; }
    else if (h < 300) { rr = x; bb = c; }
    else { rr = c; bb = x; }
    *r = (uint8_t)(rr * 255);
    *g = (uint8_t)(gg * 255);
    *b = (uint8_t)(bb * 255);
}

static void spoiler_write(const rgb8_t *leds, int n)
{
    if (!s_spoiler_ok) return;
    for (int i = 0; i < n; i++) {
        s_px[i * 3 + 0] = leds[i].g;
        s_px[i * 3 + 1] = leds[i].r;
        s_px[i * 3 + 2] = leds[i].b;
    }
    rmt_tx_wait_all_done(s_rmt, 20);
    rmt_transmit_config_t cfg = {.loop_count = 0};
    rmt_transmit(s_rmt, s_enc, s_px, (size_t)n * 3, &cfg);
}

void lights_test(int kind, int value, uint32_t ms, uint32_t now_ms)
{
    s_test_kind = kind;
    s_test_value = value;
    s_test_start = now_ms;
    s_test_until = now_ms + ms;
}

void lights_tick(float throttle_pct, bool brake, uint32_t now_ms)
{
    bool testing = s_test_kind != 0 && (int32_t)(s_test_until - now_ms) > 0;
    if (!testing) s_test_kind = 0;

    // ---- faros ----
    bool head = s_headlights;
    if (testing && s_test_kind == TEST_HEADLIGHTS) head = s_test_value != 0;
    if (s_head_ok) gpio_set_level(PIN_HEADLIGHTS, head ? 1 : 0);

    // ---- RGB de las ruedas ----
    bool on = s_rgb_on;
    uint8_t idx = s_rgb_idx;
    if (testing && s_test_kind == TEST_RGB) {
        on = true;
        idx = (uint8_t)(s_test_value < 0 ? 0 : (s_test_value >= RC433_COLOR_COUNT ? RC433_COLOR_COUNT - 1 : s_test_value));
    }
    if (!on) {
        rgb_write(0, 0, 0);
    } else if (idx == RC433_COLOR_RAINBOW) {
        uint8_t r, g, b;
        hsv_to_rgb((float)(now_ms % 3000) / 3000.0f * 360.0f, &r, &g, &b);
        rgb_write(r, g, b);
    } else {
        rgb_write(RC433_COLORS[idx].r, RC433_COLORS[idx].g, RC433_COLORS[idx].b);
    }

    // ---- tira trasera ----
    int n = (int)CFG(SPOILER_LEDS);
    if (n > SPOILER_MAX_LEDS) n = SPOILER_MAX_LEDS;
    float bright = CFG(SPOILER_BRIGHT) / 100.0f;
    rgb8_t leds[SPOILER_MAX_LEDS];
    if (testing && s_test_kind == TEST_SPOILER) {
        float p = (float)(now_ms - s_test_start) / (float)(s_test_until - s_test_start);
        spoiler_fx_sweep(leds, n, p, bright);
    } else {
        spoiler_fx_render(leds, n, throttle_pct, brake, bright);
    }
    spoiler_write(leds, n);
}
