#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define MESH_MAP_TILE_BYTES (256 * 256 * 2)
#define MESH_MAP_COMPRESSED_MAX (MESH_MAP_TILE_BYTES + 1024)
#define MESH_MAP_INDEX_MAX_BYTES (2 * 1024 * 1024)
typedef struct {
    FILE *file;
    uint32_t count, size;
    uint8_t *index; // Optional RAM copy; large/low-memory archives use file lookup.
    uint64_t index_reads, tile_reads, decoded_tiles;
} mesh_map_archive_t;
bool mesh_map_archive_open(mesh_map_archive_t *archive, const char *path);
void mesh_map_archive_close(mesh_map_archive_t *archive);
bool mesh_map_archive_tile(mesh_map_archive_t *archive, unsigned z, unsigned x, unsigned y,
                           uint8_t *compressed, uint16_t *pixels);
