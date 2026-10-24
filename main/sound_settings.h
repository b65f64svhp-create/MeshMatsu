#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "esp_err.h"

typedef struct {
    uint8_t message_sounds;
    uint8_t game_sounds;
    uint8_t volume;
    uint8_t quiet_enabled;
    uint8_t quiet_start_hour;
    uint8_t quiet_end_hour;
} mesh_sound_settings_t;

mesh_sound_settings_t mesh_sound_defaults(void);
bool mesh_sound_valid(const mesh_sound_settings_t *settings);
esp_err_t mesh_sound_load(mesh_sound_settings_t *settings);
esp_err_t mesh_sound_save(const mesh_sound_settings_t *settings);
// Uses the app's configured local timezone. Equal hours mean quiet all day.
// When quiet hours are enabled, an unset clock is treated as quiet.
bool mesh_sound_quiet_at(const mesh_sound_settings_t *settings, time_t now);
bool mesh_sound_allowed(const mesh_sound_settings_t *settings, bool game, time_t now);
