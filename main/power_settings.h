#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint8_t decorative_lights;
    uint16_t keyboard_timeout_seconds;
    uint16_t dim_timeout_seconds;
    uint8_t reduced_background;
} mesh_power_settings_t;
typedef struct { uint8_t screen, keyboard; } mesh_power_backlights_t;

mesh_power_settings_t mesh_power_defaults(void);
esp_err_t mesh_power_load(mesh_power_settings_t *settings);
esp_err_t mesh_power_save(const mesh_power_settings_t *settings);
mesh_power_backlights_t mesh_power_backlights(const mesh_power_settings_t *settings, bool asleep,
    int64_t now, int64_t last_activity, uint8_t screen, uint8_t keyboard);
unsigned mesh_power_wait_ms(const mesh_power_settings_t *settings, bool asleep, bool composing);
int64_t mesh_power_redraw_interval(const mesh_power_settings_t *settings, bool dirty, bool settings_poll);
