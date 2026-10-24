#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MESH_TRAIL_LIMIT 2048
#define MESH_TRAIL_NODE_LIMIT 64
#define MESH_TRAIL_AGE 86400u
#define MESH_TRAIL_GAP 3600u
typedef struct {
    int32_t latitude_i,longitude_i;
    uint32_t time,until; // First and last actual fix at this coordinate/precision.
    uint8_t precision;
} mesh_trail_point_t;
void mesh_trails_reset(void);
void mesh_trails_forget(uint32_t node);
bool mesh_trails_add(uint32_t node,int32_t latitude,int32_t longitude,uint32_t timestamp,uint8_t precision,uint32_t now);
size_t mesh_trails_series(uint32_t node,uint32_t now,mesh_trail_point_t *out,size_t capacity);
bool mesh_trails_join(const mesh_trail_point_t *a,const mesh_trail_point_t *b,uint32_t now);
size_t mesh_trails_count(void);
