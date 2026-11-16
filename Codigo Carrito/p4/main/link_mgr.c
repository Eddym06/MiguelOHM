#include "link_mgr.h"
#include <math.h>
#include <string.h>
#include "car_state.h"
#include "settings.h"
#include "drive.h"
#include "sensors.h"
#include "nav.h"
#include "rc433_protocol.h"
#include "drivers/rc433.h"
#include "drivers/lights.h"
#include "drivers/spi_master_link.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "link";

#define LOOP_MS          20
#define RF_ACTIVE_MS     3000   // sin comandos 433 por este tiempo -> se libera el WiFi
#define STA_GONE_TICKS   50     // 1 s sin celular asociado -> se libera el 433

static uint8_t s_mode = LINK_NONE;
static nav_t s_nav;

// comando recibido por 433 (lo escribe la tarea del radio, lo lee esta tarea)
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static rc433_cmd_t s_rf_cmd;
static bool s_rf_new;
static uint32_t s_last_rf_ms;

// secuencias de ordenes de la web ya atendidas
static bool s_seq_init;
static uint8_t s_nav_seq, s_test_seq, s_cfg_seq;
static uint32_t s_test_until;

static void send_telemetry(void)
{
    rc433_telem_t t;
    float s = g_car.speed_kmh * 10.0f;
    t.speed_x10 = s > 255.0f ? 255 : (uint8_t)s;
    float r = 0.5f * (fabsf(g_car.rpm_l) + fabsf(g_car.rpm_r)) / 4.0f;
    t.rpm_div4 = r > 255.0f ? 255 : (uint8_t)r;
    t.batt_pct = g_car.ina_ok ? g_car.batt_pct : 0xFF;
    rc433_send_telem(&t);
}

static void on_rf_cmd(const rc433_cmd_t *c)
{
    portENTER_CRITICAL(&s_mux);
    s_rf_cmd = *c;
    s_rf_new = true;
    s_last_rf_ms = now_ms();
    portEXIT_CRITICAL(&s_mux);
    send_telemetry();   // el control espera la respuesta justo despues de su comando
}

static void set_mode(uint8_t m)
{
    if (m == s_mode) return;
    if (s_mode == LINK_WEB || m == LINK_RF433) {
        nav_cancel(&s_nav);
    }
    drive_stop();
    s_mode = m;
    g_car.link_mode = m;
    g_car.wifi_allowed = (m != LINK_RF433);
    rc433_set_enabled(m != LINK_WEB);
    ESP_LOGI(TAG, "enlace: %s", m == LINK_RF433 ? "433 MHz (WiFi apagado)"
                                : m == LINK_WEB ? "WiFi/web (433 apagado)" : "ninguno (ambos disponibles)");
}

// ---------------------------------------------------------------- ordenes de la web
static void handle_cfg(const s3_to_p4_t *c)
{
    float v = 0;
    if (c->cfg_op == CFG_OP_SET && c->cfg_key < SET_COUNT) {
        v = settings_set((setting_id_t)c->cfg_key, c->cfg_val);
    } else if (c->cfg_op == CFG_OP_GET && c->cfg_key < SET_COUNT) {
        v = settings_get((setting_id_t)c->cfg_key);
    } else if (c->cfg_op == CFG_OP_RESET) {
        settings_reset_all();
    }
    g_car.cfg_key = c->cfg_key;
    g_car.cfg_val = v;
    g_car.cfg_ack = c->cfg_seq;    // se publica al final: avisa que el valor ya es valido
}

static void handle_test(const s3_to_p4_t *c, uint32_t now)
{
    uint32_t ms = c->test_ms > 3000 ? 3000 : c->test_ms;
    float v = (float)c->test_value;
    switch (c->test_id) {
    case TEST_MOTOR_L: drive_test_motor(0, v, ms); break;
    case TEST_MOTOR_R: drive_test_motor(1, v, ms); break;
    case TEST_PID_RPM: drive_test_pid(v, ms); break;
    case TEST_SERVO: drive_test_servo(v, ms); break;
    case TEST_HEADLIGHTS:
    case TEST_RGB:
    case TEST_SPOILER: lights_test(c->test_id, c->test_value, ms, now); break;
    case TEST_COMPASS_CAL: sensors_compass_cal(c->test_value != 0); ms = 0; break;
    case TEST_GYRO_RECAL: sensors_gyro_recal(); ms = 500; break;
    default: ms = 0; break;
    }
    s_test_until = now + ms;
    g_car.test_id = ms ? c->test_id : 0;
}

