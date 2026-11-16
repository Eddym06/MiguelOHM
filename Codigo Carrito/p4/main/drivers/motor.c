#include "motor.h"
#include <math.h>
#include "board_pins.h"
#include "settings.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"

static const char *TAG = "motor";

#define PWM_BITS     LEDC_TIMER_10_BIT
#define PWM_MAX      1023

typedef struct {
    int en, in1, in2;
    int enc_a, enc_b;
    int ledc_ch;
    bool present;
    pcnt_unit_handle_t pcnt;
    int32_t last_count;
    float rpm;
} motor_t;

static motor_t s_m[2] = {
    {PIN_MOT_L_EN, PIN_MOT_L_IN1, PIN_MOT_L_IN2, PIN_ENC_L_A, PIN_ENC_L_B, LEDC_CH_MOT_L, false, NULL, 0, 0},
    {PIN_MOT_R_EN, PIN_MOT_R_IN1, PIN_MOT_R_IN2, PIN_ENC_R_A, PIN_ENC_R_B, LEDC_CH_MOT_R, false, NULL, 0, 0},
};

static bool s_timer_ready;

static void ensure_pwm_timer(void)
{
    if (s_timer_ready) return;
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = PWM_BITS,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = MOTOR_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    s_timer_ready = true;
}

static void setup_encoder(motor_t *m, const char *name)
{
    if (m->enc_a < 0 || m->enc_b < 0) {
        ESP_LOGW(TAG, "encoder %s sin pines: control de velocidad en lazo abierto", name);
        return;
    }
    pcnt_unit_config_t ucfg = {
        .low_limit = -32768,
        .high_limit = 32767,
        .flags.accum_count = true,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&ucfg, &m->pcnt));
    pcnt_glitch_filter_config_t f = {.max_glitch_ns = 1000};
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(m->pcnt, &f));

    pcnt_channel_handle_t ca, cb;
    pcnt_chan_config_t ca_cfg = {.edge_gpio_num = m->enc_a, .level_gpio_num = m->enc_b};
    pcnt_chan_config_t cb_cfg = {.edge_gpio_num = m->enc_b, .level_gpio_num = m->enc_a};
    ESP_ERROR_CHECK(pcnt_new_channel(m->pcnt, &ca_cfg, &ca));
    ESP_ERROR_CHECK(pcnt_new_channel(m->pcnt, &cb_cfg, &cb));
    // decodificacion en cuadratura 4x
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(ca, PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                                 PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(ca, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                  PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(cb, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                                 PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(cb, PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                                  PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    // con accum_count hacen falta watch points en los limites para acumular el desbordamiento
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(m->pcnt, 32767));
    ESP_ERROR_CHECK(pcnt_unit_add_watch_point(m->pcnt, -32768));
    ESP_ERROR_CHECK(pcnt_unit_enable(m->pcnt));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(m->pcnt));
    ESP_ERROR_CHECK(pcnt_unit_start(m->pcnt));
    gpio_pullup_en(m->enc_a);
    gpio_pullup_en(m->enc_b);
}

static void setup_motor(motor_t *m, const char *name)
{
    if (m->en < 0 || m->in1 < 0 || m->in2 < 0) {
        ESP_LOGW(TAG, "motor %s sin pines (EN/IN1/IN2): desactivado", name);
        return;
    }
    ensure_pwm_timer();
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << m->in1) | (1ULL << m->in2),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(m->in1, 0);
    gpio_set_level(m->in2, 0);

    ledc_channel_config_t ch = {
        .gpio_num = m->en,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)m->ledc_ch,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
    m->present = true;
    ESP_LOGI(TAG, "motor %s listo (PWM %d Hz)", name, MOTOR_PWM_HZ);
}

void motors_init(void)
{
    setup_motor(&s_m[MOTOR_L], "izq");
    setup_motor(&s_m[MOTOR_R], "der");
    setup_encoder(&s_m[MOTOR_L], "izq");
    setup_encoder(&s_m[MOTOR_R], "der");
}

bool motor_present(int m)
{
    return s_m[m].present;
}

bool motor_encoder_present(int m)
{
    return s_m[m].pcnt != NULL;
}

static void set_pwm(motor_t *m, float duty01)
{
    if (duty01 < 0) duty01 = 0;
    if (duty01 > 1) duty01 = 1;
    uint32_t d = (uint32_t)(duty01 * PWM_MAX + 0.5f);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)m->ledc_ch, d);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)m->ledc_ch);
}

void motor_set(int idx, motor_mode_t mode, float duty)
{
    motor_t *m = &s_m[idx];
    if (!m->present) return;
    bool inv = CFG(MOTOR_L_INVERT + idx) > 0.5f;   // MOTOR_L_INVERT y MOTOR_R_INVERT son consecutivos

    switch (mode) {
    case MOTOR_BRAKE:               // IN1=IN2=1 con EN al 100 %: freno motor
        gpio_set_level(m->in1, 1);
        gpio_set_level(m->in2, 1);
        set_pwm(m, 1.0f);
        break;
    case MOTOR_DRIVE: {
        bool fwd = duty >= 0;
        if (inv) fwd = !fwd;
        gpio_set_level(m->in1, fwd ? 1 : 0);
        gpio_set_level(m->in2, fwd ? 0 : 1);
        set_pwm(m, fabsf(duty));
        break;
    }
    case MOTOR_COAST:
    default:                        // rueda libre
        set_pwm(m, 0);
        gpio_set_level(m->in1, 0);
        gpio_set_level(m->in2, 0);
        break;
    }
}

void motors_update_encoders(float dt)
{
    float cpr = CFG(ENC_CPR);
    if (dt <= 0 || cpr <= 0) return;
    for (int i = 0; i < 2; i++) {
        motor_t *m = &s_m[i];
        if (!m->pcnt) {
            m->rpm = 0;
            continue;
        }
        int count = 0;
        pcnt_unit_get_count(m->pcnt, &count);
        int32_t delta = (int32_t)count - m->last_count;
        m->last_count = count;
        float rpm = (float)delta / cpr * 60.0f / dt;
        if (CFG(ENC_L_INVERT + i) > 0.5f) rpm = -rpm;
        m->rpm += (rpm - m->rpm) * 0.5f;      // filtro de primer orden
        if (fabsf(m->rpm) < 0.5f) m->rpm = 0;
    }
}

float motor_rpm(int m)
{
    return s_m[m].rpm;
}
