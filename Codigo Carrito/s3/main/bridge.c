#include "bridge.h"
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#define CTL_TIMEOUT_MS   500
#define CFG_QUEUE        64
#define CFG_TIMEOUT_MS   500

typedef struct { int op, key; float val; } cfg_op_t;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static p4_to_s3_t s_status;
static uint32_t s_status_ms;
static bool s_status_valid;

static s3_to_p4_t s_cmd;
static uint32_t s_ctl_ms;
static int s_ctl_th, s_ctl_st;
static bool s_ctl_brake;
static uint8_t s_light_flags = DRV_RGB_ON;
static uint8_t s_seq;
static bool s_sta, s_auto;
static int s_ws;

static cfg_op_t s_q[CFG_QUEUE];
static int s_q_head, s_q_tail;
static bool s_cfg_active;
static uint32_t s_cfg_since;

typedef struct { int key; float val; } cfg_reply_t;
static cfg_reply_t s_replies[CFG_QUEUE];
static int s_r_head, s_r_tail;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

void bridge_init(void)
{
    memset(&s_cmd, 0, sizeof(s_cmd));
    s_cmd.magic = SPI_LINK_MAGIC;
}

void bridge_set_status(const p4_to_s3_t *st)
{
    portENTER_CRITICAL(&s_mux);
    s_status = *st;
    s_status_ms = now_ms();
    s_status_valid = true;
    // respuesta a la operacion de configuracion en curso
    if (s_cfg_active && st->cfg_ack == s_cmd.cfg_seq) {
        int next = (s_r_head + 1) % CFG_QUEUE;
        if (next != s_r_tail) {
            s_replies[s_r_head].key = st->cfg_key;
            s_replies[s_r_head].val = st->cfg_val;
            s_r_head = next;
        }
        s_cfg_active = false;
    }
    portEXIT_CRITICAL(&s_mux);
}

bool bridge_get_status(p4_to_s3_t *out, uint32_t *age_ms)
{
    bool ok;
    uint32_t t;
    portENTER_CRITICAL(&s_mux);
    ok = s_status_valid;
    if (ok) *out = s_status;
    t = s_status_ms;
    portEXIT_CRITICAL(&s_mux);
    if (ok && age_ms) *age_ms = now_ms() - t;
    return ok;
}

void bridge_fill_cmd(s3_to_p4_t *out)
{
    uint32_t now = now_ms();
    portENTER_CRITICAL(&s_mux);

    // sin ordenes frescas de la pagina: acelerador, giro y freno a cero
    bool fresh = (now - s_ctl_ms) < CTL_TIMEOUT_MS && s_ctl_ms != 0;
    s_cmd.throttle = fresh ? (int8_t)s_ctl_th : 0;
    s_cmd.steer = fresh ? (int8_t)s_ctl_st : 0;
    s_cmd.drive_flags = s_light_flags | ((fresh && s_ctl_brake) ? DRV_BRAKE : 0);
    s_cmd.auto_mode = s_auto ? 1 : 0;
    s_cmd.flags = (s_sta ? S3F_STA_CONNECTED : 0) | (s_ws > 0 ? S3F_WS_CLIENT : 0);

    // siguiente operacion de configuracion
    if (s_cfg_active && (now - s_cfg_since) > CFG_TIMEOUT_MS) s_cfg_active = false;
    if (!s_cfg_active && s_q_head != s_q_tail) {
        cfg_op_t op = s_q[s_q_tail];
        s_q_tail = (s_q_tail + 1) % CFG_QUEUE;
        s_cmd.cfg_op = (uint8_t)op.op;
        s_cmd.cfg_key = (uint8_t)op.key;
        s_cmd.cfg_val = op.val;
        s_cmd.cfg_seq++;
        s_cfg_active = true;
        s_cfg_since = now;
    }

    s_cmd.magic = SPI_LINK_MAGIC;
    s_cmd.seq = s_seq++;
    *out = s_cmd;
    portEXIT_CRITICAL(&s_mux);
}

