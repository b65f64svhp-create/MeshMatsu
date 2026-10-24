// First-use local administration. Only a correlated radio readback confirms
// Save.
static mesh_setup_radio_t setup_radio;
static struct {
  enum { SETUP_LOAD, SETUP_REGION, SETUP_NAMES, SETUP_CONFIRM } kind;
  uint32_t origin, id, write_id, region;
  unsigned part;
  int64_t started, read_at, loaded_at;
  uint8_t raw[2][192], wanted[224];
  size_t size[2], wanted_size;
  char name[40], short_name[5];
} setup_op;

bool mesh_radio_setup_supported(void) {
  const char *v = radio_metadata.firmware_version;
  return radio_metadata.loaded && !strncmp(v, "2.7.18", 6) &&
         (!v[6] || v[6] == '.' || v[6] == '-');
}
bool mesh_radio_setup_busy(void) {
  return setup_radio.state == MESH_SETUP_READING ||
         setup_radio.state == MESH_SETUP_CHECKING ||
         setup_radio.state == MESH_SETUP_SAVING ||
         setup_radio.state == MESH_SETUP_VERIFYING;
}
mesh_setup_radio_t mesh_radio_setup(void) { return setup_radio; }
static bool setup_online(void) {
  return status.started && status.synchronized && !status.reconnecting &&
         !config_request_pending && local_node_number &&
         local_node_number != MESH_BROADCAST_NODE;
}
static void setup_end(mesh_setup_state_t state) {
  setup_radio.state = state;
  setup_op.id = 0;
  history_revision++;
}
static void setup_reset(void) {
  memset(&setup_radio, 0, sizeof(setup_radio));
  memset(&setup_op, 0, sizeof(setup_op));
}
static void setup_reconnect(void) {
  if (setup_radio.state == MESH_SETUP_SAVING ||
      setup_radio.state == MESH_SETUP_VERIFYING) {
    setup_radio.state = MESH_SETUP_SAVING;
    setup_op.id = 0;
    setup_op.read_at = esp_timer_get_time() + 8000000;
  } else if (mesh_radio_setup_busy())
    setup_end(MESH_SETUP_INTERRUPTED);
  setup_radio.loaded = false;
}
static bool setup_get(void) {
  const uint8_t owner[] = {0x18, 1}, lora[] = {0x28, 5};
  if (!send_admin_payload_with_id(setup_op.part ? lora : owner, 2,
                                  &setup_op.id))
    return false;
  setup_op.read_at = esp_timer_get_time() + 5000000;
  return true;
}
static bool setup_start(bool writing) {
  return setup_online() && mesh_radio_setup_supported() &&
         !mesh_radio_setup_busy() && channel_management_ready() &&
         !manage_busy(remote.state) && !manage_busy(security_state.state) &&
         cfg_state != CFG_READING && keys_state != MESH_KEYS_READING &&
         (!writing ||
          (setup_radio.loaded && setup_radio.node == local_node_number &&
           setup_op.origin == local_node_number &&
           esp_timer_get_time() - setup_op.loaded_at < 60000000));
}
bool mesh_radio_setup_read(void) {
  if (!setup_start(false))
    return false;
  uint32_t revision = setup_radio.revision + 1;
  setup_reset();
  setup_radio = (mesh_setup_radio_t){.state = MESH_SETUP_READING,
                                     .node = local_node_number,
                                     .revision = revision};
  setup_op.origin = local_node_number;
  setup_op.started = esp_timer_get_time();
  if (!setup_get()) {
    setup_end(MESH_SETUP_FAILED);
    return false;
  }
  history_revision++;
  return true;
}
static bool setup_valid_raw(const uint8_t *raw, size_t n, bool lora) {
  // Reject duplicate fields as well as malformed values before retaining a
  // section.
  size_t p = 0;
  while (p < n) {
    protobuf_field_t f;
    size_t start = p;
    if (!read_field(raw, n, &p, &f))
      return false;
    size_t q = 0;
    while (q < start) {
      protobuf_field_t before;
      if (!read_field(raw, start, &q, &before))
        return false;
      bool scalar = lora ? (f.field <= 15 || f.field == 104 || f.field == 105)
                         : f.field <= 9;
      if (scalar && before.field == f.field)
        return false;
    }
  }
  return true;
}
static bool setup_parse(const uint8_t *admin, size_t n, const uint8_t **raw,
                        size_t *length, mesh_setup_radio_t *parsed) {
  protobuf_field_t section, body;
  if (!manage_field(admin, n, setup_op.part ? 6 : 4, &section) ||
      section.wire != 2)
    return false;
  body = section;
  if (setup_op.part) {
    size_t p = 0;
    if (!read_field(section.value, section.length, &p, &body) ||
        p != section.length || body.field != 6 || body.wire != 2)
      return false;
  }
  if (body.length > sizeof(setup_op.raw[0]) ||
      !setup_valid_raw(body.value, body.length, setup_op.part != 0))
    return false;
  if (!setup_op.part) {
    mesh_contact_t owner = {0};
    if (!manage_user(body.value, body.length, &owner))
      return false;
    snprintf(parsed->name, sizeof(parsed->name), "%s", owner.name);
    snprintf(parsed->short_name, sizeof(parsed->short_name), "%s",
             owner.short_name);
  } else {
    size_t p = 0;
    parsed->region = 0;
    while (p < body.length) {
      protobuf_field_t f;
      if (!read_field(body.value, body.length, &p, &f))
        return false;
      if (f.field == 7) {
        if (f.wire != 0 || f.integer > UINT32_MAX)
          return false;
        parsed->region = f.integer;
      }
    }
  }
  *raw = body.value;
  *length = body.length;
  return true;
}
static bool setup_build(unsigned part, uint32_t region, const char *name,
                        const char *short_name) {
  size_t p = 0, n = 0;
  while (p < setup_op.size[part]) {
    size_t start = p;
    protobuf_field_t f;
    if (!read_field(setup_op.raw[part], setup_op.size[part], &p, &f))
      return false;
    if (part ? f.field == 7 : (f.field == 2 || f.field == 3))
      continue;
    if (p - start > sizeof(setup_op.wanted) - n)
      return false;
    memcpy(setup_op.wanted + n, setup_op.raw[part] + start, p - start);
    n += p - start;
  }
  if (part) {
    if (!append_varint(setup_op.wanted, sizeof(setup_op.wanted), &n, 56) ||
        !append_varint(setup_op.wanted, sizeof(setup_op.wanted), &n, region))
      return false;
  } else if (!append_bytes_field(setup_op.wanted, sizeof(setup_op.wanted), &n,
                                 2, (const uint8_t *)name, strlen(name)) ||
             !append_bytes_field(setup_op.wanted, sizeof(setup_op.wanted), &n,
                                 3, (const uint8_t *)short_name,
                                 strlen(short_name)))
    return false;
  setup_op.wanted_size = n;
  return true;
}
static bool setup_check_start(uint32_t revision) {
  if (revision != setup_radio.revision || !setup_start(true))
    return false;
  setup_op.started = esp_timer_get_time();
  setup_op.write_id = 0;
  setup_radio.state = MESH_SETUP_CHECKING;
  setup_radio.error = 0;
  if (!setup_get()) {
    setup_end(MESH_SETUP_FAILED);
    return false;
  }
  history_revision++;
  return true;
}
bool mesh_radio_setup_region(uint32_t region, uint32_t revision) {
  if (!region || !region_matches_hardware(region) ||
      revision != setup_radio.revision || !setup_start(true))
    return false;
  if (!setup_build(1, region, NULL, NULL))
    return false;
  setup_op.kind = SETUP_REGION;
  setup_op.part = 1;
  setup_op.region = region;
  return setup_check_start(revision);
}
static bool setup_nonblank(const char *value, size_t cap) {
  if (!value)
    return false;
  size_t n = strnlen(value, cap);
  if (n == cap)
    return false;
  for (size_t i = 0; i < n; i++)
    if (value[i] != ' ')
      return true;
  return false;
}
static bool setup_names_valid(const char *name, const char *short_name) {
  return setup_nonblank(name, 40) && setup_nonblank(short_name, 5) &&
         manage_text((const uint8_t *)name, strnlen(name, 40), 40) &&
         manage_text((const uint8_t *)short_name, strnlen(short_name, 5), 5);
}
bool mesh_radio_setup_names(const char *name, const char *short_name,
                            uint32_t revision) {
  if (!setup_names_valid(name, short_name) ||
      revision != setup_radio.revision || !setup_start(true))
    return false;
  if (!setup_build(0, 0, name, short_name))
    return false;
  setup_op.kind = SETUP_NAMES;
  setup_op.part = 0;
  snprintf(setup_op.name, sizeof(setup_op.name), "%s", name);
  snprintf(setup_op.short_name, sizeof(setup_op.short_name), "%s", short_name);
  return setup_check_start(revision);
}
bool mesh_radio_setup_confirm(uint32_t region, const char *name,
                              const char *short_name, uint32_t revision) {
  if (!region || !region_matches_hardware(region) ||
      !setup_names_valid(name, short_name) ||
      revision != setup_radio.revision || !setup_start(true))
    return false;
  setup_op.kind = SETUP_CONFIRM;
  setup_op.part = 0;
  setup_op.region = region;
  snprintf(setup_op.name, sizeof(setup_op.name), "%s", name);
  snprintf(setup_op.short_name, sizeof(setup_op.short_name), "%s", short_name);
  return setup_check_start(revision);
}
static bool setup_unchanged(const uint8_t *raw, size_t n) {
  size_t p = 0;
  unsigned part = setup_op.part;
  while (p < setup_op.size[part]) {
    protobuf_field_t f;
    if (!read_field(setup_op.raw[part], setup_op.size[part], &p, &f))
      return false;
    // Meshtastic may normalize modem defaults/power and, on first region,
    // enable TX and apply ignore-MQTT defaults. Verify region separately,
    // keep preset/hops and the other preserved fields checked.
    if (part ? (f.field == 3 || f.field == 4 || f.field == 5 || f.field == 7 ||
                f.field == 9 || f.field == 10 || f.field == 104)
             : (f.field != 1 && f.field != 6))
      continue;
    if (!channel_field_equal(raw, n, setup_op.raw[part], setup_op.size[part],
                             f.field, false))
      return false;
  }
  return true;
}
static void setup_response(const uint8_t *admin, size_t n, uint32_t id) {
  if (!id || id != setup_op.id || setup_op.origin != local_node_number ||
      !setup_online() || !mesh_radio_setup_supported())
    return;
  mesh_setup_state_t state = setup_radio.state;
  if (state != MESH_SETUP_READING && state != MESH_SETUP_CHECKING &&
      state != MESH_SETUP_VERIFYING)
    return;
  mesh_setup_radio_t parsed = setup_radio;
  const uint8_t *raw;
  size_t length;
  if (!setup_parse(admin, n, &raw, &length, &parsed))
    return;
  if (state == MESH_SETUP_CHECKING && setup_op.kind != SETUP_CONFIRM) {
    if (length != setup_op.size[setup_op.part] ||
        memcmp(raw, setup_op.raw[setup_op.part], length)) {
      setup_end(MESH_SETUP_CHANGED);
      return;
    }
    protobuf_field_t token;
    uint8_t body[232], out[260];
    size_t b = 0, a = 0;
    if (!manage_field(admin, n, 101, &token) || token.wire != 2 ||
        !token.length || token.length > 16) {
      setup_end(MESH_SETUP_FAILED);
      return;
    }
    if (setup_op.part) {
      if (!append_bytes_field(body, sizeof(body), &b, 6, setup_op.wanted,
                              setup_op.wanted_size) ||
          !append_bytes_field(out, sizeof(out), &a, 34, body, b)) {
        setup_end(MESH_SETUP_FAILED);
        return;
      }
    } else if (!append_bytes_field(out, sizeof(out), &a, 32, setup_op.wanted,
                                   setup_op.wanted_size)) {
      setup_end(MESH_SETUP_FAILED);
      return;
    }
    if (!append_bytes_field(out, sizeof(out), &a, 101, token.value,
                            token.length) ||
        !send_admin_payload_with_id(out, a, &setup_op.write_id)) {
      setup_end(MESH_SETUP_FAILED);
      return;
    }
    setup_radio.state = MESH_SETUP_SAVING;
    setup_op.id = 0;
    setup_op.read_at = esp_timer_get_time() + 8000000;
    history_revision++;
    return;
  }
  bool match = true;
  if (state == MESH_SETUP_VERIFYING || setup_op.kind == SETUP_CONFIRM) {
    match = setup_op.part ? parsed.region == setup_op.region
                          : !strcmp(parsed.name, setup_op.name) &&
                                !strcmp(parsed.short_name, setup_op.short_name);
    if (state == MESH_SETUP_VERIFYING)
      match = match && setup_unchanged(raw, length);
  }
  if (!match) {
    setup_end(MESH_SETUP_CHANGED);
    return;
  }
  setup_radio = parsed;
  memcpy(setup_op.raw[setup_op.part], raw, length);
  setup_op.size[setup_op.part] = length;
  if ((state == MESH_SETUP_READING || setup_op.kind == SETUP_CONFIRM) &&
      !setup_op.part) {
    setup_op.part = 1;
    if (!setup_get())
      setup_end(MESH_SETUP_FAILED);
    return;
  }
  setup_radio.loaded = true;
  setup_radio.node = local_node_number;
  setup_radio.revision++;
  setup_op.loaded_at = esp_timer_get_time();
  setup_end(state == MESH_SETUP_READING ? MESH_SETUP_READY : MESH_SETUP_SAVED);
}
static void setup_error(uint32_t sender, uint32_t to, uint32_t id,
                        uint32_t error) {
  if (!error || !mesh_radio_setup_busy() || sender != setup_op.origin ||
      sender != local_node_number || (to && to != sender) || !id ||
      (id != setup_op.id && id != setup_op.write_id))
    return;
  // NO_RESPONSE may accompany a radio reboot: continue readback, never repeat
  // SET.
  if (error == 8 && (setup_radio.state == MESH_SETUP_SAVING ||
                     setup_radio.state == MESH_SETUP_VERIFYING))
    return;
  setup_radio.error = error;
  setup_end(MESH_SETUP_FAILED);
}
static void setup_poll(int64_t now) {
  if (setup_op.origin && setup_op.origin != local_node_number) {
    setup_radio.loaded = false;
    setup_end(MESH_SETUP_INTERRUPTED);
    setup_op.origin = 0;
    return;
  }
  if (!mesh_radio_setup_busy())
    return;
  if (now - setup_op.started >= 90000000) {
    setup_end(MESH_SETUP_TIMEOUT);
    return;
  }
  if (!setup_online() || now < setup_op.read_at)
    return;
  if (!mesh_radio_setup_supported()) {
    setup_end(MESH_SETUP_INTERRUPTED);
    return;
  }
  if (setup_radio.state == MESH_SETUP_SAVING)
    setup_radio.state = MESH_SETUP_VERIFYING;
  if (!setup_get())
    setup_end(MESH_SETUP_FAILED);
}
