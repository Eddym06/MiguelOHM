#include "web_server.h"
#include <math.h>
#include <string.h>
#include "bridge.h"
#include "settings_table.h"
#include "spi_link.h"
#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "web";

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

static httpd_handle_t s_server;

#define PUSH_PERIOD_MS 100

// ---------------------------------------------------------------- HTTP
static esp_err_t index_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, index_html_start, (ssize_t)(index_html_end - index_html_start - 1));
}

static esp_err_t favicon_get(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t settings_meta_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send_chunk(req, "[", 1);
    char buf[256];
    int i = 0;
#define X(id, label, unit, def, mn, mx, step)                                                        \
    {                                                                                                \
        int n = snprintf(buf, sizeof(buf),                                                           \
                         "%s{\"id\":%d,\"n\":\"%s\",\"u\":\"%s\",\"d\":%g,\"min\":%g,\"max\":%g,\"s\":%g}", \
                         i ? "," : "", i, label, unit, (double)(def), (double)(mn), (double)(mx),   \
                         (double)(step));                                                            \
        httpd_resp_send_chunk(req, buf, n);                                                          \
        i++;                                                                                         \
    }
    SETTINGS_TABLE(X)
#undef X
    httpd_resp_send_chunk(req, "]", 1);
    return httpd_resp_send_chunk(req, NULL, 0);
}

// ---------------------------------------------------------------- mensajes de la pagina
static double num(const cJSON *j, const char *key, double def)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(j, key);
    return cJSON_IsNumber(v) ? v->valuedouble : def;
}

static void handle_message(const char *text)
{
    cJSON *j = cJSON_Parse(text);
    if (!j) return;
    const cJSON *t = cJSON_GetObjectItemCaseSensitive(j, "t");
    if (cJSON_IsString(t) && t->valuestring) {
        const char *ty = t->valuestring;
        if (strcmp(ty, "ctl") == 0) {
            bridge_set_ctl((int)lround(num(j, "th", 0)), (int)lround(num(j, "st", 0)), num(j, "br", 0) != 0);
        } else if (strcmp(ty, "lig") == 0) {
            bridge_set_lights(num(j, "fl", 0) != 0, num(j, "rgb", 1) != 0, (int)num(j, "col", 0));
        } else if (strcmp(ty, "auto") == 0) {
            bridge_set_auto(num(j, "on", 0) != 0);
        } else if (strcmp(ty, "goto") == 0) {
            double la = num(j, "la", 0), lo = num(j, "lo", 0);
            if (fabs(la) <= 90 && fabs(lo) <= 180 && (la != 0 || lo != 0)) {
                bridge_nav_goto((int32_t)llround(la * 1e7), (int32_t)llround(lo * 1e7));
            }
        } else if (strcmp(ty, "cancel") == 0) {
            bridge_nav_cancel();
        } else if (strcmp(ty, "set") == 0) {
            int k = (int)num(j, "k", -1);
            if (k >= 0 && k < SET_COUNT) bridge_cfg_push(CFG_OP_SET, k, (float)num(j, "v", 0));
        } else if (strcmp(ty, "get") == 0) {
            int k = (int)num(j, "k", -1);
            if (k >= 0 && k < SET_COUNT) bridge_cfg_push(CFG_OP_GET, k, 0);
        } else if (strcmp(ty, "reset") == 0) {
            bridge_cfg_push(CFG_OP_RESET, 0, 0);
        } else if (strcmp(ty, "test") == 0) {
            bridge_test((int)num(j, "id", 0), (int)lround(num(j, "v", 0)), (int)num(j, "ms", 1000));
        }
    }
    cJSON_Delete(j);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket conectado");
        return ESP_OK;
    }
    httpd_ws_frame_t f;
    memset(&f, 0, sizeof(f));
    f.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t err = httpd_ws_recv_frame(req, &f, 0);
    if (err != ESP_OK) return err;
    if (f.len == 0) return ESP_OK;
    if (f.len > 320) return ESP_FAIL;

    uint8_t buf[324];
    f.payload = buf;
    err = httpd_ws_recv_frame(req, &f, f.len);
    if (err != ESP_OK) return err;
    buf[f.len] = 0;
    if (f.type == HTTPD_WS_TYPE_TEXT) handle_message((const char *)buf);
    return ESP_OK;
}

