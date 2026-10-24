#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MESH_TLM_LIMIT 4096
#define MESH_TLM_SERIES_LIMIT 96
#define MESH_TLM_FIELDS 22
enum {MESH_TLM_DEVICE=2,MESH_TLM_ENVIRONMENT=3,MESH_TLM_AIR=4,MESH_TLM_POWER=5,MESH_TLM_LOCAL=6};
enum {MESH_TLM_CACHED=1,MESH_TLM_RESTORED=2,MESH_TLM_CLOCK_SKEW=4};
typedef struct {
    uint32_t node,time,received_at,packet,present;
    uint8_t kind,flags;
    int64_t received_us;
    double value[MESH_TLM_FIELDS];
} mesh_tlm_sample_t;
typedef struct {const char *name,*unit;double min,max;bool integer,temperature;} mesh_tlm_field_t;
const mesh_tlm_field_t *mesh_tlm_field(uint8_t kind,unsigned field);
const char *mesh_tlm_family(uint8_t kind);
void mesh_tlm_reset(void);
void mesh_tlm_forget(uint32_t node);
bool mesh_tlm_add(const mesh_tlm_sample_t *sample);
bool mesh_tlm_import(const mesh_tlm_sample_t *sample,uint64_t order);
bool mesh_tlm_latest(uint32_t node,uint8_t kind,unsigned field,mesh_tlm_sample_t *out);
uint32_t mesh_tlm_fields(uint32_t node,uint8_t kind);
size_t mesh_tlm_series(uint32_t node,uint8_t kind,mesh_tlm_sample_t *out,size_t capacity);
size_t mesh_tlm_snapshot(mesh_tlm_sample_t *out,size_t capacity);
size_t mesh_tlm_count(void);
uint64_t mesh_tlm_revision(void);
bool mesh_tlm_valid(const mesh_tlm_sample_t *sample);
