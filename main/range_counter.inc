#include "range_test.h"
#include <limits.h>
#include <string.h>
static void range_increment(uint32_t *v) {
  if (*v < UINT32_MAX)
    (*v)++;
}
static void range_add(uint32_t *v, uint32_t n) {
  *v = UINT32_MAX - *v < n ? UINT32_MAX : *v + n;
}
bool mesh_range_sequence(const uint8_t *data, size_t n, uint32_t *out) {
  if (!data || !out || n < 5 || n > 14 || memcmp(data, "seq ", 4))
    return false;
  uint32_t v = 0;
  for (size_t i = 4; i < n; i++) {
    if (data[i] < '0' || data[i] > '9')
      return false;
    unsigned d = data[i] - '0';
    if (v > (UINT32_MAX - d) / 10)
      return false;
    v = v * 10 + d;
  }
  *out = v;
  return true;
}
static void range_segment(mesh_range_counter_t *c, uint32_t sequence) {
  memset(c->seen, 0, sizeof(c->seen));
  c->seen[0] = 1;
  c->first = c->highest = sequence;
  c->initialized = true;
  range_increment(&c->segment);
  range_increment(&c->received);
}
mesh_range_event_t mesh_range_count(mesh_range_counter_t *c, uint32_t sequence,
                                    bool duplicate) {
  if (duplicate) {
    range_increment(&c->duplicates);
    return MESH_RANGE_DUPLICATE;
  }
  if (!c->initialized) {
    range_segment(c, sequence);
    return MESH_RANGE_NEW;
  }
  uint32_t forward = sequence - c->highest;
  if (forward && forward < 0x80000000U) {
    if (forward >= MESH_RANGE_WINDOW) {
      range_segment(c, sequence);
      return MESH_RANGE_SEGMENT;
    }
    for (int i = MESH_RANGE_WINDOW - 1; i >= 0; i--) {
      bool bit = i >= (int)forward &&
                 (c->seen[(i - forward) / 8] & (1u << ((i - forward) % 8)));
      if (bit)
        c->seen[i / 8] |= 1u << (i % 8);
      else
        c->seen[i / 8] &= ~(1u << (i % 8));
    }
    c->seen[0] |= 1;
    c->highest = sequence;
    range_add(&c->gaps, forward - 1);
    range_increment(&c->received);
    return MESH_RANGE_NEW;
  }
  uint32_t behind = c->highest - sequence;
  // A fresh packet carrying seq 1 is a possible sender restart, not thousands
  // of lost packets.
  if (sequence == 1 && c->highest > 1) {
    range_segment(c, sequence);
    return MESH_RANGE_SEGMENT;
  }
  if (behind >= MESH_RANGE_WINDOW || behind > c->highest - c->first) {
    range_increment(&c->outside);
    return MESH_RANGE_OUTSIDE;
  }
  if (c->seen[behind / 8] & (1u << (behind % 8))) {
    range_increment(&c->duplicates);
    return MESH_RANGE_DUPLICATE;
  }
  c->seen[behind / 8] |= 1u << (behind % 8);
  if (c->gaps)
    c->gaps--;
  range_increment(&c->received);
  range_increment(&c->late);
  return MESH_RANGE_LATE;
}
const char *mesh_range_event_name(unsigned event) {
  const char *names[] = {"new",          "late",           "duplicate",
                         "new_segment",  "outside_window", "queued_to_radio",
                         "channel_busy", "uart_failed",    "radio_offline"};
  return event < 9 ? names[event] : "unknown";
}
