#define CHANNEL_IMPORT_PATH "channel-import.bin"
#define main message_tests
#include "test_direct_messages.c"
#undef main

static void admin_reply(const uint8_t *admin, size_t admin_size) {
    uint8_t data[300], packet[400], envelope[450];
    size_t d = 0, p = 0, n = 0;
    append_varint(data, sizeof(data), &d, 8); append_varint(data, sizeof(data), &d, 6);
    assert(append_bytes_field(data, sizeof(data), &d, 2, admin, admin_size));
    packet[p++] = 0x0d; memcpy(packet + p, &local_node_number, 4); p += 4;
    packet[p++] = 0x15; memcpy(packet + p, &local_node_number, 4); p += 4;
    assert(append_bytes_field(packet, sizeof(packet), &p, 4, data, d));
    packet[p++] = 0x35; uint32_t id = 4321; memcpy(packet + p, &id, 4); p += 4;
    assert(append_bytes_field(envelope, sizeof(envelope), &n, 2, packet, p));
    decode_from_radio(envelope, n);
}

static protobuf_field_t find_field(const uint8_t *data, size_t size, uint32_t number) {
    size_t offset = 0;
    protobuf_field_t f;
    while (offset < size) { assert(read_field(data, size, &offset, &f)); if (f.field == number) return f; }
    assert(!"Missing expected protobuf field");
    return f;
}

