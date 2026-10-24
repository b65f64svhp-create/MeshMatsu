#include "power_settings.h"
#include "nvs.h"
#include <stddef.h>

mesh_power_settings_t mesh_power_defaults(void) {
    // Preserve existing behavior until the user enables individual savings.
    return (mesh_power_settings_t){1, 0, 0, 0};
}
static bool valid(const mesh_power_settings_t *s) {
    return s && s->decorative_lights <= 1 && (s->keyboard_timeout_seconds == 0 || s->keyboard_timeout_seconds == 15 || s->keyboard_timeout_seconds == 30 || s->keyboard_timeout_seconds == 45 || s->keyboard_timeout_seconds == 60) &&
        (s->dim_timeout_seconds == 0 || s->dim_timeout_seconds == 15 || s->dim_timeout_seconds == 30 || s->dim_timeout_seconds == 45 || s->dim_timeout_seconds == 60) && s->reduced_background <= 1;
}
esp_err_t mesh_power_load(mesh_power_settings_t *s) {
    nvs_handle_t handle;
    esp_err_t result = nvs_open("meshdeck_mui", NVS_READONLY, &handle);
    if (result != ESP_OK) return result;
    uint8_t data[7]; size_t size = sizeof(data);
    result = nvs_get_blob(handle, "power_v1", data, &size);
    nvs_close(handle);
    if (result != ESP_OK) return result;
    mesh_power_settings_t loaded;
    if (size == 5 && data[0] == 1 && data[2] <= 1 && data[3] <= 1)
        loaded = (mesh_power_settings_t){data[1], data[2] ? 15 : 0, data[3] ? 15 : 0, data[4]};
    else if (size == 7 && data[0] == 2)
        loaded = (mesh_power_settings_t){data[1], data[2] | (uint16_t)data[3] << 8, data[4] | (uint16_t)data[5] << 8, data[6]};
    else return ESP_ERR_INVALID_ARG;
    if (!valid(&loaded)) return ESP_ERR_INVALID_ARG;
    *s = loaded;
    return ESP_OK;
}
esp_err_t mesh_power_save(const mesh_power_settings_t *s) {
    if (!valid(s)) return ESP_ERR_INVALID_ARG;
    uint8_t data[] = {2, s->decorative_lights, (uint8_t)s->keyboard_timeout_seconds, (uint8_t)(s->keyboard_timeout_seconds >> 8), (uint8_t)s->dim_timeout_seconds, (uint8_t)(s->dim_timeout_seconds >> 8), s->reduced_background};
    nvs_handle_t handle;
    esp_err_t result = nvs_open("meshdeck_mui", NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;
    result = nvs_set_blob(handle, "power_v1", data, sizeof(data));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result;
}
mesh_power_backlights_t mesh_power_backlights(const mesh_power_settings_t *s, bool asleep,
    int64_t now, int64_t last_activity, uint8_t screen, uint8_t keyboard) {
    if (asleep) return (mesh_power_backlights_t){0, 0};
    int64_t idle = now >= last_activity ? now - last_activity : 0;
    if (s->dim_timeout_seconds && idle >= (int64_t)s->dim_timeout_seconds * 1000000) {
        screen /= 4; if (!screen) screen = 1;
    }
    if (s->keyboard_timeout_seconds && idle >= (int64_t)s->keyboard_timeout_seconds * 1000000) keyboard = 0;
    return (mesh_power_backlights_t){screen, keyboard};
}
unsigned mesh_power_wait_ms(const mesh_power_settings_t *s, bool asleep, bool composing) {
    if (composing) return 10;
    return s->reduced_background ? (asleep ? 100 : 50) : 20;
}
int64_t mesh_power_redraw_interval(const mesh_power_settings_t *s, bool dirty, bool settings_poll) {
    return !s->reduced_background || dirty || settings_poll ? 1000000 : 30000000;
}
