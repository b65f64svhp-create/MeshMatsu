// Telemetry ModuleConfig: get=Admin(7, TELEMETRY_CONFIG=5), response=8,
// set=35; ModuleConfig.telemetry=6, interval=1, device enabled=14.
// Verified against the installed 2.7.18 tree's generated protobuf definitions.
static bool telemetry_enable_supported(void) {
    if (telemetry_enable_seen) return true; // Explicit tag 14 observed.
    unsigned major = 0, minor = 0, patch = 0;
    // Be conservative for unknown older versions; 2.6.7 had no enabled switch.
    return radio_metadata.loaded && sscanf(radio_metadata.firmware_version, "%u.%u.%u", &major, &minor, &patch) == 3 &&
        (major > 2 || (major == 2 && (minor > 7 || (minor == 7 && patch >= 18))));
}

static bool decode_telemetry_config(const uint8_t *data, size_t size, uint32_t request_id) {
    if (size > sizeof(telemetry_module_raw)) return false;
    const uint8_t *raw = NULL; size_t raw_size = 0, offset = 0;
    while (offset < size) {
        protobuf_field_t f;
        if (!read_field(data, size, &offset, &f)) return false;
        if (f.field == 6) {
            if (raw || f.wire != 2 || f.length > sizeof(telemetry_config_raw)) return false;
            raw = f.value; raw_size = f.length;
        } else if (f.field >= 1 && f.field <= 13) {
            // ModuleConfig uses a oneof. Other module variants are not telemetry.
            return false;
        }
    }
    if (!raw) return false;
    mesh_telemetry_config_t next = {.loaded = true, .revision = telemetry_config.revision + 1};
    offset = 0;
    while (offset < raw_size) {
        protobuf_field_t f;
        if (!read_field(raw, raw_size, &offset, &f)) return false;
        if (f.field == 1 || f.field == 14) {
            if (f.wire != 0 || f.integer > UINT32_MAX) return false;
            if (f.field == 1) next.interval_secs = (uint32_t)f.integer;
            else { next.enabled = f.integer != 0; next.enable_supported = true; }
        }
    }
    memcpy(telemetry_module_raw, data, size); telemetry_module_size = size;
    memcpy(telemetry_config_raw, raw, raw_size); telemetry_config_size = raw_size;
    telemetry_enable_seen |= next.enable_supported;
    telemetry_config = next;
    telemetry_config.enable_supported = telemetry_enable_supported();
    if (telemetry_save.state == MESH_TELEMETRY_CONFIG_SAVING && telemetry_config_op.read_id &&
        request_id == telemetry_config_op.read_id && local_node_number == telemetry_config_op.origin &&
        status.synchronized && !status.reconnecting) {
        const mesh_telemetry_config_t *wanted = &telemetry_config_op.desired;
        // The zero/default interval can be expanded to 1800 by the firmware.
        bool interval_matches = next.interval_secs == wanted->interval_secs ||
            (!wanted->interval_secs && next.interval_secs == 1800);
        bool enabled_matches = !wanted->enable_supported || next.enabled == wanted->enabled;
        telemetry_save.state = interval_matches && enabled_matches ? MESH_TELEMETRY_CONFIG_SAVED : MESH_TELEMETRY_CONFIG_FAILED;
        telemetry_config_op.read_id = 0;
    }
    history_revision++;
    return true;
}

bool mesh_radio_get_telemetry_config(mesh_telemetry_config_t *out) {
    if (!out) return false;
    *out = telemetry_config;
    out->enable_supported = telemetry_enable_supported();
    return out->loaded;
}
mesh_telemetry_save_t mesh_radio_telemetry_save(void) { return telemetry_save; }

bool mesh_radio_request_telemetry_config(void) {
    if (!status.started || status.reconnecting || !status.synchronized || !local_node_number || config_request_pending) return false;
    int64_t now = esp_timer_get_time();
    if (telemetry_config_op.request_sent && now - telemetry_config_op.last_request_us < 3000000) return true;
    const uint8_t request[] = {0x38, 5};
    uint32_t id;
    if (!send_admin_payload_with_id(request, sizeof(request), &id)) return false;
    telemetry_config_op.last_request_us = now; telemetry_config_op.request_sent = true;
    if (telemetry_save.state == MESH_TELEMETRY_CONFIG_SAVING && now >= telemetry_config_op.read_at_us)
        telemetry_config_op.read_id = id;
    return true;
}