int main(void) {
    message_tests();
    reset();
    owner_identity_received = owner_config_received = false;
    local_long_name[0] = local_short_name[0] = 0;
    session_passkey_size = 0;
    const uint8_t user[] = {0x12, 8, 'M','y',' ','R','a','d','i','o', 0x1a, 4, 'T','E','S','T'};
    uint8_t node[80]; size_t n = 0;
    append_varint(node, sizeof(node), &n, 8); append_varint(node, sizeof(node), &n, local_node_number);
    append_bytes_field(node, sizeof(node), &n, 2, user, sizeof(user));
    local_node_number = 0; // Exercise NodeDB arriving before MyNodeInfo.
    decode_node(node, n);
    assert(!owner_identity_received);
    uint8_t mine[] = {0x08, 0xf8, 0xac, 0xd1, 0x91, 0x01}; // uint32 0x12345678
    decode_my_info(mine, sizeof(mine));
    char long_name[40], short_name[5];
    assert(mesh_radio_get_owner(long_name, sizeof(long_name), short_name, sizeof(short_name)));
    assert(strcmp(long_name, "My Radio") == 0 && strcmp(short_name, "TEST") == 0);
    assert(!mesh_radio_admin_ready()); // Identity does not imply a write session.

    uint8_t admin[240]; n = 0;
    append_bytes_field(admin, sizeof(admin), &n, 4, user, sizeof(user));
    uint8_t token[8]; memset(token, 0x33, sizeof(token));
    append_bytes_field(admin, sizeof(admin), &n, 101, token, sizeof(token));
    owner_identity_received = owner_config_received = false;
    admin_reply(admin, n);
    assert(owner_identity_received && owner_config_received && mesh_radio_admin_ready());
    assert(strcmp(local_long_name, "My Radio") == 0 && strcmp(local_short_name, "TEST") == 0);
    assert(message_count == 0); // Local replies do not become chat echoes.

    uint8_t settings[160]; size_t inner = 0;
    uint8_t key[32]; memset(key, 0xaa, sizeof(key));
    append_bytes_field(settings, sizeof(settings), &inner, 2, key, sizeof(key));
    append_bytes_field(settings, sizeof(settings), &inner, 3, (const uint8_t *)"Old", 3);
    append_varint(settings, sizeof(settings), &inner, 48); append_varint(settings, sizeof(settings), &inner, 1);
    uint8_t unknown[80]; memset(unknown, 0x77, sizeof(unknown));
    append_bytes_field(settings, sizeof(settings), &inner, 99, unknown, sizeof(unknown));
    uint8_t channel[192]; size_t c = 0;
    append_varint(channel, sizeof(channel), &c, 8); append_varint(channel, sizeof(channel), &c, 1);
    append_bytes_field(channel, sizeof(channel), &c, 2, settings, inner);
    append_varint(channel, sizeof(channel), &c, 24); append_varint(channel, sizeof(channel), &c, 2);
    append_varint(channel, sizeof(channel), &c, 240); append_varint(channel, sizeof(channel), &c, 99);
    decode_channel(channel, c);
    mesh_channel_t before;
    assert(mesh_radio_get_channel(1, &before));
    assert(before.has_key && !before.default_key && before.role == 2 && before.downlink_enabled);
    assert(mesh_radio_set_channel(1, "New", true, false));
    assert(sent_bytes[5] & 0x80); // ToRadio.packet length uses multiple varint bytes.
    size_t body_size = ((size_t)sent_bytes[2] << 8) | sent_bytes[3];
    assert(body_size + 4 == sent_size);
    protobuf_field_t packet = find_field(sent_bytes + 4, body_size, 1);
    protobuf_field_t data = find_field(packet.value, packet.length, 4);
    protobuf_field_t payload = find_field(data.value, data.length, 2);
    protobuf_field_t changed = find_field(payload.value, payload.length, 33);
    protobuf_field_t changed_settings = find_field(changed.value, changed.length, 2);
    protobuf_field_t saved_key = find_field(changed_settings.value, changed_settings.length, 2);
    protobuf_field_t saved_unknown = find_field(changed_settings.value, changed_settings.length, 99);
    protobuf_field_t saved_name = find_field(changed_settings.value, changed_settings.length, 3);
    assert(saved_key.length == sizeof(key) && memcmp(saved_key.value, key, sizeof(key)) == 0);
    assert(saved_unknown.length == sizeof(unknown) && memcmp(saved_unknown.value, unknown, sizeof(unknown)) == 0);
    assert(saved_name.length == 3 && memcmp(saved_name.value, "New", 3) == 0);
    assert(find_field(changed_settings.value, changed_settings.length, 5).integer == 1);
    assert(find_field(changed_settings.value, changed_settings.length, 6).integer == 0);
    assert(find_field(changed.value, changed.length, 30).integer == 99);
    assert(channels[1].info.revision == before.revision && strcmp(channels[1].info.name, "Old") == 0);
    n = 0; append_bytes_field(admin, sizeof(admin), &n, 2, changed.value, changed.length);
    admin_reply(admin, n);
    mesh_channel_t after; assert(mesh_radio_get_channel(1, &after));
    assert(after.revision > before.revision && strcmp(after.name, "New") == 0);
    // A fresh, nonce-matched channel dump completes the new verified save.
    fake_time += 1600000; mesh_radio_poll();
    uint8_t verified[256]; size_t vn=0;
    append_bytes_field(verified,sizeof(verified),&vn,10,channels[1].raw,channels[1].size);
    append_varint(verified,sizeof(verified),&vn,56);append_varint(verified,sizeof(verified),&vn,config_nonce);
    decode_from_radio(verified,vn);
    assert(mesh_radio_location_save().state==MESH_LOCATION_SAVED);
    fake_time += 3000001;
    assert(mesh_radio_request_channel(1));
    packet = find_field(sent_bytes + 4, sent_size - 4, 1);
    data = find_field(packet.value, packet.length, 4);
    payload = find_field(data.value, data.length, 2);
    assert(find_field(payload.value, payload.length, 1).integer == 2);
    assert(!mesh_radio_set_channel(8, "invalid", true, false));
    assert(!mesh_radio_set_channel(1, "abcdefghijkl", true, false));
    channels[1].info.role = 0;
    assert(!mesh_radio_set_channel(1, "disabled", true, false));
    // Import only into an unused secondary slot, preserving the shared key/settings.
    FILE *import_file = fopen(CHANNEL_IMPORT_PATH, "wb"); assert(import_file);
    assert(fwrite(settings, inner, 1, import_file) == 1); fclose(import_file);
    mesh_channel_t imported;
    assert(mesh_radio_get_shared_channel(&imported) && strcmp(imported.name, "Old") == 0);
    assert(imported.has_key && !imported.default_key);
    assert(!mesh_radio_add_shared_channel(0));
    assert(!mesh_radio_add_shared_channel(8));
    assert(!mesh_radio_add_shared_channel(2)); // Unread slots cannot be overwritten.
    const uint8_t disabled_slot[] = {8, 2};
    decode_channel(disabled_slot, sizeof(disabled_slot));
    assert(mesh_radio_add_shared_channel(2));
    packet = find_field(sent_bytes + 4, sent_size - 4, 1);
    data = find_field(packet.value, packet.length, 4);
    payload = find_field(data.value, data.length, 2);
    changed = find_field(payload.value, payload.length, 33);
    assert(find_field(changed.value, changed.length, 1).integer == 2);
    assert(find_field(changed.value, changed.length, 3).integer == 2);
    changed_settings = find_field(changed.value, changed.length, 2);
    assert(changed_settings.length == inner && memcmp(changed_settings.value, settings, inner) == 0);
    assert(!mesh_radio_shared_channel_matches(2)); // Nothing optimistically marked confirmed.
    decode_channel(changed.value, changed.length);
    assert(mesh_radio_shared_channel_matches(2));
    assert(!mesh_radio_add_shared_channel(2)); // No overwrite after successful import.
    // A different key with the same name must fail readback verification.
    for (size_t i = 0; i < channels[2].size; i++) if (channels[2].raw[i] == 0xaa) { channels[2].raw[i] = 0xbb; break; }
    assert(!mesh_radio_shared_channel_matches(2));
    import_file = fopen(CHANNEL_IMPORT_PATH, "wb"); assert(import_file);
    fputs("not protobuf", import_file); fclose(import_file);
    assert(!mesh_radio_get_shared_channel(&imported));
    remove(CHANNEL_IMPORT_PATH);
    // Read actual LoRa config, preserve untouched fields, and write only the selected controls.
    uint8_t lora[] = {8,1, 16,0, 24,125, 32,9, 40,5, 56,1, 64,3, 72,1, 80,20, 0x98,6,77};
    uint8_t config[128]; size_t config_size = 0;
    append_bytes_field(config, sizeof(config), &config_size, 6, lora, sizeof(lora));
    uint8_t from_config[160]; size_t from_size = 0;
    append_bytes_field(from_config, sizeof(from_config), &from_size, 5, config, config_size);
    decode_from_radio(from_config, from_size);
    mesh_lora_config_t radio_before;
    assert(mesh_radio_get_lora(&radio_before) && radio_before.region == 1 && radio_before.hop_limit == 3 && radio_before.use_preset);
    assert(!mesh_radio_set_lora(2, 0, 3, true)); // 433MHz does not match this hardware.
    assert(!mesh_radio_set_lora(1, 0, 8, true));
    assert(!mesh_radio_set_lora(1, 99, 3, true));
    assert(mesh_radio_set_lora(3, 4, 5, true));
    assert(lora_config.revision == radio_before.revision && lora_config.region == 1);
    packet = find_field(sent_bytes + 4, sent_size - 4, 1);
    data = find_field(packet.value, packet.length, 4);
    payload = find_field(data.value, data.length, 2);
    protobuf_field_t new_config = find_field(payload.value, payload.length, 34);
    protobuf_field_t new_lora = find_field(new_config.value, new_config.length, 6);
    assert(find_field(new_lora.value, new_lora.length, 1).integer == 1);
    assert(find_field(new_lora.value, new_lora.length, 2).integer == 4);
    assert(find_field(new_lora.value, new_lora.length, 7).integer == 3);
    assert(find_field(new_lora.value, new_lora.length, 8).integer == 5);
    assert(find_field(new_lora.value, new_lora.length, 3).integer == 125); // Custom bandwidth preserved.
    assert(find_field(new_lora.value, new_lora.length, 4).integer == 9);
    assert(find_field(new_lora.value, new_lora.length, 5).integer == 5);
    assert(find_field(new_lora.value, new_lora.length, 9).integer == 1); // TX state preserved.
    assert(find_field(new_lora.value, new_lora.length, 10).integer == 20); // TX power preserved.
    assert(find_field(new_lora.value, new_lora.length, 99).integer == 77); // Unknown field preserved.
    n = 0; append_bytes_field(admin, sizeof(admin), &n, 6, new_config.value, new_config.length);
    admin_reply(admin, n);
    assert(lora_config.revision > radio_before.revision && lora_config.region == 3 && lora_config.modem_preset == 4 && lora_config.hop_limit == 5);
    assert(mesh_radio_send_text("configured hops"));
    packet = find_field(sent_bytes + 4, sent_size - 4, 1);
    assert(find_field(packet.value, packet.length, 9).integer == 5);
    uint32_t id = messages[message_count - 1].packet_id;
    incoming(local_node_number, UINT32_MAX, id, 5);
    assert(messages[message_count - 1].delivery == MESH_DELIVERY_PENDING);
    incoming(local_node_number, UINT32_MAX, id, 4);
    assert(messages[message_count - 1].delivery == MESH_DELIVERY_ACKNOWLEDGED);
    assert(mesh_radio_request_lora());
    packet = find_field(sent_bytes + 4, sent_size - 4, 1);
    data = find_field(packet.value, packet.length, 4);
    payload = find_field(data.value, data.length, 2);
    assert(find_field(payload.value, payload.length, 5).integer == 5);
    assert(mesh_radio_set_lora(3, 4, 0, false)); // Preserve custom mode when preset is not edited.
    packet = find_field(sent_bytes + 4, sent_size - 4, 1);
    data = find_field(packet.value, packet.length, 4);
    payload = find_field(data.value, data.length, 2);
    new_config = find_field(payload.value, payload.length, 34);
    new_lora = find_field(new_config.value, new_config.length, 6);
    assert(find_field(new_lora.value, new_lora.length, 1).integer == 0 && find_field(new_lora.value, new_lora.length, 8).integer == 0);
    assert(find_field(new_lora.value, new_lora.length, 3).integer == 125);
    // Manual-key formats, canonical Base64 validation, and safe secondary creation.
    const char *key16 = "AAECAwQFBgcICQoLDA0ODw==";
    const char *key32 = "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=";
    uint8_t decoded_key[32]; size_t decoded_size;
    assert(decode_channel_key(key16, decoded_key, &decoded_size) && decoded_size == 16);
    for (size_t i = 0; i < decoded_size; i++) assert(decoded_key[i] == i);
    assert(decode_channel_key(key32, decoded_key, &decoded_size) && decoded_size == 32);
    for (size_t i = 0; i < decoded_size; i++) assert(decoded_key[i] == i);
    assert(decode_channel_key("0x000102030405060708090a0b0c0d0e0f", decoded_key, &decoded_size) && decoded_size == 16);
    for (size_t i = 0; i < decoded_size; i++) assert(decoded_key[i] == i);
    assert(decode_channel_key("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", decoded_key, &decoded_size) && decoded_size == 32);
    assert(decode_channel_key("AQ==", decoded_key, &decoded_size) && decoded_size == 1 && decoded_key[0] == 1);
    assert(mesh_radio_channel_key_valid("AQ")); // Unpadded Base64 supported.
    assert(mesh_radio_channel_key_valid("__________________________________________8="));
    assert(mesh_radio_channel_key_valid("//////////////////////////////////////////8="));
    const char *invalid_keys[] = {"", "A", "AQ===", "A===", "AZ==", "AQ==x", "A Q==", "AQ=Q", "abc!", "SGVsbG8=", "0x00", "0x000102030405060708090a0b0c0d0e0g"};
    for (size_t i = 0; i < sizeof(invalid_keys)/sizeof(invalid_keys[0]); i++) assert(!mesh_radio_channel_key_valid(invalid_keys[i]));
    const uint8_t free_slot[] = {8,3};
    decode_channel(free_slot, sizeof(free_slot));
    assert(!mesh_radio_add_manual_channel(0, "Private", key32));
    assert(!mesh_radio_add_manual_channel(8, "Private", key32));
    assert(!mesh_radio_add_manual_channel(4, "Private", key32)); // Unknown slot.
    assert(!mesh_radio_add_manual_channel(3, "", key32));
    assert(!mesh_radio_add_manual_channel(3, "abcdefghijkl", key32));
    assert(!mesh_radio_add_manual_channel(3, "Private", "invalid"));
    uint32_t snapshot_revision = channels[3].info.revision;
    assert(mesh_radio_add_manual_channel(3, "Private", key32));
    assert(channels[3].info.revision == snapshot_revision && !channels[3].info.role);
    packet = find_field(sent_bytes + 4, sent_size - 4, 1);
    data = find_field(packet.value, packet.length, 4);
    payload = find_field(data.value, data.length, 2);
    changed = find_field(payload.value, payload.length, 33);
    assert(find_field(changed.value, changed.length, 1).integer == 3);
    assert(find_field(changed.value, changed.length, 3).integer == 2);
    changed_settings = find_field(changed.value, changed.length, 2);
    saved_key = find_field(changed_settings.value, changed_settings.length, 2);
    saved_name = find_field(changed_settings.value, changed_settings.length, 3);
    assert(saved_key.length == 32 && saved_name.length == 7 && memcmp(saved_name.value, "Private", 7) == 0);
    for (size_t i = 0; i < saved_key.length; i++) assert(saved_key.value[i] == i);
    assert(!mesh_radio_manual_channel_matches(3, "Private", key32));
    decode_channel(changed.value, changed.length);
    assert(mesh_radio_manual_channel_matches(3, "Private", key32));
    assert(!mesh_radio_manual_channel_matches(3, "Other", key32));
    assert(!mesh_radio_manual_channel_matches(3, "Private", key16));
    assert(!mesh_radio_add_manual_channel(3, "Overwrite", key16));
    puts("PASS: current identity, local admin replies, channel/radio readback, untouched field preservation, configured hops, manual key formats/import, long frames");
}
