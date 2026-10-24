// Public identity interchange and isolated, correlated administration sessions.
static mesh_security_t security_state;

static uint8_t security_raw[224];
static size_t security_size;

static struct {
  uint32_t origin, id, nonce;
  int64_t started, read_at, loaded;
  bool importing, seen;
  mesh_contact_t contact;
  uint8_t desired[3][32], count;
} security_op;

static uint8_t security_pending[240];
static size_t security_pending_size;
static mesh_remote_t remote;

static struct {
  uint32_t origin, id;
  uint8_t key[32], session[16];
  size_t session_size;
  int64_t started, read_at, loaded, session_at;

  uint8_t pending[180];
  size_t pending_size;
  uint8_t owner[160], device[180], lora[180];
  size_t owner_size, device_size, lora_size;
  mesh_remote_change_t kind;
  char name[40], short_name[5];
  uint32_t value;
  bool verification;
} remote_op;

static bool management_local_busy(void) {
  return security_state.state == MESH_MANAGE_CHECKING ||
         security_state.state == MESH_MANAGE_SAVING ||
         security_state.state == MESH_MANAGE_VERIFYING;
}
static bool manage_busy(mesh_manage_state_t s) {
  return s == MESH_MANAGE_CHECKING || s == MESH_MANAGE_READING ||
         s == MESH_MANAGE_SAVING || s == MESH_MANAGE_VERIFYING;
}
static bool manage_text(const uint8_t *text, size_t n, size_t capacity) {
  if (n >= capacity || memchr(text, 0, n))
    return false;

  for (size_t i = 0; i < n; i++)
    if (text[i] < 32 || text[i] == 127)
      return false;

  // Reject invalid/truncated UTF-8 rather than splitting a name.
  for (size_t i = 0; i < n;) {
    uint8_t c = text[i++];
    if (c < 128)
      continue;
    unsigned more;
    uint32_t cp;

    if (c >= 0xc2 && c <= 0xdf) {
      more = 1;
      cp = c & 31;
    } else if (c >= 0xe0 && c <= 0xef) {
      more = 2;
      cp = c & 15;
    } else if (c >= 0xf0 && c <= 0xf4) {
      more = 3;
      cp = c & 7;
    } else
      return false;

    if (more > n - i)
      return false;
    unsigned length = more;
    while (more--) {
      c = text[i++];
      if ((c & 0xc0) != 0x80)
        return false;
      cp = (cp << 6) | (c & 63);
    }
    if ((length == 1 && cp < 128) || (length == 2 && cp < 2048) ||
        (length == 3 && cp < 65536) || cp > 0x10ffff ||
        (cp >= 0xd800 && cp <= 0xdfff))
      return false;
  }
  return true;
}
static bool manage_field(const uint8_t *bytes, size_t n, uint32_t tag,
                         protobuf_field_t *out) {
  size_t p = 0;
  bool found = false;
  while (p < n) {
    protobuf_field_t f;
    if (!read_field(bytes, n, &p, &f))
      return false;
    if (f.field == tag) {
      if (found)
        return false;
      *out = f;
      found = true;
    }
  }
  return found;
}
static bool manage_user(const uint8_t *bytes, size_t n, mesh_contact_t *out) {
  if (!bytes || n > 160)
    return false;
  size_t p = 0;
  uint32_t seen = 0;
  memset(out->name, 0, sizeof(out->name));
  memset(out->short_name, 0, sizeof(out->short_name));
  memset(out->key, 0, 32);

  while (p < n) {
    protobuf_field_t f;
    if (!read_field(bytes, n, &p, &f))
      return false;

    if (f.field < 10) {
      if (seen & (1u << f.field))
        return false;
      seen |= 1u << f.field;
    }
    if (f.field == 2 || f.field == 3) {
      size_t cap = f.field == 2 ? 40 : 5;
      if (f.wire != 2 || !manage_text(f.value, f.length, cap))
        return false;
      memcpy(f.field == 2 ? out->name : out->short_name, f.value, f.length);
    } else if (f.field == 8) {
      if (f.wire != 2 || f.length != 32)
        return false;
      memcpy(out->key, f.value, 32);
    }
  }
  return true;
}
bool mesh_radio_contact(uint32_t number, mesh_contact_t *out) {
  mesh_node_t n;
  if (!out || !mesh_radio_get_node_by_number(number, &n) || n.is_ignored ||
      !n.public_key_available || !contact_key_valid(n.public_key))
    return false;

  *out = (mesh_contact_t){.number = number};
  snprintf(out->name, sizeof(out->name), "%s",
           n.advertised_name[0] ? n.advertised_name : n.name);

  snprintf(out->short_name, sizeof(out->short_name), "%s", n.short_name);
  memcpy(out->key, n.public_key, 32);
  return true;
}
static bool contact_encode(const mesh_contact_t *c, uint8_t *out, size_t *n) {
  if (!c || !c->number || c->number == MESH_BROADCAST_NODE ||
      !contact_key_valid(c->key) || !memchr(c->name, 0, sizeof(c->name)) ||
      !memchr(c->short_name, 0, sizeof(c->short_name)) ||
      !manage_text((const uint8_t *)c->name, strlen(c->name), 40) ||
      !manage_text((const uint8_t *)c->short_name, strlen(c->short_name), 5))
    return false;

  uint8_t user[128];
  size_t u = 0, p = 0;
  char id[10];
  snprintf(id, sizeof(id), "!%08lx", (unsigned long)c->number);

  if (!append_bytes_field(user, sizeof(user), &u, 1, (const uint8_t *)id, 9) ||
      !append_bytes_field(user, sizeof(user), &u, 2, (const uint8_t *)c->name,
                          strlen(c->name)) ||
      !append_bytes_field(user, sizeof(user), &u, 3,
                          (const uint8_t *)c->short_name,
                          strlen(c->short_name)) ||
      !append_bytes_field(user, sizeof(user), &u, 8, c->key, 32) ||
      !append_varint(out, *n, &p, 8) ||
      !append_varint(out, *n, &p, c->number) ||
      !append_bytes_field(out, *n, &p, 2, user, u))
    return false;

  *n = p;
  return true;
}
bool mesh_radio_contact_url(const mesh_contact_t *contact, char *out,
                            size_t capacity) {
  uint8_t bytes[144];
  size_t n = sizeof(bytes);
  if (!out || capacity < 26 || !contact_encode(contact, bytes, &n))
    return false;

  const char
      *prefix = "https://meshtastic.org/v/#",
      *alphabet =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

  size_t p = strlen(prefix);
  if (capacity <= p + (n * 8 + 5) / 6)
    return false;
  memcpy(out, prefix, p);
  uint32_t bits = 0;
  unsigned count = 0;

  for (size_t i = 0; i < n; i++) {
    bits = (bits << 8) | bytes[i];
    count += 8;
    while (count >= 6) {
      count -= 6;
      out[p++] = alphabet[(bits >> count) & 63];
    }
  }
  if (count)
    out[p++] = alphabet[(bits << (6 - count)) & 63];
  out[p] = 0;
  return true;
}
bool mesh_radio_parse_contact_url(const char *url, mesh_contact_t *out) {
  const char *prefix = "https://meshtastic.org/v/#";
  if (!url || !out || strncmp(url, prefix, strlen(prefix)))
    return false;

  const char *s = url + strlen(prefix);
  size_t len = strlen(s);
  if (!len || len > 200)
    return false;

  size_t padding = 0;
  while (len && s[len - 1] == '=') {
    padding++;
    len--;
  }
  if (padding > 2 || len % 4 == 1 || (padding && (len + padding) % 4))
    return false;

  uint8_t bytes[144];
  size_t n = 0;
  uint32_t bits = 0;
  unsigned count = 0;

  for (size_t i = 0; i < len; i++) {
    int digit = base64_digit(s[i]);
    if (digit < 0)
      return false;
    bits = (bits << 6) | (unsigned)digit;
    count += 6;
    if (count >= 8) {
      count -= 8;
      if (n == sizeof(bytes))
        return false;
      bytes[n++] = (uint8_t)(bits >> count);
    }
  }
  if (count && (bits & ((1u << count) - 1)))
    return false;

  mesh_contact_t result = {0};
  size_t p = 0;
  bool user = false, number = false;

  while (p < n) {
    protobuf_field_t f;
    if (!read_field(bytes, n, &p, &f))
      return false;

    if (f.field == 1) {
      if (number || f.wire != 0 || f.integer > UINT32_MAX)
        return false;
      number = true;
      result.number = f.integer;
    } else if (f.field == 2) {
      if (user || f.wire != 2 || !manage_user(f.value, f.length, &result))
        return false;
      user = true;
    } else if (f.field == 3 || f.field == 4) {
      if (f.wire != 0 || f.integer > 1 || (f.field == 3 && f.integer))
        return false;
    } // Never import ignore/verification claims.
    else
      return false;
  }
  if (!number || !user || !result.number ||
      result.number == MESH_BROADCAST_NODE || !contact_key_valid(result.key))
    return false;

  *out = result;
  return true;
}
static bool security_request(void) {
  uint8_t request[] = {0x28, 7};
  return send_admin_payload_with_id(request, sizeof(request), &security_op.id);
}
bool mesh_radio_security_read(void) {
  if (!channel_management_ready() || manage_busy(security_state.state) ||
      manage_busy(remote.state))
    return false;

  memset(security_raw, 0, sizeof(security_raw));
  security_size = 0;
  security_state = (mesh_security_t){.state = MESH_MANAGE_READING,
                                     .node = local_node_number,
                                     .revision = security_state.revision + 1};

  memset(&security_op, 0, sizeof(security_op));
  security_op.origin = local_node_number;
  security_op.started = esp_timer_get_time();

  if (!security_request()) {
    security_state.state = MESH_MANAGE_FAILED;
    return false;
  }
  history_revision++;
  return true;
}
mesh_security_t mesh_radio_security(void) { return security_state; }
static bool security_parse(const uint8_t *raw, size_t n, mesh_security_t *out) {
  if (n > sizeof(security_raw))
    return false;
  mesh_security_t result = *out;
  result.admin_count = 0;
  memset(result.admins, 0, sizeof(result.admins));
  result.managed = result.serial_enabled = false;

  size_t p = 0;
  uint32_t seen = 0;
  while (p < n) {
    protobuf_field_t f;
    if (!read_field(raw, n, &p, &f))
      return false;

    if (f.field <= 7 && f.field != 3) {
      if (seen & (1u << f.field))
        return false;
      seen |= 1u << f.field;
    }
    if (f.field == 1 || f.field == 2) {
      if (f.wire != 2 || f.length != 32 || !contact_key_valid(f.value))
        return false;
    } else if (f.field == 3) {
      if (f.wire != 2 || f.length != 32 || result.admin_count == 3 ||
          !contact_key_valid(f.value))
        return false;

      for (unsigned i = 0; i < result.admin_count; i++)
        if (!memcmp(result.admins[i], f.value, 32))
          return false;
      memcpy(result.admins[result.admin_count++], f.value, 32);
    } else if (f.field >= 4 && f.field <= 7) {
      if (f.wire != 0 || f.integer > 1)
        return false;
      if (f.field == 4)
        result.managed = f.integer;
      if (f.field == 5)
        result.serial_enabled = f.integer;
    }
  }
  // Changing SecurityConfig without both identity keys could regenerate
  // identity.
  if (!(seen & (1u << 1)) || !(seen & (1u << 2)))
    return false;
  *out = result;
  return true;
}
static void security_response(const uint8_t *payload, size_t n, uint32_t id) {
  if (!id || id != security_op.id || local_node_number != security_op.origin ||
      (security_state.state != MESH_MANAGE_CHECKING &&
       security_state.state != MESH_MANAGE_READING &&
       security_state.state != MESH_MANAGE_VERIFYING) ||
      security_op.importing)
    return;

  protobuf_field_t config, raw;
  if (!manage_field(payload, n, 6, &config) || config.wire != 2 ||
      !manage_field(config.value, config.length, 8, &raw) || raw.wire != 2)
    return;

  mesh_security_t parsed = security_state;
  if (!security_parse(raw.value, raw.length, &parsed)) {
    security_state.state = MESH_MANAGE_FAILED;
    return;
  }
  if (security_state.state == MESH_MANAGE_CHECKING) {
    protobuf_field_t token;
    size_t length = security_pending_size;
    if (raw.length != security_size ||
        memcmp(raw.value, security_raw, security_size) ||
        !manage_field(payload, n, 101, &token) || token.wire != 2 ||
        !token.length || token.length > 16 ||
        !append_bytes_field(security_pending, sizeof(security_pending), &length,
                            101, token.value, token.length) ||
        !send_admin_payload_with_id(security_pending, length,
                                    &security_op.id)) {
      security_state.state = MESH_MANAGE_FAILED;
    } else {
      security_state.state = MESH_MANAGE_SAVING;
      security_op.started = esp_timer_get_time();
      security_op.read_at = security_op.started + 2000000;
    }
    mesh_keys_wipe(security_pending, sizeof(security_pending));
    security_pending_size = 0;
    history_revision++;
    return;
  }
  bool verify = security_state.state == MESH_MANAGE_VERIFYING;

  if (verify &&
      (parsed.admin_count != security_op.count ||
       memcmp(parsed.admins, security_op.desired, sizeof(parsed.admins))))
    parsed.state = MESH_MANAGE_FAILED;

  else
    parsed.state = verify ? MESH_MANAGE_SAVED : MESH_MANAGE_READY;

  security_state = parsed;
  security_state.revision++;
  security_op.loaded = esp_timer_get_time();
  security_op.id = 0;

  mesh_keys_wipe(security_raw, sizeof(security_raw));
  security_size = raw.length;
  memcpy(security_raw, raw.value, raw.length);
  history_revision++;
}
bool mesh_radio_security_set_admin(unsigned slot, const uint8_t key[32],
                                   bool remove, uint32_t revision) {
  if (!channel_management_ready() || manage_busy(remote.state) ||
      security_state.state != MESH_MANAGE_READY || security_op.importing ||
      security_state.node != local_node_number ||
      revision != security_state.revision ||
      esp_timer_get_time() - security_op.loaded > 60000000 ||
      !security_state.serial_enabled || security_state.managed ||
      slot > security_state.admin_count || slot >= 3 ||
      (remove ? slot >= security_state.admin_count
              : !key || !contact_key_valid(key)))
    return false;

  uint8_t desired[3][32];
  memcpy(desired, security_state.admins, sizeof(desired));
  unsigned count = security_state.admin_count;

  if (remove) {
    memmove(desired[slot], desired[slot + 1], (count - slot - 1) * 32);
    memset(desired[--count], 0, 32);
  } else {
    memcpy(desired[slot], key, 32);
    if (slot == count)
      count++;
    for (unsigned i = 0; i < count; i++)
      if (i != slot && !memcmp(desired[i], key, 32))
        return false;
  }
  uint8_t raw[224], config[232], admin[260];
  size_t p = 0, r = 0, c = 0, a = 0;

  while (p < security_size) {
    size_t start = p;
    protobuf_field_t f;
    if (!read_field(security_raw, security_size, &p, &f))
      return false;
    if (f.field == 3)
      continue;

    if (p - start > sizeof(raw) - r)
      return false;
    memcpy(raw + r, security_raw + start, p - start);
    r += p - start;
  }
  for (unsigned i = 0; i < count; i++)
    if (!append_bytes_field(raw, sizeof(raw), &r, 3, desired[i], 32))
      return false;

  if (!append_bytes_field(config, sizeof(config), &c, 8, raw, r) ||
      !append_bytes_field(admin, sizeof(admin), &a, 34, config, c) ||
      a + 19 > 233 || !security_request()) {
    mesh_keys_wipe(raw, sizeof(raw));
    mesh_keys_wipe(config, sizeof(config));
    mesh_keys_wipe(admin, sizeof(admin));
    return false;
  }
  memcpy(security_pending, admin, a);
  security_pending_size = a;
  mesh_keys_wipe(raw, sizeof(raw));
  mesh_keys_wipe(config, sizeof(config));
  mesh_keys_wipe(admin, sizeof(admin));

  memcpy(security_op.desired, desired, sizeof(desired));
  security_op.count = count;
  security_state.state = MESH_MANAGE_CHECKING;
  security_op.started = esp_timer_get_time();
  security_op.read_at = security_op.started + 2000000;
  history_revision++;
  return true;
}
bool mesh_radio_import_contact(const mesh_contact_t *contact) {
  if (!channel_management_ready() || manage_busy(security_state.state) ||
      manage_busy(remote.state) || !contact ||
      contact->number == local_node_number)
    return false;

  mesh_node_t old;
  if (mesh_radio_get_node_by_number(contact->number, &old) && old.is_ignored)
    return false;

  uint8_t bytes[144], admin[192];
  size_t n = sizeof(bytes), a = 0;
  if (!contact_encode(contact, bytes, &n) ||
      !append_bytes_field(admin, sizeof(admin), &a, 66, bytes, n) ||
      !append_bytes_field(admin, sizeof(admin), &a, 101, session_passkey,
                          session_passkey_size))
    return false;

  memset(&security_op, 0, sizeof(security_op));
  if (!send_admin_payload_with_id(admin, a, &security_op.id))
    return false;

  mesh_keys_wipe(security_raw, sizeof(security_raw));
  security_size = 0;
  security_op.contact = *contact;
  security_op.importing = true;
  security_op.origin = local_node_number;
  security_op.started = esp_timer_get_time();
  security_op.read_at = security_op.started + 2000000;

  security_state = (mesh_security_t){.state = MESH_MANAGE_SAVING,
                                     .node = local_node_number,
                                     .revision = security_state.revision + 1};
  history_revision++;
  return true;
}
static void management_observe_contact(const mesh_node_t *node) {
  if (!security_op.importing || security_state.state != MESH_MANAGE_VERIFYING ||
      !config_request_pending || config_nonce != security_op.nonce)
    return;

  const mesh_contact_t *c = &security_op.contact;
  if (node->number == c->number && node->public_key_available &&
      !node->is_ignored && !memcmp(c->key, node->public_key, 32) &&
      !strcmp(c->name, node->name) && !strcmp(c->short_name, node->short_name))
    security_op.seen = true;
}
static void management_complete(uint32_t nonce) {
  if (security_op.importing && security_state.state == MESH_MANAGE_VERIFYING &&
      nonce == security_op.nonce) {
    security_state.state =
        security_op.seen ? MESH_MANAGE_SAVED : MESH_MANAGE_FAILED;
    history_revision++;
  }
}
static bool remote_target_valid(void) {
  mesh_contact_t now;
  return status.started && status.synchronized && !status.reconnecting &&
         local_node_number == remote_op.origin &&
         mesh_radio_contact(remote.node, &now) &&
         !memcmp(now.key, remote_op.key, 32) &&
         mesh_radio_contact_trust(remote.node, now.key, NULL) ==
             MESH_TRUST_VERIFIED &&
         mesh_radio_trust_storage_ok();
}
static bool remote_send(const uint8_t *payload, size_t n, uint32_t *out) {
  if (!remote_target_valid() || !payload || !n || n > 180)
    return false;

  uint8_t data[208], packet[320], frame[FRAME_MAX + 4];
  size_t d = 0, p = 0, t = 4;

  if (!append_varint(data, sizeof(data), &d, 8) ||
      !append_varint(data, sizeof(data), &d, 6) ||
      !append_bytes_field(data, sizeof(data), &d, 2, payload, n) ||
      !append_varint(data, sizeof(data), &d, 24) ||
      !append_varint(data, sizeof(data), &d, 1))
    return false;

  packet[p++] = 0x15;
  memcpy(packet + p, &remote.node, 4);
  p += 4;

  if (!append_bytes_field(packet, sizeof(packet), &p, 4, data, d))
    return false;

  uint32_t id = next_packet_id++;
  if (!id)
    id = next_packet_id++;
  packet[p++] = 0x35;
  memcpy(packet + p, &id, 4);
  p += 4;

  packet[p++] = 0x48;
  packet[p++] = lora_config.loaded ? (uint8_t)lora_config.hop_limit : 3;

  packet[p++] = 0x50;
  packet[p++] = 1;
  packet[p++] = 0x58;
  packet[p++] = 70;

  if (!append_bytes_field(packet, sizeof(packet), &p, 16, remote_op.key, 32) ||
      !append_varint(packet, sizeof(packet), &p, 17 << 3) ||
      !append_varint(packet, sizeof(packet), &p, 1) ||
      !append_bytes_field(frame, sizeof(frame), &t, 1, packet, p))
    return false;

  size_t body = t - 4;
  frame[0] = 0x94;
  frame[1] = 0xc3;
  frame[2] = body >> 8;
  frame[3] = body;

  if (uart_write_bytes(RADIO_UART, frame, t) != (int)t)
    return false;
  *out = id;
  return true;
}
static bool remote_get(uint8_t stage) {
  uint8_t bytes[2];
  if (stage == 0) {
    bytes[0] = 0x18;
    bytes[1] = 1;
  } else if (stage == 1) {
    bytes[0] = 0x60;
    bytes[1] = 1;
  } else {
    bytes[0] = 0x28;
    bytes[1] = stage == 2 ? 0 : 5;
  }
  remote.stage = stage;
  if (!remote_send(bytes, 2, &remote_op.id))
    return false;
  remote_op.started = esp_timer_get_time();
  return true;
}
bool mesh_radio_remote_read(uint32_t number) {
  if (mesh_radio_setup_busy() || mesh_radio_neighbor_config_busy() || manage_busy(remote.state) || manage_busy(security_state.state) ||
      !number || number == local_node_number || number == MESH_BROADCAST_NODE)
    return false;

  mesh_contact_t c;
  if (!mesh_radio_contact(number, &c) ||
      mesh_radio_contact_trust(number, c.key, NULL) != MESH_TRUST_VERIFIED)
    return false;

  uint32_t revision = remote.revision + 1;
  mesh_keys_wipe(&remote_op, sizeof(remote_op));
  remote = (mesh_remote_t){
      .state = MESH_MANAGE_READING, .node = number, .revision = revision};

  remote_op.origin = local_node_number;
  memcpy(remote_op.key, c.key, 32);
  if (!remote_get(0)) {
    remote.state = MESH_MANAGE_FAILED;
    return false;
  }
  history_revision++;
  return true;
}
mesh_remote_t mesh_radio_remote(void) { return remote; }
static bool remote_config(const uint8_t *bytes, size_t n, uint8_t stage) {
  protobuf_field_t raw;
  if (!manage_field(bytes, n, stage == 2 ? 1 : 6, &raw) || raw.wire != 2 ||
      raw.length > 180)
    return false;

  size_t p = 0;
  bool found = false;
  uint32_t value = 0;
  uint32_t tag = stage == 2 ? 1 : 8;

  while (p < raw.length) {
    protobuf_field_t f;
    if (!read_field(raw.value, raw.length, &p, &f))
      return false;

    if (f.field == tag) {
      if (found || f.wire != 0 || f.integer > (stage == 2 ? UINT32_MAX : 7))
        return false;
      value = f.integer;
      found = true;
    }
  }
  if (stage == 2) {
    remote.role = value;
    memcpy(remote_op.device, raw.value, raw.length);
    remote_op.device_size = raw.length;
  } else {
    remote.hops = value;
    memcpy(remote_op.lora, raw.value, raw.length);
    remote_op.lora_size = raw.length;
  }
  return true;
}
static void remote_response(uint32_t sender, uint32_t destination,
                            uint32_t request_id, bool pki, const uint8_t *key,
                            size_t key_size, const uint8_t *payload, size_t n) {
  if (!pki || !key || key_size != 32 || memcmp(key, remote_op.key, 32) ||
      sender != remote.node || destination != local_node_number ||
      !request_id || request_id != remote_op.id || !remote_target_valid() ||
      (remote.state != MESH_MANAGE_CHECKING &&
       remote.state != MESH_MANAGE_READING &&
       remote.state != MESH_MANAGE_VERIFYING))
    return;

  uint32_t expected = remote.stage == 0 ? 4 : remote.stage == 1 ? 13 : 6;
  protobuf_field_t body, token;

  if (!manage_field(payload, n, expected, &body) || body.wire != 2)
    return;

  // A complete valid envelope contains exactly one AdminMessage variant.
  size_t offset = 0;
  unsigned variants = 0;
  while (offset < n) {
    protobuf_field_t f;
    if (!read_field(payload, n, &offset, &f))
      return;

    if (f.field != 101) {
      if (f.field != expected || ++variants > 1)
        return;
    }
  }
  bool token_present = manage_field(payload, n, 101, &token);
  if (token_present && (token.wire != 2 || !token.length || token.length > 16))
    return;

  bool preflight = remote.state == MESH_MANAGE_CHECKING;
  if (preflight) {
    const uint8_t *baseline = remote.stage == 0   ? remote_op.owner
                              : remote.stage == 2 ? remote_op.device
                                                  : remote_op.lora;
    size_t baseline_size = remote.stage == 0   ? remote_op.owner_size
                           : remote.stage == 2 ? remote_op.device_size
                                               : remote_op.lora_size;
    protobuf_field_t check = body;
    if (remote.stage != 0 &&
        (!manage_field(body.value, body.length, remote.stage == 2 ? 1 : 6,
                       &check) ||
         check.wire != 2))
      return;
    if (!token_present || check.length != baseline_size ||
        memcmp(check.value, baseline, baseline_size)) {
      remote.state = MESH_MANAGE_FAILED;
      remote_op.id = 0;
      remote_op.pending_size = 0;
      history_revision++;
      return;
    }
  }
  bool ok = false;
  mesh_contact_t c = {0};

  if (remote.stage == 0) {
    ok = manage_user(body.value, body.length, &c) &&
         (!contact_key_valid(c.key) || !memcmp(c.key, remote_op.key, 32));
    if (ok) {
      snprintf(remote.name, sizeof(remote.name), "%s", c.name);
      snprintf(remote.short_name, sizeof(remote.short_name), "%s",
               c.short_name);

      remote_op.owner_size = body.length;
      memcpy(remote_op.owner, body.value, body.length);
    }
  } else if (remote.stage == 1) {
    protobuf_field_t version;
    ok = manage_field(body.value, body.length, 1, &version) &&
         version.wire == 2 &&
         manage_text(version.value, version.length, sizeof(remote.firmware));

    if (ok) {
      memcpy(remote.firmware, version.value, version.length);
      remote.firmware[version.length] = 0;
    }
  } else
    ok = remote_config(body.value, body.length, remote.stage);

  if (!ok)
    return;

  if (token_present) {
    memcpy(remote_op.session, token.value, token.length);
    remote_op.session_size = token.length;
    remote_op.session_at = esp_timer_get_time();
  }
  remote_op.id = 0;

  if (preflight) {
    size_t a = remote_op.pending_size;
    if (!append_bytes_field(remote_op.pending, sizeof(remote_op.pending), &a,
                            101, remote_op.session, remote_op.session_size) ||
        !remote_send(remote_op.pending, a, &remote_op.id))
      remote.state = MESH_MANAGE_FAILED;
    else {
      remote.state = MESH_MANAGE_SAVING;
      remote_op.started = esp_timer_get_time();
      remote_op.read_at = remote_op.started + 15000000;
    }
    mesh_keys_wipe(remote_op.pending, sizeof(remote_op.pending));
    remote_op.pending_size = 0;
    history_revision++;
    return;
  }
  if (remote_op.verification) {
    bool match = remote_op.kind == MESH_REMOTE_OWNER
                     ? !strcmp(remote.name, remote_op.name) &&
                           !strcmp(remote.short_name, remote_op.short_name)
                 : remote_op.kind == MESH_REMOTE_HOPS
                     ? remote.hops == remote_op.value
                     : remote.role == remote_op.value;

    remote.state = match ? MESH_MANAGE_SAVED : MESH_MANAGE_FAILED;
    remote.revision++;
    remote_op.verification = false;
    remote_op.loaded = esp_timer_get_time();
  } else if (remote.stage < 3) {
    if (!remote_get(remote.stage + 1))
      remote.state = MESH_MANAGE_FAILED;
  } else {
    remote.state = MESH_MANAGE_READY;
    remote.revision++;
    remote_op.loaded = esp_timer_get_time();
  }
  history_revision++;
}
static bool manage_patch(const uint8_t *raw, size_t n, uint32_t tag,
                         uint32_t value, uint8_t *out, size_t *size) {
  size_t p = 0, u = 0;
  while (p < n) {
    size_t start = p;
    protobuf_field_t f;
    if (!read_field(raw, n, &p, &f))
      return false;
    if (f.field == tag)
      continue;

    if (p - start > *size - u)
      return false;
    memcpy(out + u, raw + start, p - start);
    u += p - start;
  }
  if (!append_varint(out, *size, &u, tag << 3) ||
      !append_varint(out, *size, &u, value))
    return false;
  *size = u;
  return true;
}
bool mesh_radio_remote_change(mesh_remote_change_t kind, const char *name,
                              const char *short_name, uint32_t value,
                              uint32_t revision) {
  if (remote.state != MESH_MANAGE_READY || revision != remote.revision ||
      !remote_target_valid() || !remote_op.session_size ||
      esp_timer_get_time() - remote_op.loaded > 120000000 ||
      esp_timer_get_time() - remote_op.session_at >= 240000000 ||
      strncmp(remote.firmware, "2.7.", 4) || kind > MESH_REMOTE_REBOOT)
    return false;

  uint8_t admin[240], raw[180], config[192];
  size_t a = 0, r = 0, c = 0;

  if (kind == MESH_REMOTE_OWNER) {
    if (!name || !short_name || !name[0] || !short_name[0] ||
        !manage_text((const uint8_t *)name, strlen(name), 40) ||
        !manage_text((const uint8_t *)short_name, strlen(short_name), 5))
      return false;

    size_t p = 0;
    while (p < remote_op.owner_size) {
      size_t start = p;
      protobuf_field_t f;
      if (!read_field(remote_op.owner, remote_op.owner_size, &p, &f))
        return false;
      if (f.field == 2 || f.field == 3)
        continue;

      if (p - start > sizeof(raw) - r)
        return false;
      memcpy(raw + r, remote_op.owner + start, p - start);
      r += p - start;
    }
    if (!append_bytes_field(raw, sizeof(raw), &r, 2, (const uint8_t *)name,
                            strlen(name)) ||
        !append_bytes_field(raw, sizeof(raw), &r, 3,
                            (const uint8_t *)short_name, strlen(short_name)) ||
        !append_bytes_field(admin, sizeof(admin), &a, 32, raw, r))
      return false;

  } else if (kind == MESH_REMOTE_REBOOT) {
    if (!append_varint(admin, sizeof(admin), &a, 97 << 3) ||
        !append_varint(admin, sizeof(admin), &a, 5))
      return false;
  } else {
    const uint8_t *source =
        kind == MESH_REMOTE_HOPS ? remote_op.lora : remote_op.device;
    size_t size =
        kind == MESH_REMOTE_HOPS ? remote_op.lora_size : remote_op.device_size;

    if (kind == MESH_REMOTE_HOPS && value > 7)
      return false;

    if (kind == MESH_REMOTE_ROLE) {
      protobuf_field_t mode = {0};
      bool has = manage_field(source, size, 6, &mode);
      if (!mesh_radio_role_supported(value) ||
          (has && (mode.wire != 0 ||
                   !mesh_radio_behavior_supported(value, mode.integer))))
        return false;
    }
    r = sizeof(raw);
    if (!manage_patch(source, size, kind == MESH_REMOTE_HOPS ? 8 : 1, value,
                      raw, &r) ||
        !append_bytes_field(config, sizeof(config), &c,
                            kind == MESH_REMOTE_HOPS ? 6 : 1, raw, r) ||
        !append_bytes_field(admin, sizeof(admin), &a, 34, config, c))
      return false;
  }
  if (a + 19 > sizeof(remote_op.pending))
    return false;
  memcpy(remote_op.pending, admin, a);
  remote_op.pending_size = a;
  if (!remote_get(kind == MESH_REMOTE_OWNER || kind == MESH_REMOTE_REBOOT ? 0
                  : kind == MESH_REMOTE_HOPS                              ? 3
                                             : 2)) {
    remote_op.pending_size = 0;
    return false;
  }

  remote_op.kind = kind;
  remote_op.value = value;
  if (kind == MESH_REMOTE_OWNER) {
    snprintf(remote_op.name, sizeof(remote_op.name), "%s", name);
    snprintf(remote_op.short_name, sizeof(remote_op.short_name), "%s",
             short_name);
  }
  remote.state = MESH_MANAGE_CHECKING;
  remote_op.started = esp_timer_get_time();
  remote_op.read_at = remote_op.started + 15000000;
  remote_op.verification = false;
  history_revision++;
  return true;
}
static void management_ack(uint32_t sender, uint32_t destination, uint32_t id,
                           uint32_t error) {
  bool addressed = destination == local_node_number ||
                   (sender == local_node_number && !destination);
  if (!addressed || !id)
    return;

  if (manage_busy(security_state.state) && security_op.id == id &&
      sender == local_node_number && error) {
    security_state.state = MESH_MANAGE_FAILED;
    security_state.error = error;
    history_revision++;
  }
  if (manage_busy(remote.state) && remote_op.id == id &&
      (sender == remote.node || sender == local_node_number)) {
    if (error) {
      remote.state = MESH_MANAGE_FAILED;
      remote.error = error;
    } else if (remote.state == MESH_MANAGE_SAVING &&
               remote_op.kind == MESH_REMOTE_REBOOT && sender == remote.node)
      remote.state = MESH_MANAGE_REBOOT_ACK;

    else
      return;
    history_revision++;
  }
}
static void management_poll(int64_t now) {
  if (manage_busy(security_state.state)) {
    if (local_node_number != security_op.origin) {
      security_state.state = MESH_MANAGE_INTERRUPTED;
      mesh_keys_wipe(security_raw, sizeof(security_raw));
      security_size = 0;
      history_revision++;
    } else if (now - security_op.started > 120000000) {
      security_state.state = MESH_MANAGE_TIMEOUT;
      mesh_keys_wipe(security_raw, sizeof(security_raw));
      security_size = 0;
      history_revision++;
    } else if (security_state.state == MESH_MANAGE_SAVING &&
               now >= security_op.read_at && status.synchronized &&
               !status.reconnecting) {
      security_state.state = MESH_MANAGE_VERIFYING;

      if (security_op.importing) {
        mesh_radio_request_config();
        security_op.nonce = config_nonce;
        security_op.seen = false;
      } else if (!security_request())
        security_state.state = MESH_MANAGE_FAILED;
    }
  } else if (security_size && now - security_op.loaded > 60000000) {
    mesh_keys_wipe(security_raw, sizeof(security_raw));
    security_size = 0;
    if (security_state.state == MESH_MANAGE_READY)
      security_state.state = MESH_MANAGE_IDLE;
  }
  if (!manage_busy(security_state.state) &&
      security_state.state != MESH_MANAGE_READY) {
    mesh_keys_wipe(security_raw, sizeof(security_raw));
    mesh_keys_wipe(security_pending, sizeof(security_pending));
    security_pending_size = 0;
    security_size = 0;
  }
  if (!manage_busy(remote.state) && remote.state != MESH_MANAGE_READY) {
    mesh_keys_wipe(remote_op.session, sizeof(remote_op.session));
    remote_op.session_size = 0;
  }
  if (manage_busy(remote.state)) {
    if (!remote_target_valid()) {
      remote.state = MESH_MANAGE_INTERRUPTED;
      mesh_keys_wipe(remote_op.session, sizeof(remote_op.session));
      remote_op.session_size = 0;
      history_revision++;
    } else if (now - remote_op.started > 120000000) {
      remote.state = MESH_MANAGE_TIMEOUT;
      history_revision++;
    } else if (remote.state == MESH_MANAGE_SAVING &&
               remote_op.kind != MESH_REMOTE_REBOOT &&
               now >= remote_op.read_at) {
      remote.state = MESH_MANAGE_VERIFYING;
      remote_op.verification = true;

      if (!remote_get(remote_op.kind == MESH_REMOTE_OWNER  ? 0
                      : remote_op.kind == MESH_REMOTE_HOPS ? 3
                                                           : 2))
        remote.state = MESH_MANAGE_FAILED;
    }
  } else if (remote_op.session_size &&
             now - remote_op.session_at >= 240000000) {
    mesh_keys_wipe(remote_op.session, sizeof(remote_op.session));
    remote_op.session_size = 0;
  }
}
void mesh_radio_management_clear(void) {
  if (!manage_busy(security_state.state)) {
    mesh_keys_wipe(security_pending, sizeof(security_pending));
    security_pending_size = 0;
    mesh_keys_wipe(security_raw, sizeof(security_raw));
    security_size = 0;
    security_state.state = MESH_MANAGE_IDLE;
  }
  if (!manage_busy(remote.state)) {
    mesh_keys_wipe(&remote_op, sizeof(remote_op));
    remote.state = MESH_MANAGE_IDLE;
  }
}
bool mesh_contact_key_parse(const char *text, uint8_t out[32]) {
  size_t n = 0;
  return text && out && decode_channel_key(text, out, &n) && n == 32 &&
         contact_key_valid(out);
}
