#pragma once
#include "map_archive.h"
#define MESH_MAP_CACHE_TILES 24
// Worker-owned cache; the UI never reads or mutates tile storage.
typedef struct {
    bool valid;
    unsigned z, x, y;
    uint64_t used;
    uint32_t pinned;
} mesh_map_cache_entry_t;
typedef struct {
    mesh_map_cache_entry_t entries[MESH_MAP_CACHE_TILES];
    uint16_t *pixels;
    size_t capacity;
    uint64_t clock, hits, misses;
    uint32_t view;
} mesh_map_cache_t;
void mesh_map_cache_init(mesh_map_cache_t *cache, uint16_t *pixels, size_t capacity);
void mesh_map_cache_begin_view(mesh_map_cache_t *cache);
void mesh_map_cache_pin(mesh_map_cache_t *cache, unsigned z, unsigned x, unsigned y);
const uint16_t *mesh_map_cache_tile(mesh_map_cache_t *cache, mesh_map_archive_t *archive,
    unsigned z, unsigned x, unsigned y, uint8_t *compressed, uint16_t *scratch);
