#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef void *tanmatsu_coprocessor_handle_t;
esp_err_t tanmatsu_coprocessor_set_display_backlight(tanmatsu_coprocessor_handle_t, uint8_t);
esp_err_t tanmatsu_coprocessor_set_keyboard_backlight(tanmatsu_coprocessor_handle_t, uint8_t);
esp_err_t tanmatsu_coprocessor_get_display_backlight(tanmatsu_coprocessor_handle_t, uint8_t *);
esp_err_t tanmatsu_coprocessor_get_keyboard_backlight(tanmatsu_coprocessor_handle_t, uint8_t *);