// ---------------------------------------------------------------- envio periodico del estado
static int build_status(char *buf, size_t cap)
{
    p4_to_s3_t st;
    uint32_t age = 0;
    bool have = bridge_get_status(&st, &age);
    if (!have || age > 600) {
        return snprintf(buf, cap, "{\"t\":\"st\",\"ok\":0,\"age\":%lu}", (unsigned long)age);
    }
    int n = snprintf(buf, cap,
        "{\"t\":\"st\",\"ok\":1,\"lm\":%d,\"ns\":%d,\"so\":%d,\"df\":%d,\"th\":%d,\"sr\":%d,\"bp\":%d,"
        "\"sa\":%d,\"vb\":%d,\"cu\":%d,\"rl\":%d,\"rr\":%d,\"sp\":%d,\"hd\":%d,\"pi\":%d,\"ro\":%d,"
        "\"sv\":%d,\"hp\":%d,\"la\":%ld,\"lo\":%ld,\"wa\":%ld,\"wo\":%ld,\"wd\":%d,\"wb\":%d,"
        "\"gs\":%d,\"gc\":%d,\"gf\":%d,\"ti\":%d,\"ld\":[",
        st.link_mode, st.nav_state, st.sensor_ok, st.drive_flags, st.throttle, st.steer, st.batt_pct,
        st.sats, st.vbat_mv, st.current_ma, st.rpm_l, st.rpm_r, st.speed_x100, st.heading_d10,
        st.pitch_d10, st.roll_d10, st.servo_d10, st.hdop_x100, (long)st.lat_e7, (long)st.lon_e7,
        (long)st.wp_lat_e7, (long)st.wp_lon_e7, st.wp_dist_dm, st.wp_bearing_d10, st.gps_speed_x100,
        st.gps_course_d10, st.gps_fix, st.test_id);
    for (int i = 0; i < LIDAR_SECTORS && n < (int)cap - 16; i++) {
        n += snprintf(buf + n, cap - n, "%s%d", i ? "," : "", st.lidar_cm[i]);
    }
    n += snprintf(buf + n, cap - n, "]}");
    return n;
}

static void push_task(void *arg)
{
    static char buf[1100];
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(PUSH_PERIOD_MS));

        int fds[8];
        size_t nfds = sizeof(fds) / sizeof(fds[0]);
        if (httpd_get_client_list(s_server, &nfds, fds) != ESP_OK) continue;
        int ws[8], nws = 0;
        for (size_t i = 0; i < nfds; i++) {
            if (httpd_ws_get_fd_info(s_server, fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) ws[nws++] = fds[i];
        }
        bridge_set_ws_clients(nws);

        int key;
        float val;
        if (nws == 0) {                       // nadie escucha: descartar respuestas viejas
            while (bridge_cfg_pop_reply(&key, &val)) {}
            continue;
        }

        int n = build_status(buf, sizeof(buf));
        httpd_ws_frame_t f = {.final = true, .type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t *)buf, .len = (size_t)n};
        for (int i = 0; i < nws; i++) httpd_ws_send_frame_async(s_server, ws[i], &f);

        while (bridge_cfg_pop_reply(&key, &val)) {
            char r[64];
            int m = snprintf(r, sizeof(r), "{\"t\":\"cfg\",\"k\":%d,\"v\":%g}", key, (double)val);
            httpd_ws_frame_t rf = {.final = true, .type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t *)r, .len = (size_t)m};
            for (int i = 0; i < nws; i++) httpd_ws_send_frame_async(s_server, ws[i], &rf);
        }
    }
}

void web_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = 7;
    cfg.lru_purge_enable = true;
    cfg.stack_size = 8192;
    cfg.max_uri_handlers = 8;
    ESP_ERROR_CHECK(httpd_start(&s_server, &cfg));

    httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_get};
    httpd_uri_t fav_uri = {.uri = "/favicon.ico", .method = HTTP_GET, .handler = favicon_get};
    httpd_uri_t set_uri = {.uri = "/api/settings", .method = HTTP_GET, .handler = settings_meta_get};
    httpd_uri_t ws_uri = {.uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true};
    httpd_register_uri_handler(s_server, &index_uri);
    httpd_register_uri_handler(s_server, &fav_uri);
    httpd_register_uri_handler(s_server, &set_uri);
    httpd_register_uri_handler(s_server, &ws_uri);

    xTaskCreate(push_task, "ws_push", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "servidor web listo");
}
