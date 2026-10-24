#include "backlight.h"
#include <math.h>
#include <stdlib.h>
#include <stddef.h>
#if defined(CONFIG_BSP_TARGET_TANMATSU)
#include "bsp/tanmatsu.h"
#include "tanmatsu_coprocessor.h"
#else
#include "bsp/display.h"
#include "bsp/input.h"
#endif

esp_err_t mesh_backlight_set(bool keyboard, uint8_t level) {
    if (level > 100) return ESP_ERR_INVALID_ARG;
    uint8_t expected = mesh_backlight_raw(level), actual = 0;
#if defined(CONFIG_BSP_TARGET_TANMATSU)
    tanmatsu_coprocessor_handle_t handle = NULL;
    esp_err_t result = bsp_tanmatsu_coprocessor_get_handle(&handle);
    if (result != ESP_OK) return result;
    result = keyboard ? tanmatsu_coprocessor_set_keyboard_backlight(handle, expected) :
                        tanmatsu_coprocessor_set_display_backlight(handle, expected);
    if (result != ESP_OK) return result;
    result = keyboard ? tanmatsu_coprocessor_get_keyboard_backlight(handle, &actual) :
                        tanmatsu_coprocessor_get_display_backlight(handle, &actual);
    if (result == ESP_OK && actual != expected) return ESP_ERR_INVALID_STATE;
#else
    expected = (expected * 100u + 127) / 255;
    if (level && !expected) expected = 1;
    esp_err_t result = keyboard ? bsp_input_set_backlight_brightness(expected) : bsp_display_set_backlight_brightness(expected);
    if (result != ESP_OK) return result;
    result = keyboard ? bsp_input_get_backlight_brightness(&actual) : bsp_display_get_backlight_brightness(&actual);
    if (result == ESP_OK && abs((int)actual - expected) > 1) return ESP_ERR_INVALID_STATE;
#endif
    return result;
}

esp_err_t mesh_backlight_get(bool keyboard, uint8_t *level) {
    if (!level) return ESP_ERR_INVALID_ARG;
    uint8_t raw = 0;
#if defined(CONFIG_BSP_TARGET_TANMATSU)
    tanmatsu_coprocessor_handle_t handle = NULL;
    esp_err_t result = bsp_tanmatsu_coprocessor_get_handle(&handle);
    if (result != ESP_OK) return result;
    result = keyboard ? tanmatsu_coprocessor_get_keyboard_backlight(handle, &raw) :
                        tanmatsu_coprocessor_get_display_backlight(handle, &raw);
#else
    esp_err_t result = keyboard ? bsp_input_get_backlight_brightness(&raw) : bsp_display_get_backlight_brightness(&raw);
    raw = (raw * 255u) / 100;
#endif
    if (result == ESP_OK) *level = mesh_backlight_level(raw);
    return result;
}
