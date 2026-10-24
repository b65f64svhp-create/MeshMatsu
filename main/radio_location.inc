// Local radio location mutations. Success requires a fresh nonce-matched dump,
// never a UART enqueue ACK or a cached coordinate/configuration snapshot.
static bool location_verifying(void) {
    return location_in_dump && location_save.state == MESH_LOCATION_SAVING &&
        config_request_pending && location_op.nonce && location_op.nonce == config_nonce &&
        local_node_number == location_op.origin;
}
static void location_observe_node(const mesh_node_t *node) {
    if (location_verifying() && node->number == location_op.origin && node->position_available)
        location_op.node_seen = node->latitude_i == location_op.lat && node->longitude_i == location_op.lon;
}
static void decode_position_config(const uint8_t *data, size_t size) {
    if (size > sizeof(position_raw)) return;
    mesh_position_config_t info = {.loaded = true, .revision = position_config.revision + 1};
    size_t o = 0;
    while (o < size) {
        protobuf_field_t f;
        if (!read_field(data, size, &o, &f)) return;
        if (f.field >= 1 && f.field <= 3) {
            if (f.wire != 0 || f.integer > UINT32_MAX) return;
            if (f.field == 1) info.broadcast_secs = (uint32_t)f.integer;
            if (f.field == 2) info.smart = f.integer != 0;
            if (f.field == 3) info.fixed = f.integer != 0;
        }
    }
    memcpy(position_raw, data, size); position_raw_size = size; position_config = info;
    if (location_verifying()) location_op.config_seen = location_op.kind == 2 ? info.fixed :
        info.fixed == location_op.position.fixed && info.smart == location_op.position.smart &&
        info.broadcast_secs == location_op.position.broadcast_secs;
    history_revision++;
}
static void location_dump_field(uint32_t field) {
    if (!location_verifying() || field != 10 || location_op.kind != 3) return;
    mesh_channel_t c = channels[location_op.channel].info;
    // Called only for a decoded channel whose index is the selected slot.
    location_op.channel_seen = c.loaded && c.uplink_enabled == location_op.uplink &&
        c.downlink_enabled == location_op.downlink && !strcmp(c.name, location_op.name) &&
        (location_op.precision < 0 || (c.has_position_precision && c.position_precision == (unsigned)location_op.precision));
}
static void location_dump_complete(uint32_t nonce) {
    if (location_save.state != MESH_LOCATION_SAVING || !location_op.nonce || nonce != location_op.nonce) return;
    bool ok = local_node_number == location_op.origin && (location_op.kind == 3 ? location_op.channel_seen :
        location_op.config_seen && (location_op.kind != 2 || location_op.node_seen));
    location_save.state = ok ? MESH_LOCATION_SAVED : MESH_LOCATION_FAILED;
    history_revision++;
}
static void location_poll(int64_t now) {
    if (location_save.state != MESH_LOCATION_SAVING) return;
    if (now - location_op.started >= 60000000 || (local_node_number && local_node_number != location_op.origin)) {
        location_save.state = MESH_LOCATION_TIMEOUT; history_revision++; return;
    }
    if (now >= location_op.read_at && !config_request_pending && status.started) mesh_radio_request_config();
}
mesh_location_save_t mesh_radio_location_save(void) { return location_save; }
bool mesh_radio_get_position_config(mesh_position_config_t *out) {
    if (!out) return false;
    *out = position_config; return out->loaded;
}
bool mesh_radio_request_position_config(void) {
    if (!status.started || status.reconnecting || !local_node_number) return false;
    int64_t now = esp_timer_get_time();
    if (last_position_request_us && now - last_position_request_us < 3000000) return true;
    const uint8_t request[] = {0x28, 1}; // Admin get_config_request POSITION_CONFIG.
    if (!send_admin_payload(request, sizeof(request))) return false;
    last_position_request_us = now; return true;
}
static bool location_can_write(uint32_t revision) {
    if (!status.synchronized || status.reconnecting || config_request_pending || !position_config.loaded ||
        position_config.revision != revision || location_save.state == MESH_LOCATION_SAVING ||
        telemetry_save.state == MESH_TELEMETRY_CONFIG_SAVING || role_save.state == MESH_ROLE_SAVING || (node_control.state == MESH_FAVORITE_SAVING || channel_operation.state == MESH_FAVORITE_SAVING) || favorite_status.state == MESH_FAVORITE_SAVING) return false;
    if (!mesh_radio_admin_ready() || esp_timer_get_time() - session_passkey_received_us >= 240000000) {
        last_owner_request_us = 0; mesh_radio_request_owner(); return false;
    }
    return true;
}
static bool location_send(unsigned kind, uint32_t field, const uint8_t *data, size_t size) {
    uint8_t admin[256]; size_t n = 0; uint32_t id;
    if (!append_bytes_field(admin, sizeof(admin), &n, field, data, size) ||
        !append_bytes_field(admin, sizeof(admin), &n, 101, session_passkey, session_passkey_size) ||
        !send_admin_payload_with_id(admin, n, &id)) return false;
    location_op.kind = kind; location_op.write_id = id; location_op.origin = local_node_number;
    location_op.started = esp_timer_get_time(); location_op.read_at = location_op.started + 1500000;
    location_op.nonce = 0; location_save = (mesh_location_save_t){.state = MESH_LOCATION_SAVING};
    history_revision++; return true;
}
bool mesh_radio_set_position_config(const mesh_position_config_t *desired) {
    if (!desired || !location_can_write(desired->revision)) return false;
    mesh_node_t local;
    if (desired->fixed && !position_config.fixed &&
        (!mesh_radio_get_node_by_number(local_node_number, &local) || !local.position_available)) return false;
    uint8_t pos[192], config[208]; size_t n = 0, o = 0, c = 0;
    while (o < position_raw_size) {
        size_t start = o; protobuf_field_t f;
        if (!read_field(position_raw, position_raw_size, &o, &f)) return false;
        if (f.field >= 1 && f.field <= 3) continue;
        if (o - start > sizeof(pos) - n) return false;
        memcpy(pos + n, position_raw + start, o - start); n += o - start;
    }
    if (!append_varint(pos, sizeof(pos), &n, 8) || !append_varint(pos, sizeof(pos), &n, desired->broadcast_secs) ||
        !append_varint(pos, sizeof(pos), &n, 16) || !append_varint(pos, sizeof(pos), &n, desired->smart) ||
        !append_varint(pos, sizeof(pos), &n, 24) || !append_varint(pos, sizeof(pos), &n, desired->fixed) ||
        !append_bytes_field(config, sizeof(config), &c, 2, pos, n) || !location_send(1, 34, config, c)) return false;
    location_op.position = *desired; return true;
}
bool mesh_radio_set_fixed_position(int32_t lat, int32_t lon, uint32_t revision) {
    if (lat < -900000000 || lat > 900000000 || lon < -1800000000 || lon > 1800000000 ||
        !location_can_write(revision)) return false;
    uint8_t pos[20]; size_t n = 0;
    pos[n++] = 0x0d; memcpy(pos + n, &lat, 4); n += 4;
    pos[n++] = 0x15; memcpy(pos + n, &lon, 4); n += 4;
    // A manual position has no measured altitude/accuracy or GPS timestamp.
    if (!location_send(2, 41, pos, n)) return false;
    location_op.lat = lat; location_op.lon = lon; return true;
}