void bridge_set_ctl(int throttle, int steer, bool brake)
{
    if (throttle > 100) throttle = 100;
    if (throttle < -100) throttle = -100;
    if (steer > 100) steer = 100;
    if (steer < -100) steer = -100;
    portENTER_CRITICAL(&s_mux);
    s_ctl_th = throttle;
    s_ctl_st = steer;
    s_ctl_brake = brake;
    s_ctl_ms = now_ms();
    portEXIT_CRITICAL(&s_mux);
}

void bridge_set_lights(bool headlights, bool rgb_on, int color)
{
    if (color < 0 || color > 8) color = 0;
    portENTER_CRITICAL(&s_mux);
    s_light_flags = (headlights ? DRV_HEADLIGHTS : 0) | (rgb_on ? DRV_RGB_ON : 0) |
                    (uint8_t)(color << DRV_COLOR_SHIFT);
    portEXIT_CRITICAL(&s_mux);
}

void bridge_set_auto(bool on)
{
    portENTER_CRITICAL(&s_mux);
    s_auto = on;
    portEXIT_CRITICAL(&s_mux);
}

void bridge_nav_goto(int32_t lat_e7, int32_t lon_e7)
{
    portENTER_CRITICAL(&s_mux);
    s_auto = true;
    s_cmd.wp_lat_e7 = lat_e7;
    s_cmd.wp_lon_e7 = lon_e7;
    s_cmd.nav_cmd = NAVC_GOTO;
    s_cmd.nav_seq++;
    portEXIT_CRITICAL(&s_mux);
}

void bridge_nav_cancel(void)
{
    portENTER_CRITICAL(&s_mux);
    s_auto = false;
    s_cmd.nav_cmd = NAVC_CANCEL;
    s_cmd.nav_seq++;
    portEXIT_CRITICAL(&s_mux);
}

void bridge_test(int id, int value, int ms)
{
    if (ms < 0) ms = 0;
    if (ms > 3000) ms = 3000;
    portENTER_CRITICAL(&s_mux);
    s_cmd.test_id = (uint8_t)id;
    s_cmd.test_value = (int16_t)value;
    s_cmd.test_ms = (uint16_t)ms;
    s_cmd.test_seq++;
    portEXIT_CRITICAL(&s_mux);
}

bool bridge_cfg_push(int op, int key, float val)
{
    bool ok = false;
    portENTER_CRITICAL(&s_mux);
    int next = (s_q_head + 1) % CFG_QUEUE;
    if (next != s_q_tail) {
        s_q[s_q_head] = (cfg_op_t){op, key, val};
        s_q_head = next;
        ok = true;
    }
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

bool bridge_cfg_pop_reply(int *key, float *val)
{
    bool ok = false;
    portENTER_CRITICAL(&s_mux);
    if (s_r_head != s_r_tail) {
        *key = s_replies[s_r_tail].key;
        *val = s_replies[s_r_tail].val;
        s_r_tail = (s_r_tail + 1) % CFG_QUEUE;
        ok = true;
    }
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

void bridge_set_sta(bool connected)
{
    portENTER_CRITICAL(&s_mux);
    s_sta = connected;
    portEXIT_CRITICAL(&s_mux);
}

void bridge_set_ws_clients(int n)
{
    portENTER_CRITICAL(&s_mux);
    s_ws = n;
    portEXIT_CRITICAL(&s_mux);
}

bool bridge_wifi_allowed(void)
{
    bool allowed = true;
    portENTER_CRITICAL(&s_mux);
    // sin P4 (nunca hubo trama o hace >3 s) el WiFi queda disponible para diagnostico
    if (s_status_valid && (now_ms() - s_status_ms) < 3000) allowed = (s_status.flags & P4F_WIFI_ALLOWED) != 0;
    portEXIT_CRITICAL(&s_mux);
    return allowed;
}
