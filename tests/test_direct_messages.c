#include <assert.h>
#define HISTORY_PATH "history.bin"
#define HISTORY_TEMP_PATH "history.tmp"
#define LEGACY_HISTORY_PATH "legacy.bin"
#include "../main/radio_link.c"

static void reset(void) {
    mesh_tlm_reset();mesh_trails_reset();
    waypoint_count=0;waypoint_dirty=false;memset(waypoint_sent_us,0,sizeof(waypoint_sent_us));
    channel_operation=(mesh_channel_operation_t){0};memset(&channel_operation_data,0,sizeof(channel_operation_data));
    node_control=(mesh_node_control_status_t){0};
    memset(&node_preferences,0,sizeof(node_preferences));
    message_alerts=0;telemetry_enable_seen=false;telemetry_config=(mesh_telemetry_config_t){0};telemetry_save=(mesh_telemetry_save_t){0};
    telemetry_module_size=telemetry_config_size=0;memset(&telemetry_config_op,0,sizeof(telemetry_config_op));
    memset(telemetry_requests,0,sizeof(telemetry_requests));telemetry_replace_index=0;
    last_telemetry_request_us=0;telemetry_request_sent=false;
    local_activity=(mesh_device_metrics_t){0};local_activity_node=0;
    location_save=(mesh_location_save_t){0};memset(&location_op,0,sizeof(location_op));location_in_dump=false;
    position_config=(mesh_position_config_t){0};position_raw_size=0;last_position_request_us=0;
    history_limit=500;history_dirty=false;history_save_at_us=0;
    device_config=(mesh_device_config_t){0};device_raw_size=0;
    role_save=(mesh_role_save_t){0};role_write_id=role_read_id=role_origin=0;
    role_started_us=role_read_at_us=last_device_request_us=0;
    radio_metadata = (mesh_radio_metadata_t){0}; last_metadata_request_us = 0;
    memset(&node_seen, 0, sizeof(node_seen)); node_seen_dirty = false; node_seen_save_at_us = 0;
    favorite_status = (mesh_favorite_status_t){0};
    favorite_request_id = favorite_verify_nonce = 0;
    favorite_started_us = favorite_verify_at_us = session_passkey_received_us = 0;
    favorite_seen = favorite_observed = false;
    session_passkey_size = 0;
    memset(traces, 0, sizeof(traces));
    memset(trace_started_us, 0, sizeof(trace_started_us));
    last_trace_started_us = 0; trace_replace_index = 0;
    memset(messages, 0, sizeof(messages));
    memset(message_sent_us, 0, sizeof(message_sent_us));
    memset(message_unread, 0, sizeof(message_unread));
    reaction_count = 0;
    memset(reactions, 0, sizeof(reactions));
    memset(&status, 0, sizeof(status));
    status.started = status.synchronized = true;
    message_count = node_count = 0;
    local_node_number = 0x12345678;
    next_packet_id = 100;
    history_fs_ready = false;
    short_write = false;
    memset(&lora_config, 0, sizeof(lora_config));
    memset(message_hop_limit, 0, sizeof(message_hop_limit));
    last_lora_request_us = lora_verify_at_us = 0;
    owner_config_received = true;
    config_nonce = 1000;
    config_request_pending = false;
    last_radio_activity_us = last_rx_byte_us = fake_time;
    last_heartbeat_us = fake_time;
    probe_started_us = 0;
    config_retry_delay_us = 10000000;
    next_config_retry_us = fake_time + config_retry_delay_us;
    received_size = received_offset = rx_size = write_count = 0;
    read_error = false;
    power_error = uart_config_error = 0;
}

static void ack(uint32_t sender, uint32_t id, uint8_t error) {
    uint8_t data[] = {0x08, 5, 0x12, 2, 0x18, error, 0x35, 0, 0, 0, 0};
    memcpy(data + 7, &id, 4);
    parse_routing_ack(data, sizeof(data), sender, local_node_number);
}

static void incoming(uint32_t from, uint32_t to, uint32_t id, uint8_t hop_limit) {
    // MeshPacket from/to/id fixed32; decoded TEXT_MESSAGE_APP payload "hi".
    uint8_t packet[] = {0x0d,0,0,0,0, 0x15,0,0,0,0,
                        0x22,6, 0x08,1,0x12,2,'h','i',
                        0x35,0,0,0,0, 0x48,hop_limit};
    memcpy(packet + 1, &from, 4);
    memcpy(packet + 6, &to, 4);
    memcpy(packet + 19, &id, 4);
    decode_mesh_packet(packet, sizeof(packet));
}

static void routing_from_radio(uint32_t sender, uint32_t request_id, uint8_t error) {
    // Full FromRadio.packet envelope: the ACK's packet ID differs from request_id.
    uint8_t frame[] = {0x12, 28,
        0x0d,0,0,0,0, 0x15,0,0,0,0,
        0x22,11, 0x08,5,0x12,2,0x18,error,0x35,0,0,0,0,
        0x35,0x55,0x55,0,0};
    memcpy(frame + 3, &sender, 4);
    memcpy(frame + 8, &local_node_number, 4);
    memcpy(frame + 21, &request_id, 4);
    decode_from_radio(frame, sizeof(frame));
}

