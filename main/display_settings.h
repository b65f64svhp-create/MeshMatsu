#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

enum { MESH_ALERT_CHANNEL = 1, MESH_ALERT_DIRECT = 2, MESH_ALERT_BOTH = 3 };
typedef struct {
    uint8_t screen_brightness;
    uint8_t keyboard_brightness;
    uint16_t timeout_seconds; // 0 disables the idle timer.
    uint8_t unread_light;
    uint8_t night_brightness;
    uint8_t alert_sources;
} mesh_display_settings_t;

mesh_display_settings_t mesh_display_defaults(uint8_t screen, uint8_t keyboard);
bool mesh_display_valid(const mesh_display_settings_t *settings);
esp_err_t mesh_display_load(mesh_display_settings_t *settings);
esp_err_t mesh_display_save(const mesh_display_settings_t *settings);
bool mesh_display_alert(const mesh_display_settings_t *settings, bool channels, bool direct);
bool mesh_display_idle_due(const mesh_display_settings_t *settings, int64_t now, int64_t last_activity);
