#include "sound_settings.h"
#include "nvs.h"

mesh_sound_settings_t mesh_sound_defaults(void) {
    return (mesh_sound_settings_t){0, 0, 35, 0, 22, 7};
}

bool mesh_sound_valid(const mesh_sound_settings_t *s) {
    return s && s->message_sounds <= 1 && s->game_sounds <= 1 &&
        s->volume <= 100 && s->quiet_enabled <= 1 &&
        s->quiet_start_hour < 24 && s->quiet_end_hour < 24;
}

esp_err_t mesh_sound_load(mesh_sound_settings_t *s) {
    if (!s) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t result = nvs_open("meshdeck_mui", NVS_READONLY, &handle);
    if (result != ESP_OK) return result;
    uint8_t data[7];
    size_t size = sizeof(data);
    result = nvs_get_blob(handle, "sound_v1", data, &size);
    nvs_close(handle);
    if (result != ESP_OK) return result;
    if (size != sizeof(data) || data[0] != 1) return ESP_ERR_INVALID_ARG;
    mesh_sound_settings_t loaded = {data[1], data[2], data[3], data[4], data[5], data[6]};
    if (!mesh_sound_valid(&loaded)) return ESP_ERR_INVALID_ARG;
    *s = loaded;
    return ESP_OK;
}

esp_err_t mesh_sound_save(const mesh_sound_settings_t *s) {
    if (!mesh_sound_valid(s)) return ESP_ERR_INVALID_ARG;
    uint8_t data[] = {1, s->message_sounds, s->game_sounds, s->volume,
        s->quiet_enabled, s->quiet_start_hour, s->quiet_end_hour};
    nvs_handle_t handle;
    esp_err_t result = nvs_open("meshdeck_mui", NVS_READWRITE, &handle);
    if (result != ESP_OK) return result;
    result = nvs_set_blob(handle, "sound_v1", data, sizeof(data));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);
    return result;
}

bool mesh_sound_quiet_at(const mesh_sound_settings_t *s, time_t now) {
    if (!mesh_sound_valid(s) || !s->quiet_enabled) return false;
    if (now < (time_t)1700000000) return true;
    struct tm local;
    if (!localtime_r(&now, &local)) return true;
    unsigned hour = local.tm_hour;
    if (s->quiet_start_hour == s->quiet_end_hour) return true;
    if (s->quiet_start_hour < s->quiet_end_hour)
        return hour >= s->quiet_start_hour && hour < s->quiet_end_hour;
    return hour >= s->quiet_start_hour || hour < s->quiet_end_hour;
}

bool mesh_sound_allowed(const mesh_sound_settings_t *s, bool game, time_t now) {
    return mesh_sound_valid(s) && s->volume &&
        (game ? s->game_sounds : s->message_sounds) && !mesh_sound_quiet_at(s, now);
}