int main(void) {
    const uint32_t alice = 0x11223344, bob = 0xaabbccdd;
    reset();
    assert(!mesh_radio_send_direct(0, "hi"));
    assert(!mesh_radio_send_direct(local_node_number, "hi"));
    assert(!mesh_radio_send_direct(UINT32_MAX, "hi"));
    assert(!mesh_radio_send_direct(alice, ""));
    char long_text[MESH_TEXT_MAX_LENGTH + 2]; memset(long_text, 'x', MESH_TEXT_MAX_LENGTH + 1); long_text[MESH_TEXT_MAX_LENGTH + 1] = 0;
    assert(!mesh_radio_send_direct(alice, long_text));
    short_write = true;
    assert(!mesh_radio_send_direct(alice, "hi"));
    assert(message_count == 0);
    short_write = false;
    assert(mesh_radio_send_direct(alice, "hello"));
    // Assert the actual transport bytes use a unicast destination, not broadcast.
    assert(sent_bytes[0] == 0x94 && sent_bytes[1] == 0xc3);
    assert(sent_bytes[4] == 0x0a && sent_bytes[6] == 0x15);
    assert(memcmp(sent_bytes + 7, "\x44\x33\x22\x11", 4) == 0);
    assert(sent_bytes[sent_size - 4] == 0x50 && sent_bytes[sent_size - 3] == 1);
    uint32_t id = messages[0].packet_id;
    assert(messages[0].peer_node == alice && messages[0].outgoing);
    routing_from_radio(local_node_number, id, 0);
    assert(messages[0].delivery == MESH_DELIVERY_PENDING);
    ack(bob, id, 0);
    assert(messages[0].delivery == MESH_DELIVERY_PENDING);
    incoming(local_node_number, alice, id, 2);
    assert(messages[0].delivery == MESH_DELIVERY_PENDING && message_count == 1);
    ack(alice, id, 0);
    assert(messages[0].delivery == MESH_DELIVERY_ACKNOWLEDGED);

    assert(mesh_radio_send_text("channel"));
    assert(memcmp(sent_bytes + 7, "\xff\xff\xff\xff", 4) == 0);
    uint32_t broadcast_id = messages[1].packet_id;
    routing_from_radio(local_node_number, broadcast_id, 0);
    assert(messages[1].delivery == MESH_DELIVERY_ACKNOWLEDGED);
    messages[1].delivery = MESH_DELIVERY_PENDING; // Also exercise the relay-echo path.
    incoming(local_node_number, UINT32_MAX, broadcast_id, 2);
    assert(messages[1].delivery == MESH_DELIVERY_ACKNOWLEDGED);
    incoming(alice, local_node_number, 77, 3);
    incoming(alice, local_node_number, 77, 3); // Retransmission deduplicates.
    incoming(bob, local_node_number, 78, 3);
    incoming(bob, alice, 79, 3); // Text to another node does not enter our inbox.
    incoming(alice, UINT32_MAX, 80, 3);
    assert(message_count == 5);
    assert(mesh_radio_conversation_count(alice) == 2);
    assert(mesh_radio_conversation_count(bob) == 1);
    assert(mesh_radio_conversation_count(0) == 2);
    mesh_message_t result;
    assert(mesh_radio_get_conversation_message(alice, 1, &result));
    assert(!result.outgoing && strcmp(result.text, "hi") == 0);
    assert(!mesh_radio_get_conversation_message(alice, 2, &result));
    uint32_t peer;
    assert(mesh_radio_direct_peer_count() == 2);
    assert(mesh_radio_get_direct_peer(0, &peer) && peer == bob);
    assert(mesh_radio_get_direct_peer(1, &peer) && peer == alice);
    assert(mesh_radio_send_direct(bob, "test fail"));
    routing_from_radio(local_node_number, messages[message_count - 1].packet_id, 3);
    assert(messages[message_count - 1].delivery == MESH_DELIVERY_FAILED);
    assert(mesh_radio_send_direct(bob, "timeout"));
    fake_time += 91000000;
    mesh_radio_poll();
    assert(messages[message_count - 1].delivery == MESH_DELIVERY_FAILED);

    // Round trip recipient and direction, and turn stale pending sends into failures.
    history_fs_ready = true;
    assert(mesh_radio_send_direct(alice, "pending on reboot"));
    if (history_fs_ready) assert(mesh_radio_flush_history());
    size_t saved_count = message_count;
    memset(messages, 0, sizeof(messages)); message_count = 0;
    restore_message_history();
    assert(message_count == saved_count);
    assert(messages[0].peer_node == alice && messages[0].outgoing);
    assert(messages[2].peer_node == alice && !messages[2].outgoing);
    assert(messages[message_count - 1].delivery == MESH_DELIVERY_FAILED);
    remove(HISTORY_PATH);

    // Import v3 history without changing the original app's file.
    reset(); history_fs_ready = true; remove(HISTORY_BACKUP_PATH);
    stored_v3_message_history_t old = {.magic = MESSAGE_HISTORY_MAGIC, .version = 3, .count = 1};
    strcpy(old.messages[0].from, "You"); strcpy(old.messages[0].text, "old channel history");
    old.messages[0].delivery = MESH_DELIVERY_PENDING;
    FILE *f = fopen(LEGACY_HISTORY_PATH, "wb"); assert(f);
    assert(fwrite(&old, sizeof(old), 1, f) == 1); fclose(f);
    restore_message_history();
    assert(message_count == 1 && messages[0].peer_node == 0 && messages[0].outgoing);
    assert(messages[0].delivery == MESH_DELIVERY_FAILED);
    stored_v3_message_history_t unchanged;
    f = fopen(LEGACY_HISTORY_PATH, "rb"); assert(f);
    assert(fread(&unchanged, sizeof(unchanged), 1, f) == 1); fclose(f);
    assert(memcmp(&old, &unchanged, sizeof(old)) == 0);
    remove(HISTORY_PATH); remove(LEGACY_HISTORY_PATH);
    puts("PASS: DM addressing, ACKs, retries, conversations, timeouts, persistence, v3 import");
    return 0;
}
