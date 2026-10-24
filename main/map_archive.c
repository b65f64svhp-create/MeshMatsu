#include "map_archive.h"
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#ifdef ESP_PLATFORM
#include "miniz.h"
#include "esp_heap_caps.h"
#else
#include <zlib.h>
#endif
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static uint32_t checksum(const uint8_t *p, size_t size) {
#ifdef ESP_PLATFORM
    return (uint32_t)mz_crc32(0, p, size);
#else
    return (uint32_t)crc32(0, p, (uInt)size);
#endif
}
void mesh_map_archive_close(mesh_map_archive_t *a) { if (a->file) fclose(a->file); free(a->index); memset(a, 0, sizeof(*a)); }
bool mesh_map_archive_open(mesh_map_archive_t *a, const char *path) {
    memset(a, 0, sizeof(*a));
    FILE *f = fopen(path, "rb"); if (!f) return false;
    uint8_t header[16];
    if (fread(header, 1, 16, f) != 16 || memcmp(header, "MMAP0001", 8) ||
        fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long size = ftell(f); uint32_t count = le32(header + 8);
    if (size < 16 || (unsigned long)size > INT32_MAX || count > 1000000 ||
        (uint64_t)count * 24 + 16 > (uint64_t)size || le32(header + 12) != 256) { fclose(f); return false; }
    a->file = f; a->count = count; a->size = (uint32_t)size;
    size_t bytes = (size_t)count * 24;
    if (bytes && bytes <= MESH_MAP_INDEX_MAX_BYTES) {
#ifdef ESP_PLATFORM
        a->index = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
        a->index = malloc(bytes);
#endif
        if (a->index) {
            a->index_reads++;
            if (fseek(f, 16, SEEK_SET) || fread(a->index, 1, bytes, f) != bytes) {
                mesh_map_archive_close(a); return false;
            }
        }
    }
    return true;
}
bool mesh_map_archive_tile(mesh_map_archive_t *a, unsigned z, unsigned x, unsigned y,
                           uint8_t *compressed, uint16_t *pixels) {
    if (!a->file || z > 20 || x >= (1u << z) || y >= (1u << z)) return false;
    uint32_t low = 0, high = a->count;
    uint8_t entry[24];
    while (low < high) {
        uint32_t mid = low + (high - low) / 2;
        if (a->index) memcpy(entry, a->index + (size_t)mid * 24, 24);
        else {
            a->index_reads++;
            if (fseek(a->file, 16 + (long)mid * 24, SEEK_SET) || fread(entry, 1, 24, a->file) != 24) return false;
        }
        uint32_t ez = le32(entry), ex = le32(entry+4), ey = le32(entry+8);
        int cmp = ez != z ? (ez < z ? -1 : 1) : ex != x ? (ex < x ? -1 : 1) : ey != y ? (ey < y ? -1 : 1) : 0;
        if (cmp < 0) low = mid + 1;
        else if (cmp > 0) high = mid;
        else {
            a->tile_reads++;
            uint32_t offset = le32(entry+12), length = le32(entry+16), hash = le32(entry+20);
            if (offset < 16 + a->count * 24 || !length || length > MESH_MAP_COMPRESSED_MAX ||
                offset > a->size || length > a->size - offset || fseek(a->file, offset, SEEK_SET) ||
                fread(compressed, 1, length, a->file) != length) return false;
#ifdef ESP_PLATFORM
            size_t result = tinfl_decompress_mem_to_mem(pixels, MESH_MAP_TILE_BYTES, compressed, length, TINFL_FLAG_PARSE_ZLIB_HEADER);
            if (result != MESH_MAP_TILE_BYTES) return false;
#else
            uLongf result = MESH_MAP_TILE_BYTES;
            if (uncompress((Bytef *)pixels, &result, compressed, length) != Z_OK || result != MESH_MAP_TILE_BYTES) return false;
#endif
            if (checksum((const uint8_t *)pixels, MESH_MAP_TILE_BYTES) != hash) return false;
            a->decoded_tiles++;
            return true;
        }
    }
    return false;
}
