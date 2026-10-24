// Meshtastic 2.7.18: NEIGHBORINFO_APP=71; NeighborInfo.node_id=1,
// last_sent_by_id=2, interval=3, repeated Neighbor=4. Never infer an edge
// from packet RSSI/SNR or last_sent_by_id: those can describe a relay instead.
static EXT_RAM_BSS_ATTR mesh_neighbor_report_t
    nb_reports[MESH_NEIGHBOR_REPORTS];
static mesh_neighbor_request_t nb_request;
static uint32_t nb_origin;
static bool nb_origin_changed(void) { return nb_origin != local_node_number; }
static int64_t nb_requested, nb_last_query;
static bool nb_query_sent;
static mesh_neighbor_config_t nb_config;
static uint8_t nb_raw[180], nb_wanted[180];
static size_t nb_raw_size, nb_wanted_size;
static struct {
  uint32_t origin, id, write_id;
  int64_t started, read_at, loaded;
  mesh_neighbor_config_t desired;
  uint8_t primary;
  bool check_channel;
} nb_op;
static bool nb_node(uint32_t id) { return id && id != MESH_BROADCAST_NODE; }
static bool nb_parse_neighbor(const uint8_t *bytes, size_t n,
                              mesh_neighbor_t *out) {
  memset(out, 0, sizeof(*out));
  size_t p = 0;
  unsigned seen = 0;
  while (p < n) {
    protobuf_field_t f;
    if (!read_field(bytes, n, &p, &f))
      return false;
    if (f.field <= 4) {
      if (!f.field || (seen & (1u << f.field)))
        return false;
      seen |= 1u << f.field;
      if (f.field == 2) {
        if (f.wire != 5 || f.length != 4)
          return false;
        memcpy(&out->snr, f.value, 4);
        if (!isfinite(out->snr) || out->snr < -200 || out->snr > 200)
          return false;
        out->has_snr = true;
      } else if (f.field == 3) {
        if (f.wire != 5 || f.length != 4)
          return false;
        memcpy(&out->last_rx, f.value, 4);
      } else {
        if (f.wire != 0 || f.integer > UINT32_MAX)
          return false;
        if (f.field == 1)
          out->node = f.integer;
        else
          out->interval = f.integer;
      }
    }
  }
  return nb_node(out->node);
}
static bool nb_parse_report(const uint8_t *bytes, size_t n,
                            mesh_neighbor_report_t *out) {
  size_t p = 0;
  unsigned seen = 0;
  while (p < n) {
    protobuf_field_t f;
    if (!read_field(bytes, n, &p, &f))
      return false;
    if (f.field >= 1 && f.field <= 3) {
      if (f.wire != 0 || f.integer > UINT32_MAX || (seen & (1u << f.field)))
        return false;
      seen |= 1u << f.field;
      if (f.field == 1)
        out->node = f.integer;
      else if (f.field == 2)
        out->last_sent_by = f.integer;
      else
        out->interval = f.integer;
    } else if (f.field == 4) {
      if (f.wire != 2 || out->count == MESH_NEIGHBORS ||
          !nb_parse_neighbor(f.value, f.length, &out->neighbors[out->count]))
        return false;
      for (size_t i = 0; i < out->count; i++)
        if (out->neighbors[i].node == out->neighbors[out->count].node)
          return false;
      out->count++;
    }
  }
  if (!nb_node(out->node))
    return false;
  for (size_t i = 0; i < out->count; i++)
    if (out->neighbors[i].node == out->node)
      return false;
  return true;
}
static void neighbor_receive(const uint8_t *data, size_t n, uint32_t sender,
                             uint32_t to, uint32_t id, uint32_t timestamp,
                             bool mqtt, uint32_t channel) {
  if (!nb_node(sender) || channel >= 8 ||
      (to != local_node_number && to != MESH_BROADCAST_NODE &&
       !(sender == local_node_number && !to)))
    return;
  mesh_node_t node;
  if (mesh_radio_get_node_by_number(sender, &node) && node.is_ignored)
    return;
  protobuf_field_t port, payload;
  if (!manage_field(data, n, 1, &port) || port.wire != 0 ||
      port.integer != 71 || !manage_field(data, n, 2, &payload) ||
      payload.wire != 2)
    return;
  mesh_neighbor_report_t report = {.packet = id,
                                   .timestamp = timestamp,
                                   .received_us = esp_timer_get_time(),
                                   .via_mqtt = mqtt};
  // The request dummy has node 0; rejected before it can overwrite real data.
  if (!nb_parse_report(payload.value, payload.length, &report) ||
      report.node != sender)
    return;
  uint32_t reply = 0;
  size_t check = 0;
  bool request_seen = false;
  while (check < n) {
    protobuf_field_t f;
    if (!read_field(data, n, &check, &f))
      return;
    if (f.field == 6) {
      if (request_seen || f.wire != 5 || f.length != 4)
        return;
      request_seen = true;
      memcpy(&reply, f.value, 4);
    }
  }
  if (nb_request.state == MESH_NB_WAITING && nb_request.node == sender &&
      nb_request.origin == local_node_number && to == local_node_number &&
      reply == nb_request.id) {
    nb_request.state = MESH_NB_REPLY;
    history_revision++;
  }
  size_t at = MESH_NEIGHBOR_REPORTS;
  int64_t oldest = INT64_MAX;
  for (size_t i = 0; i < MESH_NEIGHBOR_REPORTS; i++)
    if (nb_reports[i].node == sender) {
      at = i;
      break;
    }
  if (at < MESH_NEIGHBOR_REPORTS) {
    if (id && nb_reports[at].packet == id)
      return;
    if (timestamp >= VALID_UNIX_TIME &&
        nb_reports[at].timestamp >= VALID_UNIX_TIME &&
        timestamp < nb_reports[at].timestamp)
      return;
  } else
    for (size_t i = 0; i < MESH_NEIGHBOR_REPORTS; i++) {
      if (!nb_reports[i].node) {
        at = i;
        break;
      }
      if (nb_reports[i].node != local_node_number &&
          nb_reports[i].received_us < oldest) {
        oldest = nb_reports[i].received_us;
        at = i;
      }
    }
  if (at < MESH_NEIGHBOR_REPORTS) {
    nb_reports[at] = report;
    history_revision++;
  }
}
bool mesh_radio_get_neighbors(uint32_t node, mesh_neighbor_report_t *out) {
  if (!out)
    return false;
  memset(out, 0, sizeof(*out));
  for (size_t i = 0; i < MESH_NEIGHBOR_REPORTS; i++)
    if (nb_reports[i].node == node && node) {
      *out = nb_reports[i];
      return true;
    }
  return false;
}
mesh_neighbor_request_t mesh_radio_neighbor_request(void) {
  mesh_neighbor_request_t r = nb_request;
  int64_t elapsed = esp_timer_get_time() - nb_requested;
  r.elapsed = elapsed > 0 ? (unsigned)(elapsed / 1000000) : 0;
  return r;
}
unsigned mesh_radio_neighbor_cooldown(void) {
  int64_t elapsed = esp_timer_get_time() - nb_last_query;
  if (!nb_query_sent || elapsed >= 180000000)
    return 0;
  return elapsed < 0 ? 180
                     : (unsigned)((180000000 - elapsed + 999999) / 1000000);
}
mesh_neighbor_start_t mesh_radio_request_neighbors(uint32_t node) {
  mesh_node_t target;
  if (!nb_node(node) ||
      (node != local_node_number &&
       (!mesh_radio_get_node_by_number(node, &target) || target.is_ignored)))
    return MESH_NB_START_INVALID;
  if (!status.started || !status.synchronized || status.reconnecting ||
      !local_node_number || config_request_pending)
    return MESH_NB_START_OFFLINE;
  if (nb_request.state == MESH_NB_WAITING)
    return MESH_NB_START_BUSY;
  if (mesh_radio_neighbor_cooldown())
    return MESH_NB_START_COOLDOWN;
  // One explicit dummy Neighbor, not a fabricated empty neighbor list. The
  // firmware recognizes node 0/SNR 0 as a request and never stores it as data.
  uint8_t payload[20], data[40];
  size_t p = 0, d = 0;
  if (!append_varint(payload, sizeof(payload), &p, 8) ||
      !append_varint(payload, sizeof(payload), &p, local_node_number) ||
      !append_bytes_field(payload, sizeof(payload), &p, 4, payload, 0) ||
      !append_varint(data, sizeof(data), &d, 8) ||
      !append_varint(data, sizeof(data), &d, 71) ||
      !append_bytes_field(data, sizeof(data), &d, 2, payload, p) ||
      !append_varint(data, sizeof(data), &d, 24) ||
      !append_varint(data, sizeof(data), &d, 1))
    return MESH_NB_START_WRITE_FAILED;
  uint32_t id;
  uint8_t hops;
  if (!transmit_chat_data(node, 0, data, d, &id, &hops))
    return MESH_NB_START_WRITE_FAILED;
  nb_request = (mesh_neighbor_request_t){.state = MESH_NB_WAITING,
                                         .node = node,
                                         .origin = local_node_number,
                                         .id = id};
  nb_requested = nb_last_query = esp_timer_get_time();
  nb_query_sent = true;
  history_revision++;
  return MESH_NB_START_OK;
}
bool mesh_radio_neighbor_config_busy(void) {
  return nb_config.state == MESH_NBC_CHECKING ||
         nb_config.state == MESH_NBC_SAVING ||
         nb_config.state == MESH_NBC_VERIFYING;
}
mesh_neighbor_config_t mesh_radio_neighbor_config(void) { return nb_config; }
bool mesh_radio_neighbor_lora_allowed(void) {
  // Conservative: require a known primary channel with a full private key.
  // Firmware may also suppress sends for congestion/default-frequency rules.
  for (size_t i = 0; i < 8; i++)
    if (channels[i].info.loaded && channels[i].info.role == 1)
      return channels[i].info.has_key && !channels[i].info.default_key;
  return false;
}
static bool nb_parse_config(const uint8_t *data, size_t n,
                            mesh_neighbor_config_t *out, const uint8_t **raw,
                            size_t *length) {
  size_t p = 0;
  protobuf_field_t body;
  bool found = false;
  while (p < n) {
    protobuf_field_t f;
    if (!read_field(data, n, &p, &f) || f.field != 10 || f.wire != 2 || found ||
        f.length > sizeof(nb_raw))
      return false;
    body = f;
    found = true;
  }
  if (!found)
    return false;
  mesh_neighbor_config_t parsed = {.loaded = true, .origin = local_node_number};
  p = 0;
  unsigned seen = 0;
  while (p < body.length) {
    protobuf_field_t f;
    if (!read_field(body.value, body.length, &p, &f))
      return false;
    if (f.field >= 1 && f.field <= 3) {
      if (f.wire != 0 || (seen & (1u << f.field)) ||
          f.integer > (f.field == 2 ? UINT32_MAX : 1))
        return false;
      seen |= 1u << f.field;
      if (f.field == 1)
        parsed.enabled = f.integer;
      else if (f.field == 2)
        parsed.interval = f.integer;
      else
        parsed.over_lora = f.integer;
    }
  }
  *out = parsed;
  *raw = body.value;
  *length = body.length;
  return true;
}
static bool nb_get_config(uint32_t *id) {
  const uint8_t request[] = {0x38, 9};
  return send_admin_payload_with_id(request, sizeof(request), id);
}
bool mesh_radio_neighbor_config_read(void) {
  if (!status.started || !status.synchronized || status.reconnecting ||
      config_request_pending || !local_node_number ||
      mesh_radio_neighbor_config_busy() || nb_config.state == MESH_NBC_READING)
    return false;
  uint32_t id;
  if (!nb_get_config(&id))
    return false;
  nb_op.origin = local_node_number;
  nb_op.id = id;
  nb_op.started = esp_timer_get_time();
  nb_config.state = MESH_NBC_READING;
  nb_config.error = 0;
  history_revision++;
  return true;
}
static bool nb_build_wanted(const mesh_neighbor_config_t *desired) {
  size_t p = 0, n = 0;
  while (p < nb_raw_size) {
    size_t start = p;
    protobuf_field_t f;
    if (!read_field(nb_raw, nb_raw_size, &p, &f))
      return false;
    if (f.field >= 1 && f.field <= 3)
      continue;
    if (p - start > sizeof(nb_wanted) - n)
      return false;
    memcpy(nb_wanted + n, nb_raw + start, p - start);
    n += p - start;
  }
  if (!append_varint(nb_wanted, sizeof(nb_wanted), &n, 8) ||
      !append_varint(nb_wanted, sizeof(nb_wanted), &n, desired->enabled) ||
      !append_varint(nb_wanted, sizeof(nb_wanted), &n, 16) ||
      !append_varint(nb_wanted, sizeof(nb_wanted), &n, desired->interval) ||
      !append_varint(nb_wanted, sizeof(nb_wanted), &n, 24) ||
      !append_varint(nb_wanted, sizeof(nb_wanted), &n, desired->over_lora))
    return false;
  nb_wanted_size = n;
  return true;
}
bool mesh_radio_neighbor_config_save(const mesh_neighbor_config_t *desired) {
  unsigned major = 0, minor = 0;
  if (!desired || !nb_config.loaded || desired->origin != local_node_number ||
      nb_config.origin != local_node_number ||
      desired->revision != nb_config.revision || !radio_metadata.loaded ||
      sscanf(radio_metadata.firmware_version, "%u.%u", &major, &minor) != 2 ||
      major != 2 || minor != 7 ||
      (desired->interval && desired->interval < 14400) ||
      !channel_management_ready() || manage_busy(remote.state) ||
      mesh_radio_neighbor_config_busy() ||
      nb_config.state == MESH_NBC_READING ||
      nb_op.origin != local_node_number ||
      esp_timer_get_time() - nb_op.loaded >= 60000000 ||
      (desired->over_lora &&
       (!desired->enabled || !mesh_radio_neighbor_lora_allowed())))
    return false;
  if (!nb_build_wanted(desired))
    return false;
  uint32_t id;
  nb_op.check_channel = desired->over_lora;
  if (nb_op.check_channel) {
    for (uint8_t i = 0; i < 8; i++)
      if (channels[i].info.loaded && channels[i].info.role == 1) {
        nb_op.primary = i;
        break;
      }
    const uint8_t request[] = {8, (uint8_t)(nb_op.primary + 1)};
    if (!send_admin_payload_with_id(request, sizeof(request), &id))
      return false;
  } else if (!nb_get_config(&id))
    return false;
  nb_op.origin = local_node_number;
  nb_op.id = id;
  nb_op.started = esp_timer_get_time();
  nb_op.desired = *desired;
  nb_config.state = MESH_NBC_CHECKING;
  nb_config.error = 0;
  history_revision++;
  return true;
}
static void neighbor_module_dump(const uint8_t *data, size_t n) {
  if (mesh_radio_neighbor_config_busy() || nb_config.state == MESH_NBC_READING)
    return;
  mesh_neighbor_config_t parsed;
  const uint8_t *raw;
  size_t length;
  if (!nb_parse_config(data, n, &parsed, &raw, &length))
    return;
  parsed.revision = nb_config.revision + 1;
  parsed.state = MESH_NBC_READY;
  nb_config = parsed;
  memcpy(nb_raw, raw, length);
  nb_raw_size = length;
  nb_op.loaded = esp_timer_get_time();
  nb_op.origin = local_node_number;
  history_revision++;
}
static void neighbor_admin_response(const uint8_t *data, size_t n,
                                    uint32_t id) {
  if (!id || id != nb_op.id || nb_op.origin != local_node_number ||
      !status.synchronized || status.reconnecting || config_request_pending)
    return;
  mesh_neighbor_config_state_t state = nb_config.state;
  if (state != MESH_NBC_READING && state != MESH_NBC_CHECKING &&
      state != MESH_NBC_VERIFYING)
    return;
  if (state == MESH_NBC_CHECKING && nb_op.check_channel) {
    protobuf_field_t channel, settings, role;
    mesh_channel_t fresh;
    if (!manage_field(data, n, 2, &channel) || channel.wire != 2 ||
        !manage_field(channel.value, channel.length, 2, &settings) ||
        settings.wire != 2 ||
        !manage_field(channel.value, channel.length, 3, &role) ||
        role.wire != 0)
      return;
    uint32_t slot = 0;
    bool index_seen = false;
    size_t offset = 0;
    while (offset < channel.length) {
      protobuf_field_t f;
      if (!read_field(channel.value, channel.length, &offset, &f))
        return;
      if (f.field == 1) {
        if (index_seen || f.wire != 0 || f.integer >= 8)
          return;
        index_seen = true;
        slot = f.integer;
      }
    }
    if (slot != nb_op.primary || role.integer != 1 ||
        !channel_settings_info(settings.value, settings.length, &fresh) ||
        !fresh.has_key || fresh.default_key) {
      nb_config.state = MESH_NBC_CHANGED;
      nb_op.id = 0;
      history_revision++;
      return;
    }
    nb_op.check_channel = false;
    if (!nb_get_config(&nb_op.id))
      nb_config.state = MESH_NBC_FAILED;
    history_revision++;
    return;
  }
  protobuf_field_t module, token;
  if (!manage_field(data, n, 8, &module) || module.wire != 2)
    return;
  mesh_neighbor_config_t parsed;
  const uint8_t *raw;
  size_t length;
  if (!nb_parse_config(module.value, module.length, &parsed, &raw, &length))
    return;
  if (state == MESH_NBC_CHECKING) {
    if (length != nb_raw_size || memcmp(raw, nb_raw, length) ||
        (nb_op.desired.over_lora && !mesh_radio_neighbor_lora_allowed())) {
      nb_config.state = MESH_NBC_CHANGED;
      nb_op.id = 0;
      history_revision++;
      return;
    }
    uint8_t body[192], admin[233];
    size_t b = 0, a = 0;
    if (!manage_field(data, n, 101, &token) || token.wire != 2 ||
        !token.length || token.length > 16 ||
        !append_bytes_field(body, sizeof(body), &b, 10, nb_wanted,
                            nb_wanted_size) ||
        !append_bytes_field(admin, sizeof(admin), &a, 35, body, b) ||
        !append_bytes_field(admin, sizeof(admin), &a, 101, token.value,
                            token.length) ||
        !send_admin_payload_with_id(admin, a, &nb_op.write_id)) {
      nb_config.state = MESH_NBC_FAILED;
    } else {
      nb_config.state = MESH_NBC_SAVING;
      nb_op.read_at = esp_timer_get_time() + 8000000;
    }
    nb_op.id = 0;
    history_revision++;
    return;
  }
  parsed.revision = nb_config.revision + 1;
  parsed.state = MESH_NBC_READY;
  if (state == MESH_NBC_VERIFYING) {
    // Unknown fields were copied into the mutation. Compare each reported
    // field as well as the user's values before claiming a verified save.
    bool same = parsed.enabled == nb_op.desired.enabled &&
                parsed.over_lora == nb_op.desired.over_lora &&
                (parsed.interval == nb_op.desired.interval ||
                 (!nb_op.desired.interval && parsed.interval == 21600));
    size_t p = 0;
    while (same && p < nb_raw_size) {
      protobuf_field_t f;
      if (!read_field(nb_raw, nb_raw_size, &p, &f)) {
        same = false;
        break;
      }
      if (f.field <= 3)
        continue;
      protobuf_field_t check;
      if (!manage_field(raw, length, f.field, &check) || check.wire != f.wire ||
          check.integer != f.integer || check.length != f.length ||
          (f.length && memcmp(check.value, f.value, f.length)))
        same = false;
    }
    parsed.state = same ? MESH_NBC_SAVED : MESH_NBC_FAILED;
  }
  nb_config = parsed;
  memcpy(nb_raw, raw, length);
  nb_raw_size = length;
  nb_op.loaded = esp_timer_get_time();
  nb_op.id = 0;
  history_revision++;
}
static void neighbor_error(uint32_t sender, uint32_t to, uint32_t id,
                           uint32_t error) {
  if (!error)
    return;
  if (nb_request.state == MESH_NB_WAITING && nb_request.id == id &&
      nb_request.origin == local_node_number &&
      (sender == local_node_number || sender == nb_request.node) &&
      (to == local_node_number || !to)) {
    nb_request.state = MESH_NB_FAILED;
    nb_request.error = error;
    history_revision++;
  }
  if ((mesh_radio_neighbor_config_busy() ||
       nb_config.state == MESH_NBC_READING) &&
      nb_op.origin == sender && sender == local_node_number &&
      (to == sender || !to) &&
      (id == nb_op.id ||
       (nb_config.state == MESH_NBC_SAVING && id == nb_op.write_id))) {
    // A local NO_RESPONSE during a restart does not prove a rejected write.
    // Keep waiting for correlated settings readback; never resend the mutation.
    if (error == 8 && (nb_config.state == MESH_NBC_SAVING ||
                       nb_config.state == MESH_NBC_VERIFYING))
      return;
    nb_config.state = MESH_NBC_FAILED;
    nb_config.error = error;
    history_revision++;
  }
}
static void neighbor_reconnect(void) {
  if (nb_request.state == MESH_NB_WAITING)
    nb_request.state = MESH_NB_INTERRUPTED;
  if (nb_config.state == MESH_NBC_SAVING ||
      nb_config.state == MESH_NBC_VERIFYING) {
    nb_config.state = MESH_NBC_SAVING;
    nb_op.id = 0;
    nb_op.read_at = esp_timer_get_time() + 8000000;
  } else if (nb_config.state == MESH_NBC_CHECKING ||
             nb_config.state == MESH_NBC_READING)
    nb_config.state = MESH_NBC_INTERRUPTED;
  nb_config.loaded = false;
  history_revision++;
}
static void neighbor_reset(void) {
  memset(nb_reports, 0, sizeof(nb_reports));
  nb_request = (mesh_neighbor_request_t){0};
  nb_config = (mesh_neighbor_config_t){0};
  memset(&nb_op, 0, sizeof(nb_op));
  nb_raw_size = nb_wanted_size = 0;
  nb_origin = local_node_number;
  nb_query_sent = false;
}
static void neighbor_poll(int64_t now) {
  if (nb_origin != local_node_number) {
    neighbor_reset();
    history_revision++;
  }
  if (nb_request.state == MESH_NB_WAITING && now - nb_requested >= 120000000) {
    nb_request.state = MESH_NB_TIMEOUT;
    history_revision++;
  }
  if (mesh_radio_neighbor_config_busy() ||
      nb_config.state == MESH_NBC_READING) {
    if (nb_op.origin != local_node_number) {
      nb_config.state = MESH_NBC_INTERRUPTED;
      history_revision++;
    } else if (now - nb_op.started >= 90000000) {
      nb_config.state = MESH_NBC_TIMEOUT;
      history_revision++;
    } else if ((nb_config.state == MESH_NBC_SAVING ||
                nb_config.state == MESH_NBC_VERIFYING) &&
               now >= nb_op.read_at && status.synchronized &&
               !status.reconnecting && !config_request_pending) {
      if (nb_get_config(&nb_op.id)) {
        nb_config.state = MESH_NBC_VERIFYING;
        nb_op.read_at = now + 5000000;
        history_revision++;
      } else {
        nb_config.state = MESH_NBC_FAILED;
        history_revision++;
      }
    }
  }
}