static void run_nav(const s3_to_p4_t *c, uint32_t now)
{
    nav_in_t in;
    uint16_t lidar[LIDAR_SECTORS];
    car_lock();
    gps_fix_t gps = g_car.gps;
    uint32_t gga_ms = g_car.gps_gga_ms;
    uint32_t lidar_ms = g_car.lidar_ms;
    memcpy(lidar, g_car.lidar_cm, sizeof(lidar));
    car_unlock();

    in.gps_valid = gps.valid && (now - gga_ms) < 2000;
    in.lat = gps.lat;
    in.lon = gps.lon;
    in.heading_valid = g_car.heading_valid && (g_car.compass_ok || g_car.imu_ok);
    in.heading_deg = g_car.heading_deg;
    in.lidar_valid = lidar_ms != 0 && (now - lidar_ms) < 600;
    in.lidar_cm = lidar;

    nav_cfg_t cfg = {
        .speed_pct = CFG(NAV_SPEED_PCT),
        .arrive_m = CFG(NAV_ARRIVE_M),
        .steer_kp = CFG(NAV_STEER_KP),
        .stop_cm = CFG(LIDAR_STOP_CM),
        .avoid_cm = CFG(LIDAR_AVOID_CM),
        .lidar_yaw_deg = CFG(LIDAR_YAW_DEG),
        .lidar_invert = CFG(LIDAR_INVERT) > 0.5f,
    };
    nav_out_t out;
    nav_step(&s_nav, &cfg, &in, LOOP_MS / 1000.0f, &out);
    drive_set_command(out.throttle, out.steer, false);
    g_car.nav_state = out.state;
    g_car.wp_dist_m = out.dist_m;
    g_car.wp_bearing_deg = out.bearing_deg;
    (void)c;
}

static void web_tick(uint32_t now)
{
    s3_to_p4_t c;
    uint32_t age = 0;
    bool have = spi_link_master_get(&c, &age) && age < 500;

    if (!have) {                       // se perdio el S3: parar y cancelar el autonomo
        if (s_nav.state != NAVS_IDLE) nav_cancel(&s_nav);
        drive_stop();
        g_car.nav_state = NAVS_IDLE;
        g_car.wp_valid = false;
        return;
    }

    if (!s_seq_init) {                 // no ejecutar ordenes viejas que quedaron en la trama
        s_nav_seq = c.nav_seq;
        s_test_seq = c.test_seq;
        s_cfg_seq = c.cfg_seq;
        s_seq_init = true;
    }
    if (c.cfg_seq != s_cfg_seq) {
        s_cfg_seq = c.cfg_seq;
        handle_cfg(&c);
    }
    if (c.test_seq != s_test_seq) {
        s_test_seq = c.test_seq;
        handle_test(&c, now);
    }
    if (g_car.test_id && g_car.test_id != TEST_COMPASS_CAL && (int32_t)(s_test_until - now) <= 0) g_car.test_id = 0;
    if (g_car.test_id == TEST_COMPASS_CAL && !sensors_compass_cal_active()) g_car.test_id = 0;

    if (c.nav_seq != s_nav_seq) {
        s_nav_seq = c.nav_seq;
        if (c.nav_cmd == NAVC_GOTO && c.auto_mode) {
            nav_set_target(&s_nav, c.wp_lat_e7 / 1e7, c.wp_lon_e7 / 1e7);
            g_car.wp_lat = s_nav.wp_lat;
            g_car.wp_lon = s_nav.wp_lon;
            g_car.wp_valid = true;
            ESP_LOGI(TAG, "autonomo: destino %.6f, %.6f", s_nav.wp_lat, s_nav.wp_lon);
        } else if (c.nav_cmd == NAVC_CANCEL) {
            nav_cancel(&s_nav);
            g_car.wp_valid = false;
            g_car.nav_state = NAVS_IDLE;
            drive_stop();
        }
    }

    lights_set((c.drive_flags & DRV_HEADLIGHTS) != 0, (c.drive_flags & DRV_RGB_ON) != 0,
               (uint8_t)(c.drive_flags >> DRV_COLOR_SHIFT));
    g_car.headlights = (c.drive_flags & DRV_HEADLIGHTS) != 0;
    g_car.rgb_on = (c.drive_flags & DRV_RGB_ON) != 0;
    g_car.rgb_idx = (uint8_t)(c.drive_flags >> DRV_COLOR_SHIFT);

    bool manual = c.throttle != 0 || (c.drive_flags & DRV_BRAKE);
    bool nav_running = s_nav.has_wp && s_nav.state == NAVS_RUNNING;
    if (nav_running && c.auto_mode && !manual) {
        run_nav(&c, now);
    } else {
        if (nav_running && (manual || !c.auto_mode)) {   // el usuario retoma el control
            nav_cancel(&s_nav);
            g_car.nav_state = NAVS_IDLE;
            g_car.wp_valid = false;
        } else if (s_nav.has_wp) {
            g_car.nav_state = s_nav.state;                // ARRIVED / NO_GPS / BLOCKED visibles
        }
        drive_set_command((float)c.throttle, (float)c.steer, (c.drive_flags & DRV_BRAKE) != 0);
    }
}

