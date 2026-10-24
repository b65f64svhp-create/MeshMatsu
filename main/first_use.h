#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum {
  MESH_SETUP_IDLE,
  MESH_SETUP_READING,
  MESH_SETUP_READY,
  MESH_SETUP_CHECKING,
  MESH_SETUP_SAVING,
  MESH_SETUP_VERIFYING,
  MESH_SETUP_SAVED,
  MESH_SETUP_CHANGED,
  MESH_SETUP_FAILED,
  MESH_SETUP_TIMEOUT,
  MESH_SETUP_INTERRUPTED
} mesh_setup_state_t;
typedef struct {
  mesh_setup_state_t state;
  bool loaded;
  uint32_t node, revision, region, error;
  char name[40], short_name[5];
} mesh_setup_radio_t;
// Setup deliberately targets the tested Tanmatsu 2.7.18 firmware/schema.
bool mesh_radio_setup_supported(void);
bool mesh_radio_setup_busy(void);
mesh_setup_radio_t mesh_radio_setup(void);
bool mesh_radio_setup_read(void);
bool mesh_radio_setup_region(uint32_t region, uint32_t revision);
bool mesh_radio_setup_names(const char *name, const char *short_name,
                            uint32_t revision);
// Fresh read-only verification of both reviewed values before marking complete.
bool mesh_radio_setup_confirm(uint32_t region, const char *name,
                              const char *short_name, uint32_t revision);
