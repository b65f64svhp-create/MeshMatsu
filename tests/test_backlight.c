#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "backlight.h"
#include "bsp/tanmatsu.h"

static uint8_t screen = 128, keys = 64;
static int handle_error, write_error, read_error;
static bool ignore_write;
static unsigned writes;
int bsp_tanmatsu_coprocessor_get_handle(tanmatsu_coprocessor_handle_t *h) { *h = (void *)1; return handle_error; }
int tanmatsu_coprocessor_set_display_backlight(void *h, uint8_t value) {
    assert(h == (void *)1); writes++;
    if (!write_error && !ignore_write) screen = value;
    return write_error;
}
int tanmatsu_coprocessor_set_keyboard_backlight(void *h, uint8_t value) {
    assert(h == (void *)1); writes++;
    if (!write_error && !ignore_write) keys = value;
    return write_error;
}
int tanmatsu_coprocessor_get_display_backlight(void *h, uint8_t *value) { *value = screen; return read_error; }
int tanmatsu_coprocessor_get_keyboard_backlight(void *h, uint8_t *value) { *value = keys; return read_error; }

int main(void) {
    assert(mesh_backlight_raw(0) == 0 && mesh_backlight_raw(100) == 255);
    assert(mesh_backlight_raw(255) == 255);
    assert(mesh_backlight_raw(20) == 7 && mesh_backlight_raw(50) == 55);
    uint8_t previous = 0;
    for (unsigned i = 1; i <= 100; i++) {
        uint8_t raw = mesh_backlight_raw(i);
        assert(raw && raw >= previous); previous = raw;
    }
    // Every selectable five-point step changes the hardware duty, including
    // the lowest keyboard levels. Integer percentage conversion is bypassed.
    previous = 0;
    for (unsigned i = 5; i <= 100; i += 5) {
        uint8_t raw = mesh_backlight_raw(i);
        assert(raw > previous); previous = raw;
        assert(mesh_backlight_set(false, i) == 0 && screen == raw);
        assert(mesh_backlight_set(true, i) == 0 && keys == raw);
        uint8_t actual;
        assert(mesh_backlight_get(false, &actual) == 0);
        assert(abs((int)mesh_backlight_raw(actual) - raw) <= 3);
    }
    for (unsigned raw = 0; raw <= 255; raw++) {
        assert(abs((int)mesh_backlight_raw(mesh_backlight_level(raw)) - (int)raw) <= 3);
    }
    assert(mesh_backlight_set(true, 0) == 0 && keys == 0 && screen == 255);
    assert(mesh_backlight_set(false, 0) == 0 && screen == 0);
    assert(mesh_backlight_set(false, 10) == 0 && screen == 2);
    unsigned count = writes;
    assert(mesh_backlight_set(false, 101) == ESP_ERR_INVALID_ARG && writes == count);
    assert(mesh_backlight_get(false, NULL) == ESP_ERR_INVALID_ARG);
    handle_error = 9; assert(mesh_backlight_set(false, 50) == 9 && writes == count);
    handle_error = 0; write_error = 9; assert(mesh_backlight_set(false, 50) == 9);
    write_error = 0; read_error = 9; assert(mesh_backlight_set(false, 50) == 9);
    uint8_t actual = 99; assert(mesh_backlight_get(false, &actual) == 9 && actual == 99);
    read_error = 0; ignore_write = true;
    assert(mesh_backlight_set(false, 80) == ESP_ERR_INVALID_STATE);
    assert(mesh_backlight_set(true, 80) == ESP_ERR_INVALID_STATE);
    puts("PASS: perceptual curve, distinct 5-point steps, nonzero dim floor, full 8-bit controller writes/readback, inverse mapping, off and error handling");
}