static void rf_tick(uint32_t now)
{
    (void)now;
    rc433_cmd_t c;
    bool fresh;
    portENTER_CRITICAL(&s_mux);
    c = s_rf_cmd;
    fresh = s_rf_new;
    s_rf_new = false;
    portEXIT_CRITICAL(&s_mux);
    if (!fresh) return;

    lights_set((c.flags & RC433_F_HEADLIGHTS) != 0, (c.flags & RC433_F_RGB_ON) != 0,
               (uint8_t)(c.flags >> RC433_COLOR_SHIFT));
    g_car.headlights = (c.flags & RC433_F_HEADLIGHTS) != 0;
    g_car.rgb_on = (c.flags & RC433_F_RGB_ON) != 0;
    g_car.rgb_idx = (uint8_t)(c.flags >> RC433_COLOR_SHIFT);
    drive_set_command((float)c.throttle, (float)c.steer, (c.flags & RC433_F_BRAKE) != 0);
}

static void link_task(void *arg)
{
    int sta_gone = 0;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(LOOP_MS));
        uint32_t now = now_ms();

        s3_to_p4_t c;
        uint32_t age = 0;
        bool s3_ok = spi_link_master_get(&c, &age) && age < 500;
        bool sta = s3_ok && (c.flags & S3F_STA_CONNECTED);

        uint32_t last_rf;
        portENTER_CRITICAL(&s_mux);
        last_rf = s_last_rf_ms;
        portEXIT_CRITICAL(&s_mux);
        bool rf_active = last_rf != 0 && (now - last_rf) < RF_ACTIVE_MS;

        // ---- arbitraje: el primero en conectarse gana ----
        switch (s_mode) {
        case LINK_NONE:
            if (sta) set_mode(LINK_WEB);
            else if (rf_active) set_mode(LINK_RF433);
            break;
        case LINK_RF433:
            if (!rf_active) set_mode(LINK_NONE);
            break;
        case LINK_WEB:
            if (sta) sta_gone = 0;
            else if (++sta_gone >= STA_GONE_TICKS) {
                sta_gone = 0;
                set_mode(LINK_NONE);
            }
            break;
        default:
            break;
        }

        if (s_mode == LINK_RF433) rf_tick(now);
        else if (s_mode == LINK_WEB) web_tick(now);
    }
}

void link_mgr_start(void)
{
    nav_init(&s_nav);
    g_car.link_mode = LINK_NONE;
    g_car.wifi_allowed = true;
    rc433_start(on_rf_cmd);
    spi_link_master_start();
    xTaskCreate(link_task, "link_mgr", 6144, NULL, 8, NULL);
}
