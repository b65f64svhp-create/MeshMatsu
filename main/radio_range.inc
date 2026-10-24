#include "range_counter.inc"
static mesh_range_status_t range_status;
static EXT_RAM_BSS_ATTR mesh_range_record_t range_records[MESH_RANGE_RECORDS];
static size_t range_record_start;
static uint32_t range_packet_ids[64];
static size_t range_id_next;
static int64_t range_started, range_next;
static mesh_range_native_t range_native;
static uint32_t range_native_id, range_native_origin;
static int64_t range_native_requested, range_native_loaded;
static bool range_position(uint32_t number, mesh_node_t *out) {
  return mesh_radio_get_node_by_number(number, out) &&
         out->position_available && out->latitude_i >= -900000000 &&
         out->latitude_i <= 900000000 && out->longitude_i >= -1800000000 &&
         out->longitude_i <= 1800000000;
}
static void range_log(mesh_range_record_t *row) {
  mesh_node_t node;
  row->time = current_message_timestamp();
  row->channel = range_status.channel;
  if (range_position(range_status.origin, &node)) {
    row->has_rx_position = true;
    row->rx_lat = node.latitude_i;
    row->rx_lon = node.longitude_i;
    row->rx_fix = node.position_time;
    row->rx_precision = node.position_precision;
  }
  if (range_status.mode == MESH_RANGE_RECEIVE &&
      range_position(row->peer, &node)) {
    row->has_tx_position = true;
    row->tx_lat = node.latitude_i;
    row->tx_lon = node.longitude_i;
    row->tx_fix = node.position_time;
    row->tx_precision = node.position_precision;
  }
  size_t at;
  if (range_status.records < MESH_RANGE_RECORDS) {
    at = (range_record_start + range_status.records) % MESH_RANGE_RECORDS;
    range_status.records++;
  } else {
    at = range_record_start;
    range_record_start = (range_record_start + 1) % MESH_RANGE_RECORDS;
    range_status.records_overwritten++;
  }
  range_records[at] = *row;
  range_status.last = *row;
  range_status.has_last = true;
  history_revision++;
}
static void range_receive_packet(const uint8_t *data, size_t size,
                                 uint32_t sender, uint32_t destination,
                                 uint32_t id, uint32_t channel, bool mqtt,
                                 uint32_t hop_start, uint32_t hop_limit,
                                 bool has_snr, float snr, bool has_rssi,
                                 int32_t rssi) {
  if (range_status.state != MESH_RANGE_ACTIVE ||
      range_status.mode != MESH_RANGE_RECEIVE || sender == local_node_number ||
      !sender || sender == MESH_BROADCAST_NODE ||
      destination != MESH_BROADCAST_NODE || channel != range_status.channel ||
      mqtt || hop_start || hop_limit ||
      range_status.origin != local_node_number)
    return;
  mesh_node_t node;
  if (mesh_radio_get_node_by_number(sender, &node) && node.is_ignored)
    return;
  protobuf_field_t port, payload;
  if (!manage_field(data, size, 1, &port) || port.wire != 0 ||
      port.integer != 66 || !manage_field(data, size, 2, &payload) ||
      payload.wire != 2)
    return;
  uint32_t seq;
  if (!mesh_range_sequence(payload.value, payload.length, &seq))
    return;
  if (range_status.peer && range_status.peer != sender)
    return;
  range_status.peer = sender;
  bool duplicate = false;
  for (unsigned i = 0; id && i < 64; i++)
    if (range_packet_ids[i] == id)
      duplicate = true;
  if (id && !duplicate) {
    range_packet_ids[range_id_next++ % 64] = id;
  }
  mesh_range_record_t row = {.peer = sender,
                             .sequence = seq,
                             .packet = id,
                             .has_snr = has_snr && isfinite(snr),
                             .snr = snr,
                             .has_rssi = has_rssi,
                             .rssi = rssi};
  row.event = mesh_range_count(&range_status.counter, seq, duplicate);
  row.segment = range_status.counter.segment;
  row.gaps = range_status.counter.gaps;
  range_log(&row);
}
mesh_range_native_t mesh_radio_range_native(void) { return range_native; }
bool mesh_radio_range_refresh(void) {
  if (!status.started || !status.synchronized || status.reconnecting ||
      !local_node_number || config_request_pending || range_native.pending)
    return false;
  const uint8_t request[] = {0x38, 4};
  if (!send_admin_payload_with_id(request, sizeof(request), &range_native_id))
    return false;
  range_native = (mesh_range_native_t){.pending = true};
  range_native_origin = local_node_number;
  range_native_requested = esp_timer_get_time();
  return true;
}
static void range_module_response(const uint8_t *data, size_t size,
                                  uint32_t id) {
  if (!range_native.pending || !id || id != range_native_id ||
      local_node_number != range_native_origin)
    return;
  protobuf_field_t raw;
  if (!manage_field(data, size, 5, &raw) || raw.wire != 2 || raw.length > 192)
    return;
  size_t p = 0;
  unsigned seen = 0;
  mesh_range_native_t result = {.loaded = true};
  while (p < raw.length) {
    protobuf_field_t f;
    if (!read_field(raw.value, raw.length, &p, &f))
      return;
    if (f.field <= 4) {
      if (f.wire != 0 || (seen & (1u << f.field)) ||
          f.integer > (f.field == 2 ? UINT32_MAX : 1))
        return;
      seen |= 1u << f.field;
      if (f.field == 1)
        result.enabled = f.integer;
      else if (f.field == 2)
        result.sender = f.integer;
    }
  }
  // ModuleConfig is a oneof; reject conflicting variants.
  p = 0;
  while (p < size) {
    protobuf_field_t f;
    if (!read_field(data, size, &p, &f) || f.field != 5)
      return;
  }
  range_native = result;
  range_native_loaded = esp_timer_get_time();
  range_native_id = 0;
  history_revision++;
}
static void range_error(uint32_t sender, uint32_t destination, uint32_t id,
                        uint32_t error) {
  if (range_native.pending && id == range_native_id &&
      sender == range_native_origin &&
      (destination == sender || !destination) && error) {
    range_native = (mesh_range_native_t){.error = error};
    range_native_id = 0;
    history_revision++;
  }
}
bool mesh_radio_range_start(mesh_range_mode_t mode, uint8_t channel,
                            uint32_t peer, uint32_t interval,
                            uint32_t duration) {
  if (range_status.state == MESH_RANGE_ACTIVE || mode > MESH_RANGE_SEND ||
      channel >= 8 || !channels[channel].info.loaded ||
      !channels[channel].info.role || !status.started || !status.synchronized ||
      status.reconnecting || !local_node_number ||
      peer == MESH_BROADCAST_NODE || peer == local_node_number ||
      (duration != 300 && duration != 900 && duration != 1800 &&
       duration != 3600) ||
      (interval != 30 && interval != 60 && interval != 120 && interval != 300))
    return false;
  int64_t now = esp_timer_get_time();
  if (mode == MESH_RANGE_SEND &&
      (!range_native.loaded || range_native_origin != local_node_number ||
       now - range_native_loaded >= 60000000 ||
       (range_native.enabled && range_native.sender)))
    return false;
  range_status =
      (mesh_range_status_t){.state = MESH_RANGE_ACTIVE,
                            .mode = mode,
                            .origin = local_node_number,
                            .peer = mode == MESH_RANGE_RECEIVE ? peer : 0,
                            .channel = channel,
                            .interval = interval,
                            .duration = duration};
  range_record_start = range_id_next = 0;
  memset(range_packet_ids, 0, sizeof(range_packet_ids));
  range_started = now;
  range_next = now + 5000000;
  history_revision++;
  return true;
}
void mesh_radio_range_stop(void) {
  if (range_status.state == MESH_RANGE_ACTIVE) {
    range_status.state = MESH_RANGE_STOPPED;
    history_revision++;
  }
}
mesh_range_status_t mesh_radio_range_status(void) {
  mesh_range_status_t result = range_status;
  if (result.state == MESH_RANGE_ACTIVE) {
    int64_t now = esp_timer_get_time();
    result.elapsed = (now - range_started) / 1000000;
  }
  return result;
}
size_t mesh_radio_range_snapshot(mesh_range_record_t *out, size_t capacity,
                                 mesh_range_status_t *state) {
  if (state)
    *state = mesh_radio_range_status();
  size_t n = range_status.records;
  if (n > capacity)
    n = capacity;
  if (out)
    for (size_t i = 0; i < n; i++)
      out[i] = range_records[(range_record_start + i) % MESH_RANGE_RECORDS];
  return n;
}
static bool range_send(uint32_t sequence, uint32_t *id) {
  uint8_t data[48], packet[96], frame[128];
  size_t d = 0, p = 0, n = 4;
  char text[16];
  int length = snprintf(text, sizeof(text), "seq %lu", (unsigned long)sequence);
  if (!append_varint(data, sizeof(data), &d, 8) ||
      !append_varint(data, sizeof(data), &d, 66) ||
      !append_bytes_field(data, sizeof(data), &d, 2, (const uint8_t *)text,
                          length))
    return false;
  packet[p++] = 0x15;
  uint32_t to = MESH_BROADCAST_NODE;
  memcpy(packet + p, &to, 4);
  p += 4;
  packet[p++] = 0x18;
  packet[p++] = range_status.channel;
  if (!append_bytes_field(packet, sizeof(packet), &p, 4, data, d))
    return false;
  *id = next_packet_id++;
  if (!*id)
    *id = next_packet_id++;
  packet[p++] = 0x35;
  memcpy(packet + p, id, 4);
  p += 4;
  packet[p++] = 0x48;
  packet[p++] = 0; // hop_limit=0: never relay.
  packet[p++] = 0x50;
  packet[p++] = 0; // want_ack=false; no reliable resend cycle.
  packet[p++] = 0x58;
  packet[p++] = 10; // BACKGROUND priority.
  if (!append_bytes_field(frame, sizeof(frame), &n, 1, packet, p))
    return false;
  size_t body = n - 4;
  frame[0] = 0x94;
  frame[1] = 0xc3;
  frame[2] = body >> 8;
  frame[3] = body;
  return uart_write_bytes(RADIO_UART, frame, n) == (int)n;
}
static void range_poll(int64_t now) {
  if (range_native.pending && now - range_native_requested >= 10000000) {
    range_native = (mesh_range_native_t){.error = UINT32_MAX};
    range_native_id = 0;
    history_revision++;
  }
  if (range_native_origin && local_node_number &&
      range_native_origin != local_node_number) {
    range_native = (mesh_range_native_t){0};
    range_native_id = 0;
  }
  if (range_status.state != MESH_RANGE_ACTIVE)
    return;
  range_status.elapsed = (now - range_started) / 1000000;
  if (local_node_number && local_node_number != range_status.origin) {
    range_status.state = MESH_RANGE_INTERRUPTED;
    history_revision++;
    return;
  }
  if (range_status.elapsed >= range_status.duration) {
    range_status.state = MESH_RANGE_FINISHED;
    history_revision++;
    return;
  }
  if (range_status.mode != MESH_RANGE_SEND || now < range_next)
    return;
  range_next =
      now + (int64_t)range_status.interval *
                1000000; // No catch-up bursts/replays after slow polling.
  mesh_range_record_t row = {.peer = MESH_BROADCAST_NODE};
  if (!status.started || !status.synchronized || status.reconnecting) {
    range_status.skipped++;
    row.event = 8;
    range_log(&row);
    return;
  }
  mesh_device_metrics_t activity;
  if (mesh_radio_get_local_activity(&activity) && activity.has_channel_use &&
      !activity.cached && now >= activity.received_us &&
      now - activity.received_us < 300000000 && activity.channel_use >= 25) {
    range_status.skipped++;
    row.event = 6;
    range_log(&row);
    return;
  }
  // Consume a sequence even after an ambiguous partial UART write; never retry
  // that attempt.
  row.sequence = ++range_status.sequence;
  bool ok = range_send(row.sequence, &row.packet);
  if (ok) {
    range_status.queued++;
    row.event = 5;
  } else {
    range_status.write_failures++;
    row.event = 7;
  }
  range_log(&row);
}
