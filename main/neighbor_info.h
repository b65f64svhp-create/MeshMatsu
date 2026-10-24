#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MESH_NEIGHBORS 10
#define MESH_NEIGHBOR_REPORTS 32
// These are edges reported by the origin, never inferred from a relayed packet.
typedef struct {
  uint32_t node, last_rx, interval;
  float snr;
  bool has_snr;
} mesh_neighbor_t;
typedef struct {
  uint32_t node, packet, timestamp, interval, last_sent_by;
  int64_t received_us;
  size_t count;
  bool via_mqtt;
  mesh_neighbor_t neighbors[MESH_NEIGHBORS];
} mesh_neighbor_report_t;
typedef enum {
  MESH_NB_IDLE,
  MESH_NB_WAITING,
  MESH_NB_REPLY,
  MESH_NB_TIMEOUT,
  MESH_NB_FAILED,
  MESH_NB_INTERRUPTED
} mesh_neighbor_state_t;
typedef struct {
  mesh_neighbor_state_t state;
  uint32_t node, origin, id, error;
  unsigned elapsed;
} mesh_neighbor_request_t;
typedef enum {
  MESH_NB_START_OK,
  MESH_NB_START_INVALID,
  MESH_NB_START_OFFLINE,
  MESH_NB_START_BUSY,
  MESH_NB_START_COOLDOWN,
  MESH_NB_START_WRITE_FAILED
} mesh_neighbor_start_t;
bool mesh_radio_get_neighbors(uint32_t node, mesh_neighbor_report_t *out);
mesh_neighbor_start_t mesh_radio_request_neighbors(uint32_t node);
mesh_neighbor_request_t mesh_radio_neighbor_request(void);
unsigned mesh_radio_neighbor_cooldown(void);
typedef enum {
  MESH_NBC_IDLE,
  MESH_NBC_READING,
  MESH_NBC_READY,
  MESH_NBC_CHECKING,
  MESH_NBC_SAVING,
  MESH_NBC_VERIFYING,
  MESH_NBC_SAVED,
  MESH_NBC_FAILED,
  MESH_NBC_TIMEOUT,
  MESH_NBC_INTERRUPTED,
  MESH_NBC_CHANGED
} mesh_neighbor_config_state_t;
typedef struct {
  bool loaded, enabled, over_lora;
  uint32_t interval, revision, origin;
  mesh_neighbor_config_state_t state;
  uint32_t error;
} mesh_neighbor_config_t;
mesh_neighbor_config_t mesh_radio_neighbor_config(void);
bool mesh_radio_neighbor_config_read(void);
bool mesh_radio_neighbor_config_save(const mesh_neighbor_config_t *desired);
bool mesh_radio_neighbor_config_busy(void);
bool mesh_radio_neighbor_lora_allowed(void);