static void telemetry_config_error(uint32_t sender, uint32_t destination, uint32_t request_id, uint32_t error) {
    if (telemetry_save.state == MESH_TELEMETRY_CONFIG_SAVING && error && error != 8 &&
        sender == telemetry_config_op.origin && local_node_number == telemetry_config_op.origin &&
        (destination == telemetry_config_op.origin || !destination) &&
        (request_id == telemetry_config_op.write_id || (telemetry_config_op.read_id && request_id == telemetry_config_op.read_id))) {
        telemetry_save = (mesh_telemetry_save_t){.state = MESH_TELEMETRY_CONFIG_FAILED, .error_reason = error};
        history_revision++;
    }
}

static void telemetry_config_poll(int64_t now) {
    if (telemetry_save.state != MESH_TELEMETRY_CONFIG_SAVING) return;
    if (local_node_number && local_node_number != telemetry_config_op.origin) {
        telemetry_save.state = MESH_TELEMETRY_CONFIG_INTERRUPTED; history_revision++; return;
    }
    if (now - telemetry_config_op.started_us >= 60000000) {
        telemetry_save.state = MESH_TELEMETRY_CONFIG_TIMEOUT; history_revision++; return;
    }
    if (now >= telemetry_config_op.read_at_us && status.synchronized && !status.reconnecting && !config_request_pending) {
        // Reads may repeat while waiting. The mutation above is never resent.
        mesh_radio_request_telemetry_config();
    }
}

bool mesh_radio_set_telemetry_config(const mesh_telemetry_config_t *desired) {
    if (!desired || !telemetry_config.loaded || desired->revision != telemetry_config.revision ||
        !status.synchronized || status.reconnecting || config_request_pending ||
        telemetry_save.state == MESH_TELEMETRY_CONFIG_SAVING || location_save.state == MESH_LOCATION_SAVING ||
        role_save.state == MESH_ROLE_SAVING || (node_control.state == MESH_FAVORITE_SAVING || channel_operation.state == MESH_FAVORITE_SAVING) || favorite_status.state == MESH_FAVORITE_SAVING ||
        channel_verify_index >= 0 || owner_verify_at_us || lora_verify_at_us) return false;
    bool supported = telemetry_enable_supported();
    if (!supported && desired->enabled != telemetry_config.enabled) return false;
    if (!mesh_radio_admin_ready() || esp_timer_get_time() - session_passkey_received_us >= 240000000) {
        last_owner_request_us = 0; mesh_radio_request_owner(); return false;
    }
    uint8_t raw[192], module[224], admin[256]; size_t n = 0, m = 0, a = 0, offset = 0;
    while (offset < telemetry_config_size) {
        size_t start = offset; protobuf_field_t f;
        if (!read_field(telemetry_config_raw, telemetry_config_size, &offset, &f)) return false;
        if (f.field == 1 || (supported && f.field == 14)) continue;
        if (offset - start > sizeof(raw) - n) return false;
        memcpy(raw + n, telemetry_config_raw + start, offset - start); n += offset - start;
    }
    if (!append_varint(raw, sizeof(raw), &n, 8) || !append_varint(raw, sizeof(raw), &n, desired->interval_secs)) return false;
    if (supported && (!append_varint(raw, sizeof(raw), &n, 112) || !append_varint(raw, sizeof(raw), &n, desired->enabled))) return false;
    offset = 0;
    while (offset < telemetry_module_size) {
        size_t start = offset; protobuf_field_t f;
        if (!read_field(telemetry_module_raw, telemetry_module_size, &offset, &f)) return false;
        if (f.field == 6) continue;
        if (offset - start > sizeof(module) - m) return false;
        memcpy(module + m, telemetry_module_raw + start, offset - start); m += offset - start;
    }
    uint32_t id;
    if (!append_bytes_field(module, sizeof(module), &m, 6, raw, n) ||
        !append_bytes_field(admin, sizeof(admin), &a, 35, module, m) ||
        !append_bytes_field(admin, sizeof(admin), &a, 101, session_passkey, session_passkey_size) ||
        !send_admin_payload_with_id(admin, a, &id)) return false;
    int64_t now = esp_timer_get_time();
    telemetry_config_op.origin = local_node_number; telemetry_config_op.write_id = id; telemetry_config_op.read_id = 0;
    telemetry_config_op.started_us = now; telemetry_config_op.read_at_us = now + 1500000;
    telemetry_config_op.request_sent = false;
    telemetry_config_op.desired = *desired; telemetry_config_op.desired.enable_supported = supported;
    telemetry_save = (mesh_telemetry_save_t){.state = MESH_TELEMETRY_CONFIG_SAVING};
    history_revision++;
    return true;
}
