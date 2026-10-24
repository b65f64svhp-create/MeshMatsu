#pragma once
#include "telemetry_data.h"
#include <stdio.h>
typedef struct {size_t bytes,rows;uint32_t hash;} mesh_tlm_csv_stats_t;
// Long-format CSV, original units, empty missing values, UTC plus raw timestamps.
bool mesh_tlm_csv_write(FILE *file,const mesh_tlm_sample_t *samples,size_t count,mesh_tlm_csv_stats_t *stats);
