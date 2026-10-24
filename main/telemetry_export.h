#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum {MESH_EXPORT_OK,MESH_EXPORT_BUSY,MESH_EXPORT_EMPTY,MESH_EXPORT_MEMORY,MESH_EXPORT_INVALID} mesh_tlm_export_start_t;
typedef struct {bool busy,success;uint32_t node;uint8_t kind;size_t reports,rows;char filename[64],message[112];} mesh_tlm_export_status_t;
mesh_tlm_export_start_t mesh_tlm_export_start(uint32_t node,uint8_t kind);
// Called on the main loop: true when a background completion changes the UI.
bool mesh_tlm_export_poll(void);
bool mesh_tlm_export_busy(void);
mesh_tlm_export_status_t mesh_tlm_export_status(void);
