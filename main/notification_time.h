#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "esp_err.h"
typedef struct { uint8_t zone, use_24h; int16_t offset_minutes; } mesh_time_settings_t;
mesh_time_settings_t mesh_time_defaults(void);
size_t mesh_time_zone_count(void);
const char *mesh_time_zone_name(uint8_t zone);
bool mesh_time_valid(const mesh_time_settings_t *s);
void mesh_time_apply(const mesh_time_settings_t *s);
esp_err_t mesh_time_load(mesh_time_settings_t *s);
esp_err_t mesh_time_save(const mesh_time_settings_t *s);
void mesh_time_stamp(const mesh_time_settings_t *s, uint32_t timestamp, char *out, size_t size);
// Separate Home clock/date labels using the same timezone and format as messages.
bool mesh_time_clock(const mesh_time_settings_t *settings, uint32_t timestamp,
                     char *clock, size_t clock_size, char *date, size_t date_size);
esp_err_t mesh_channel_alerts_load(uint8_t *mask);
esp_err_t mesh_channel_alerts_save(uint8_t mask);
bool mesh_channel_alert_enabled(uint8_t mask, uint8_t channel);
