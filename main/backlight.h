#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Perceptual UI level (0..100) <-> full 8-bit backlight duty.
uint8_t mesh_backlight_raw(uint8_t level);
uint8_t mesh_backlight_level(uint8_t raw);
esp_err_t mesh_backlight_set(bool keyboard, uint8_t level);
esp_err_t mesh_backlight_get(bool keyboard, uint8_t *level);
