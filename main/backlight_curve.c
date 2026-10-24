#include "backlight.h"
#include <math.h>

uint8_t mesh_backlight_raw(uint8_t level) {
    if (!level) return 0;
    if (level >= 100) return 255;
    unsigned raw = (unsigned)lroundf(255.0f * powf(level / 100.0f, 2.2f));
    return raw ? raw : 1; // Only an explicit zero may turn the backlight off.
}

uint8_t mesh_backlight_level(uint8_t raw) {
    return raw ? (uint8_t)lroundf(100.0f * powf(raw / 255.0f, 1.0f / 2.2f)) : 0;
}
