#include "map_cache.h"
#include <string.h>
void mesh_map_cache_init(mesh_map_cache_t *cache, uint16_t *pixels, size_t capacity) {
    memset(cache, 0, sizeof(*cache));
    cache->pixels = pixels;
    cache->capacity = pixels ? (capacity > MESH_MAP_CACHE_TILES ? MESH_MAP_CACHE_TILES : capacity) : 0;
}
static bool matches(const mesh_map_cache_entry_t *e, unsigned z, unsigned x, unsigned y) {
    return e->valid && e->z == z && e->x == x && e->y == y;
}
void mesh_map_cache_begin_view(mesh_map_cache_t *cache) {
    if (++cache->view == 0) {
        cache->view = 1;
        for (size_t i = 0; i < cache->capacity; i++) cache->entries[i].pinned = 0;
    }
}
void mesh_map_cache_pin(mesh_map_cache_t *cache, unsigned z, unsigned x, unsigned y) {
    for (size_t i = 0; i < cache->capacity; i++) if (matches(&cache->entries[i], z, x, y)) cache->entries[i].pinned = cache->view;
}
const uint16_t *mesh_map_cache_tile(mesh_map_cache_t *cache, mesh_map_archive_t *archive,
    unsigned z, unsigned x, unsigned y, uint8_t *compressed, uint16_t *scratch) {
    size_t replace = cache->capacity;
    for (size_t i = 0; i < cache->capacity; i++) {
        mesh_map_cache_entry_t *entry = &cache->entries[i];
        if (matches(entry, z, x, y)) {
            entry->used = ++cache->clock; entry->pinned = cache->view; cache->hits++;
            return cache->pixels + i * (MESH_MAP_TILE_BYTES / 2);
        }
        if (entry->valid && entry->pinned == cache->view) continue;
        if (replace == cache->capacity || !entry->valid ||
            (cache->entries[replace].valid && entry->used < cache->entries[replace].used)) replace = i;
    }
    cache->misses++;
    uint16_t *pixels = replace < cache->capacity ? cache->pixels + replace * (MESH_MAP_TILE_BYTES / 2) : scratch;
    if (replace < cache->capacity) cache->entries[replace].valid = false;
    if (!mesh_map_archive_tile(archive, z, x, y, compressed, pixels)) return NULL;
    if (replace < cache->capacity) cache->entries[replace] = (mesh_map_cache_entry_t){
        .valid = true, .z = z, .x = x, .y = y, .used = ++cache->clock, .pinned = cache->view};
    return pixels;
}
