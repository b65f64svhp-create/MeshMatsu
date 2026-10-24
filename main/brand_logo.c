#include "brand_logo.h"
#include "brand_logo_pixels.inc"
void mesh_brand_draw(pax_buf_t *target, float x, float y) {
    static pax_buf_t image;
    static bool ready;
    if (!ready) ready = pax_buf_init(&image, (void *)mesh_brand_pixels, 264, 88, PAX_BUF_16_565RGB);
    if (ready) pax_draw_image(target, &image, x, y);
}
