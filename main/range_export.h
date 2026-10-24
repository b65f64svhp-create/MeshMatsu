#pragma once
#include "range_test.h"
#include <stdio.h>
typedef enum {
  MESH_RANGE_EXPORT_OK,
  MESH_RANGE_EXPORT_BUSY,
  MESH_RANGE_EXPORT_EMPTY,
  MESH_RANGE_EXPORT_MEMORY
} mesh_range_export_start_t;
typedef struct {
  bool busy, success;
  size_t rows;
  char filename[64], message[128];
} mesh_range_export_status_t;
typedef struct {
  size_t bytes, rows;
  uint32_t hash;
} mesh_range_csv_stats_t;
bool mesh_range_csv_write(FILE *file, const mesh_range_record_t *rows,
                          size_t count, const mesh_range_status_t *session,
                          mesh_range_csv_stats_t *stats);
mesh_range_export_start_t mesh_range_export_start(void);
bool mesh_range_export_poll(void);
bool mesh_range_export_busy(void);
mesh_range_export_status_t mesh_range_export_status(void);
