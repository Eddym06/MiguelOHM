#include "settings.h"
#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "settings";

typedef struct { float def, min, max; } meta_t;

static const meta_t s_meta[SET_COUNT] = {
#define X(id, label, unit, def, mn, mx, step) {(float)(def), (float)(mn), (float)(mx)},
    SETTINGS_TABLE(X)
#undef X
};

static float s_val[SET_COUNT];
static nvs_handle_t s_nvs;
static bool s_nvs_ok;

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void key_name(int id, char *out, size_t n)
{
    snprintf(out, n, "s%d", id);
}

void settings_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    s_nvs_ok = (err == ESP_OK) && (nvs_open("kakata", NVS_READWRITE, &s_nvs) == ESP_OK);
    if (!s_nvs_ok) ESP_LOGW(TAG, "NVS no disponible: se usan valores por defecto");

    int loaded = 0;
    for (int i = 0; i < SET_COUNT; i++) {
        s_val[i] = s_meta[i].def;
        if (!s_nvs_ok) continue;
        char key[8];
        key_name(i, key, sizeof(key));
        uint32_t raw;
        if (nvs_get_u32(s_nvs, key, &raw) == ESP_OK) {
            float f;
            memcpy(&f, &raw, sizeof(f));
            if (f == f) {  // descarta NaN
                s_val[i] = clampf(f, s_meta[i].min, s_meta[i].max);
                loaded++;
            }
        }
    }
    ESP_LOGI(TAG, "%d ajustes, %d cargados de NVS", SET_COUNT, loaded);
}

float settings_get(setting_id_t id)
{
    if ((int)id < 0 || id >= SET_COUNT) return 0;
    return s_val[id];
}

float settings_set(setting_id_t id, float value)
{
    if ((int)id < 0 || id >= SET_COUNT) return 0;
    if (value != value) return s_val[id];
    float v = clampf(value, s_meta[id].min, s_meta[id].max);
    s_val[id] = v;
    if (s_nvs_ok) {
        char key[8];
        key_name(id, key, sizeof(key));
        uint32_t raw;
        memcpy(&raw, &v, sizeof(raw));
        nvs_set_u32(s_nvs, key, raw);
        nvs_commit(s_nvs);
    }
    return v;
}

void settings_reset_all(void)
{
    for (int i = 0; i < SET_COUNT; i++) {
        s_val[i] = s_meta[i].def;
        if (s_nvs_ok) {
            char key[8];
            key_name(i, key, sizeof(key));
            nvs_erase_key(s_nvs, key);
        }
    }
    if (s_nvs_ok) nvs_commit(s_nvs);
    ESP_LOGI(TAG, "ajustes restaurados a valores por defecto");
}
