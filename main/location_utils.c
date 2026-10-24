#include "location_utils.h"
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
bool mesh_location_parse(const char *text, bool latitude, int32_t *out) {
    if (!text || !out || !*text) return false;
    const char *p = text; if (*p == '-' || *p == '+') p++;
    bool digit = false, dot = false; unsigned decimals = 0;
    for (; *p; p++) {
        if (*p == '.' && !dot) { dot = true; continue; }
        if (*p < '0' || *p > '9' || (dot && ++decimals > 7)) return false;
        digit = true;
    }
    if (!digit) return false;
    double value = strtod(text, NULL), limit = latitude ? 90 : 180;
    if (!isfinite(value) || value < -limit || value > limit) return false;
    *out = (int32_t)llround(value * 1e7); return true;
}
bool mesh_location_distance(const mesh_node_t *from, const mesh_node_t *to, double *meters, double *bearing) {
    if (!from || !to || !meters || !bearing || !from->position_available || !to->position_available) return false;
    const double rad = 0.017453292519943295;
    double a = from->latitude_i / 1e7 * rad, b = to->latitude_i / 1e7 * rad;
    double dl = (to->longitude_i / 1e7 - from->longitude_i / 1e7) * rad;
    double s = sin((b-a)/2), t = sin(dl/2);
    double h = s*s + cos(a)*cos(b)*t*t; h = fmax(0, fmin(1, h));
    *meters = 6371000 * 2 * atan2(sqrt(h), sqrt(1-h));
    *bearing = fmod(atan2(sin(dl)*cos(b), cos(a)*sin(b)-sin(a)*cos(b)*cos(dl))/rad + 360, 360);
    return true;
}
const char *mesh_location_compass(double bearing) {
    static const char *directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return directions[(unsigned)floor(fmod(bearing + 382.5, 360) / 45) % 8];
}
const char *mesh_location_precision_label(int bits) {
    static const char *labels[] = {"Approx. 23.3 km", "Approx. 11.7 km", "Approx. 5.8 km", "Approx. 2.9 km",
        "Approx. 1.5 km", "Approx. 729 m", "Approx. 364 m", "Approx. 182 m", "Approx. 91 m", "Approx. 45 m"};
    if (bits < 0) return "Keep current radio setting";
    if (!bits) return "Off";
    if (bits == 32) return "Full precision";
    return bits >= 10 && bits <= 19 ? labels[bits-10] : "Custom radio precision";
}
int mesh_location_precision_step(int current, bool forward, bool public_key) {
    static const int values[] = {-1,0,10,11,12,13,14,15,16,17,18,19,32};
    unsigned count = public_key ? 8 : 13, selected = 0;
    for (unsigned i=0; i<count; i++) if (values[i] == current) selected=i;
    return values[(selected + count + (forward ? 1 : -1)) % count];
}
