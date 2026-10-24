#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MESH_RANGE_RECORDS 512
#define MESH_RANGE_WINDOW 2048
// Native Meshtastic RANGE_TEST_APP payload: ASCII "seq <uint32>".
typedef enum {
  MESH_RANGE_NEW,
  MESH_RANGE_LATE,
  MESH_RANGE_DUPLICATE,
  MESH_RANGE_SEGMENT,
  MESH_RANGE_OUTSIDE
} mesh_range_event_t;
typedef struct {
  uint32_t first, highest, segment, received, gaps, duplicates, late, outside;
  uint8_t seen[MESH_RANGE_WINDOW / 8];
  bool initialized;
} mesh_range_counter_t;
bool mesh_range_sequence(const uint8_t *data, size_t size, uint32_t *out);
mesh_range_event_t mesh_range_count(mesh_range_counter_t *counter,
                                    uint32_t sequence, bool duplicate_packet);
const char *mesh_range_event_name(unsigned event);
typedef struct {
  uint32_t sequence, packet, time, peer, segment, gaps;
  float snr;
  int32_t rssi;
  int32_t rx_lat, rx_lon, tx_lat, tx_lon;
  uint32_t rx_fix, tx_fix;
  uint8_t rx_precision, tx_precision, event, channel;
  bool has_snr, has_rssi, has_rx_position, has_tx_position;
} mesh_range_record_t;
typedef enum { MESH_RANGE_RECEIVE, MESH_RANGE_SEND } mesh_range_mode_t;
typedef enum {
  MESH_RANGE_STOPPED,
  MESH_RANGE_ACTIVE,
  MESH_RANGE_FINISHED,
  MESH_RANGE_INTERRUPTED
} mesh_range_state_t;
typedef struct {
  mesh_range_state_t state;
  mesh_range_mode_t mode;
  uint32_t origin, peer;
  uint8_t channel;
  uint32_t interval, duration, elapsed, queued, write_failures, skipped,
      sequence, records_overwritten;
  mesh_range_counter_t counter;
  mesh_range_record_t last;
  size_t records;
  bool has_last;
} mesh_range_status_t;
typedef struct {
  bool loaded, pending, enabled;
  uint32_t sender, error;
} mesh_range_native_t;
mesh_range_native_t mesh_radio_range_native(void);
bool mesh_radio_range_refresh(void);
// Starting clears previous session results; never writes radio module settings.
bool mesh_radio_range_start(mesh_range_mode_t mode, uint8_t channel,
                            uint32_t peer, uint32_t interval,
                            uint32_t duration);
void mesh_radio_range_stop(void);
mesh_range_status_t mesh_radio_range_status(void);
size_t mesh_radio_range_snapshot(mesh_range_record_t *records, size_t capacity,
                                 mesh_range_status_t *status);
