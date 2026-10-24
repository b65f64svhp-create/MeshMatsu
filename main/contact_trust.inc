// App-local key baselines. First sighting is not manual verification.
#ifndef CONTACT_TRUST_PATH
#define CONTACT_TRUST_PATH "/locfd/meshmatsu-contacts.bin"
#endif
static struct {
  uint32_t number;
  uint8_t key[32];
  bool verified;
} contact_trust[MESH_NODE_LIMIT];

static EXT_RAM_BSS_ATTR uint8_t trust_bytes[2][12 + MESH_NODE_LIMIT * 37];
static bool trust_recovered, trust_future;
static size_t contact_trust_count;

static bool contact_trust_ok = true, contact_trust_dirty;

static int64_t contact_trust_save_at;

static bool contact_key_valid(const uint8_t key[32]) {
  unsigned bits = 0;
  for (unsigned i = 0; i < 32; i++)
    bits |= key[i];
  return bits != 0;
}
static bool trust_read(const char *path) {
  uint8_t *bytes = trust_bytes[0];
  FILE *f = fopen(path, "rb");
  if (!f)
    return false;

  size_t n = fread(bytes, 1, sizeof(trust_bytes[0]), f);
  bool ok = !ferror(f) && fgetc(f) == EOF;
  fclose(f);

  if (n >= 4 && !memcmp(bytes, "MCT", 3) && bytes[3] != '1') {
    trust_future = true;
    return false;
  }
  uint32_t count = 0, sum = 0;
  if (n >= 12) {
    memcpy(&count, bytes + 4, 4);
    memcpy(&sum, bytes + n - 4, 4);
  }
  if (!ok || n < 12 || memcmp(bytes, "MCT1", 4) || count > MESH_NODE_LIMIT ||
      n != 12 + count * 37 ||
      sum != history_checksum(2166136261U, bytes, n - 4))
    return false;

  for (size_t i = 0; i < count; i++) {
    const uint8_t *p = bytes + 8 + i * 37;
    uint32_t number;
    memcpy(&number, p, 4);

    if (!number || number == MESH_BROADCAST_NODE || p[36] > 1 ||
        !contact_key_valid(p + 4))
      return false;

    for (size_t j = 0; j < i; j++)
      if (!memcmp(bytes + 8 + j * 37, p, 4))
        return false;
  }
  memset(contact_trust, 0, sizeof(contact_trust));
  contact_trust_count = count;

  for (size_t i = 0; i < count; i++) {
    const uint8_t *p = bytes + 8 + i * 37;
    memcpy(&contact_trust[i].number, p, 4);
    memcpy(contact_trust[i].key, p + 4, 32);
    contact_trust[i].verified = p[36];
  }
  return true;
}
static void trust_load(void) {
  contact_trust_count = 0;
  contact_trust_dirty = false;
  trust_recovered = trust_future = false;
  if (!history_fs_ready) {
    contact_trust_ok = false;
    return;
  }
  bool primary = access(CONTACT_TRUST_PATH, F_OK) == 0,
       backup = access(CONTACT_TRUST_PATH ".bak", F_OK) == 0;
  if (trust_read(CONTACT_TRUST_PATH)) {
    contact_trust_ok = true;
    return;
  }
  if (trust_future) {
    contact_trust_ok = false;
    return;
  }
  if (trust_read(CONTACT_TRUST_PATH ".bak")) {
    contact_trust_ok = true;
    trust_recovered = true;
    return;
  }
  contact_trust_ok = !primary && !backup && !trust_future;
}
static bool trust_save(void) {
  if (!history_fs_ready || !contact_trust_ok)
    return false;

  uint8_t *bytes = trust_bytes[0];
  memset(bytes, 0, sizeof(trust_bytes[0]));
  memcpy(bytes, "MCT1", 4);
  uint32_t count = contact_trust_count;
  memcpy(bytes + 4, &count, 4);

  size_t n = 8;
  for (size_t i = 0; i < count; i++) {
    memcpy(bytes + n, &contact_trust[i].number, 4);
    memcpy(bytes + n + 4, contact_trust[i].key, 32);
    bytes[n + 36] = contact_trust[i].verified;
    n += 37;
  }
  uint32_t sum = history_checksum(2166136261U, bytes, n);
  memcpy(bytes + n, &sum, 4);
  n += 4;

  FILE *f = fopen(CONTACT_TRUST_PATH ".tmp", "wb");
  if (!f)
    return false;

  bool ok =
      fwrite(bytes, 1, n, f) == n && fflush(f) == 0 && fsync(fileno(f)) == 0;
  if (fclose(f))
    ok = false;

  uint8_t *check = trust_bytes[1];
  if (ok) {
    f = fopen(CONTACT_TRUST_PATH ".tmp", "rb");
    ok = f && fread(check, 1, n, f) == n && !memcmp(bytes, check, n) &&
         fgetc(f) == EOF;
    if (f)
      fclose(f);
  }
  bool moved = false;
  if (ok && trust_recovered && access(CONTACT_TRUST_PATH, F_OK) == 0)
    ok = remove(CONTACT_TRUST_PATH) == 0;
  if (ok && !trust_recovered && access(CONTACT_TRUST_PATH, F_OK) == 0) {
    remove(CONTACT_TRUST_PATH ".bak");
    moved = rename(CONTACT_TRUST_PATH, CONTACT_TRUST_PATH ".bak") == 0;
    ok = moved;
  }
  if (ok)
    ok = rename(CONTACT_TRUST_PATH ".tmp", CONTACT_TRUST_PATH) == 0;

  if (!ok) {
    if (moved)
      rename(CONTACT_TRUST_PATH ".bak", CONTACT_TRUST_PATH);
    remove(CONTACT_TRUST_PATH ".tmp");
    return false;
  }
  contact_trust_dirty = false;
  trust_recovered = false;
  return true;
}
mesh_trust_t mesh_radio_contact_trust(uint32_t node, const uint8_t key[32],
                                      uint8_t previous[32]) {
  if (!key || !contact_key_valid(key))
    return MESH_TRUST_UNKNOWN;

  for (size_t i = 0; i < contact_trust_count; i++)
    if (contact_trust[i].number == node) {
      if (previous)
        memcpy(previous, contact_trust[i].key, 32);

      return memcmp(key, contact_trust[i].key, 32) ? MESH_TRUST_CHANGED
             : contact_trust[i].verified           ? MESH_TRUST_VERIFIED
                                                   : MESH_TRUST_SEEN;
    }
  return MESH_TRUST_UNKNOWN;
}
bool mesh_radio_trust_storage_ok(void) {
  return contact_trust_ok && history_fs_ready;
}
bool mesh_radio_verify_contact(uint32_t node, const uint8_t reviewed_key[32]) {
  mesh_node_t live;
  if (!mesh_radio_trust_storage_ok() || !reviewed_key ||
      !mesh_radio_get_node_by_number(node, &live) ||
      !live.public_key_available || memcmp(live.public_key, reviewed_key, 32) ||
      !contact_key_valid(reviewed_key))
    return false;

  size_t i = 0;
  for (; i < contact_trust_count; i++)
    if (contact_trust[i].number == node)
      break;

  if (i == MESH_NODE_LIMIT)
    return false;

  __typeof__(contact_trust[0]) old = contact_trust[i];
  size_t old_count = contact_trust_count;

  contact_trust[i].number = node;
  memcpy(contact_trust[i].key, reviewed_key, 32);
  contact_trust[i].verified = true;

  if (i == contact_trust_count)
    contact_trust_count++;

  if (!trust_save()) {
    contact_trust[i] = old;
    contact_trust_count = old_count;
    return false;
  }
  history_revision++;
  return true;
}
static void trust_poll(int64_t now) {
  if (!contact_trust_ok || !history_fs_ready)
    return;

  for (size_t n = 0; n < node_count; n++) {
    mesh_node_t *node = &nodes[n];
    if (node->is_ignored || !node->public_key_available ||
        !contact_key_valid(node->public_key))
      continue;

    if (mesh_radio_contact_trust(node->number, node->public_key, NULL) !=
        MESH_TRUST_UNKNOWN)
      continue;

    if (contact_trust_count == MESH_NODE_LIMIT)
      break;

    size_t i = contact_trust_count++;
    contact_trust[i].number = node->number;
    memcpy(contact_trust[i].key, node->public_key, 32);
    contact_trust[i].verified = false;

    if (!contact_trust_dirty)
      contact_trust_save_at = now + 1000000;
    contact_trust_dirty = true;
  }
  if (contact_trust_dirty && now >= contact_trust_save_at) {
    if (!trust_save())
      contact_trust_save_at = now + 10000000;
  }
}
