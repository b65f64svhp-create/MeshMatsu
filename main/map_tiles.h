#pragma once
#include "map_view.h"
#include "pax_gfx.h"
// Starts lazy SD reads in a worker; never formats or writes the SD card.
void mesh_map_tiles_retry(void);
void mesh_map_tiles_request(const mesh_map_view_t *view, int width, int height);
bool mesh_map_tiles_needs_draw(void);
bool mesh_map_tiles_draw(pax_buf_t *fb, int x, int y, const mesh_map_view_t *view,
                         int width, int height, char *status, size_t status_size);

// Shared lazy mount for encrypted backups; serializes mount/power setup with maps.
bool mesh_map_storage_mount(void);
