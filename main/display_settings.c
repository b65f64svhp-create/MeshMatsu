#include "display_settings.h"
#include "nvs.h"
#include "backlight.h"
#include <string.h>

mesh_display_settings_t mesh_display_defaults(uint8_t screen, uint8_t keyboard) {
    return (mesh_display_settings_t){screen >= 10 && screen <= 100 ? screen : 100,
        keyboard <= 100 ? keyboard : 100, 0, 1, 20, MESH_ALERT_BOTH};
}

bool mesh_display_valid(const mesh_display_settings_t *s) {
    return s && s->screen_brightness >= 10 && s->screen_brightness <= 100 &&
        s->keyboard_brightness <= 100 &&
        (s->timeout_seconds == 0 || s->timeout_seconds == 15 || s->timeout_seconds == 30 || s->timeout_seconds == 45 || s->timeout_seconds == 60 || s->timeout_seconds == 120 || s->timeout_seconds == 300 || s->timeout_seconds == 900) &&
        s->unread_light <= 1 && s->night_brightness >= 5 && s->night_brightness <= 100 &&
        s->alert_sources >= MESH_ALERT_CHANNEL && s->alert_sources <= MESH_ALERT_BOTH;
}

esp_err_t mesh_display_load(mesh_display_settings_t *s) {
    nvs_handle_t handle;
    esp_err_t result = nvs_open("meshdeck_mui", NVS_READONLY, &handle);
    if (result != ESP_OK) return result;
    uint8_t data[8];
    size_t size = sizeof(data);
    result = nvs_get_blob(handle, "display_v1", data, &size);
    nvs_close(handle);
    if (result != ESP_OK) return result;
    mesh_display_settings_t loaded;
    if (size == 7 && (data[0] == 1 || data[0] == 2)) {
        if (data[3] != 0 && data[3] != 1 && data[3] != 5 && data[3] != 15) return ESP_ERR_INVALID_ARG;
        loaded = (mesh_display_settings_t){data[1], data[2], data[3] * 60u, data[4], data[5], data[6]};
    } else if (size == 8 && data[0] == 3) {
        loaded = (mesh_display_settings_t){data[1], data[2], data[3] | (uint16_t)data[4] << 8, data[5], data[6], data[7]};
    } else return ESP_ERR_INVALID_ARG;
    if (!mesh_display_valid(&loaded)) return ESP_ERR_INVALID_ARG;
    if (data[0] == 1) {
        // Earlier sliders stored linear PWM percentages. Keep approximately
        // the same light output when changing to perceptual UI levels.
        loaded.screen_brightness = mesh_backlight_level(loaded.screen_brightness * 255u / 100);
        if (loaded.screen_brightness < 10) loaded.screen_brightness = 10;
        loaded.keyboard_brightness = mesh_backlight_level(loaded.keyboard_brightness * 255u / 100);
    }
    *s = loaded;
    return ESP_OK;
}

esp_err_t mesh_display_save(const mesh_display_settings_t *s) {
    if (!mesh_display_valid(s)) return ESP_ERR_INVALID_ARG;
    uint8_t data[] = {3, s->screen_brightness, s->keyboard_brightness, (uint8_t)s->timeout_seconds, (uint8_t)(s->timeout_seconds >> 8),
        s->unread_light, s->night_brightness, s->alert_sources};
    nvs_handle_t handle;
    esp_err_t result = nvs_open("meshdeck_mui", NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;
    result = nvs_set_blob(handle, "display_v1", data, sizeof(data));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result;
}

bool mesh_display_alert(const mesh_display_settings_t *s, bool channels, bool direct) {
    return s->unread_light && ((channels && (s->alert_sources & MESH_ALERT_CHANNEL)) ||
        (direct && (s->alert_sources & MESH_ALERT_DIRECT)));
}

bool mesh_display_idle_due(const mesh_display_settings_t *s, int64_t now, int64_t last_activity) {
    return s->timeout_seconds && now >= last_activity &&
        now - last_activity >= (int64_t)s->timeout_seconds * 1000000;
}
