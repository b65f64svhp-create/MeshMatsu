#define CHANNEL_TEST_MAIN channel_tests
#include "test_channel_messaging.c"

static void feed_radio(const uint8_t *payload, size_t size) {
    assert(size + 4 <= sizeof(received_bytes));
    received_bytes[0] = 0x94; received_bytes[1] = 0xc3;
    received_bytes[2] = size >> 8; received_bytes[3] = size;
    memcpy(received_bytes + 4, payload, size);
    received_size = size + 4; received_offset = 0;
    do { mesh_radio_poll(); } while (received_offset < received_size);
}

static void complete_sync(uint32_t nonce) {
    uint8_t payload[8] = {0x38}; size_t size = 1;
    assert(append_varint(payload, sizeof(payload), &size, nonce));
    feed_radio(payload, size);
}

static void burst_and_revision_tests(void) {
    reset(); received_size=received_offset=0;
    uint32_t before=mesh_radio_history_revision();
    for(uint32_t id=1;id<=40;id++) {
        uint32_t sender=0x11223344;
        uint8_t packet[]={0x0d,0,0,0,0,0x15,0xff,0xff,0xff,0xff,
            0x22,6,0x08,1,0x12,2,'h','i',0x35,0,0,0,0,0x18,1};
        memcpy(packet+1,&sender,4);memcpy(packet+19,&id,4);
        uint8_t body[64];size_t length=0;
        assert(append_bytes_field(body,sizeof(body),&length,2,packet,sizeof(packet)));
        assert(received_size+length+4<=sizeof(received_bytes));
        received_bytes[received_size++]=0x94;received_bytes[received_size++]=0xc3;
        received_bytes[received_size++]=length>>8;received_bytes[received_size++]=length;
        memcpy(received_bytes+received_size,body,length);received_size+=length;
    }
    assert(received_size>1024);
    mesh_radio_poll();
    assert(received_offset==received_size && message_count==40 && mesh_radio_unread_channel(1)==40);
    assert(mesh_radio_history_revision()!=before);
    assert(mesh_radio_send_direct(0x11223344,"timeout"));
    before=mesh_radio_history_revision();fake_time+=90000000;mesh_radio_poll();
    assert(messages[message_count-1].delivery==MESH_DELIVERY_FAILED && mesh_radio_history_revision()!=before);
}

static void unread_tests(void) {
    reset();
    const uint32_t alice = 0x11223344, bob = 0xaabbccdd;
    channel_packet(0, alice, 101);
    channel_packet(1, alice, 102);
    channel_packet(1, alice, 102); // Retransmission does not increment.
    incoming(alice, local_node_number, 103, 3);
    incoming(alice, local_node_number, 103, 3);
    incoming(bob, local_node_number, 104, 3);
    incoming(bob, alice, 105, 3); // Another node's DM is not ours.
    assert(mesh_radio_unread_total(false) == 2 && mesh_radio_unread_total(true) == 2);
    assert(mesh_radio_unread_channel(0) == 1 && mesh_radio_unread_channel(1) == 1);
    assert(mesh_radio_unread_direct(alice) == 1 && mesh_radio_unread_direct(bob) == 1);
    assert(mesh_radio_unread_channel(8) == 0 && mesh_radio_unread_direct(0) == 0);
    assert(mesh_radio_send_direct(alice, "outgoing"));
    routing_from_radio(alice, messages[message_count - 1].packet_id, 0);
    assert(mesh_radio_unread_total(true) == 2);
    mesh_radio_mark_read(alice, 0);
    assert(mesh_radio_unread_direct(alice) == 0 && mesh_radio_unread_direct(bob) == 1);
    mesh_radio_mark_read(0, 0);
    assert(mesh_radio_unread_channel(0) == 0 && mesh_radio_unread_channel(1) == 1);
    channel_packet(1, alice, 102); // A read message replay stays read.
    assert(mesh_radio_unread_channel(1) == 1);

    history_fs_ready = true;
    persist_message_history(); if (history_fs_ready) assert(mesh_radio_flush_history());
    size_t count = message_count;
    memset(messages, 0, sizeof(messages)); memset(message_unread, 0, sizeof(message_unread)); message_count = 0;
    restore_message_history();
    assert(message_count == count);
    assert(mesh_radio_unread_total(true) == 1 && mesh_radio_unread_total(false) == 1);
    mesh_radio_mark_read(0, 1); if (history_fs_ready) assert(mesh_radio_flush_history());
    restore_message_history();
    assert(mesh_radio_unread_channel(1) == 0 && mesh_radio_unread_direct(bob) == 1);
    remove(HISTORY_PATH);

    // Existing v5 history migrates as read, retaining channel/peer and ACKs.
    stored_v5_message_history_t old = {.magic = MESSAGE_HISTORY_MAGIC, .version = 5, .count = 2};
    old.messages[0] = (v8_mesh_message_t){.peer_node = alice, .packet_id = 123, .outgoing = false};
    strcpy(old.messages[0].from, "Alice"); strcpy(old.messages[0].text, "previous message");
    old.messages[1] = (v8_mesh_message_t){.channel = 1, .outgoing = true, .delivery = MESH_DELIVERY_ACKNOWLEDGED};
    FILE *file = fopen(HISTORY_PATH, "wb"); assert(file);
    assert(fwrite(&old, sizeof(old), 1, file) == 1); fclose(file);
    restore_message_history();
    assert(message_count == 2 && messages[0].peer_node == alice && messages[1].channel == 1);
    assert(messages[1].delivery == MESH_DELIVERY_ACKNOWLEDGED);
    assert(mesh_radio_unread_total(false) == 0 && mesh_radio_unread_total(true) == 0);
    incoming(bob, local_node_number, 999, 3); if (history_fs_ready) assert(mesh_radio_flush_history());
    restore_message_history();
    assert(mesh_radio_unread_direct(bob) == 1);
    remove(HISTORY_PATH);

    reset();
    for (uint32_t i = 0; i < 510; i++) channel_packet(i % 2, alice, i + 1);
    assert(message_count == 500 && mesh_radio_unread_total(false) == 500);
    assert(mesh_radio_unread_channel(0) == 250 && mesh_radio_unread_channel(1) == 250);
    mesh_radio_mark_read(0, 1);
    assert(mesh_radio_unread_total(false) == 250);
    // Eviction shifts unread state with its message, including read entries.
    channel_packet(1, alice, 511);
    assert(mesh_radio_unread_channel(0) == 250 && mesh_radio_unread_channel(1) == 1);
}

static void reconnect_tests(void) {
    reset();
    lora_config.loaded = true;
    channel_packet(1, 0x11223344, 22);
    assert(mesh_radio_send_direct(0x11223344, "pending"));
    uint32_t pending_id = messages[message_count - 1].packet_id;
    fake_time += 20000000;
    mesh_radio_poll(); // Quiet radio gets a local owner query before any loss.
    assert(probe_started_us && status.synchronized && !status.reconnecting);
    assert(message_count == 2 && mesh_radio_unread_channel(1) == 1);
    // A local API response proves the connection is alive without mesh traffic.
    const uint8_t my_info[] = {0x1a, 2, 0x08, 1};
    feed_radio(my_info, sizeof(my_info));
    assert(!probe_started_us && status.synchronized && local_node_number == 1);
    size_t writes = write_count;
    fake_time += 19000000; mesh_radio_poll();
    assert(write_count == writes + 1 && !probe_started_us);
    assert(sent_size == 6 && sent_bytes[4] == 0x3a && sent_bytes[5] == 0); // Local keepalive only.
    fake_time += 1000000; mesh_radio_poll();
    assert(probe_started_us);
    session_passkey_size = 3; memset(session_passkey, 0x55, 3);
    channels[1].info.loaded = true; channels[1].info.revision = 7;
    fake_time += 10000000; mesh_radio_poll();
    assert(status.reconnecting && !status.synchronized && status.reconnect_attempts == 1);
    assert(!session_passkey_size && !channels[1].info.loaded && channels[1].info.revision == 7);
    assert(!mesh_radio_send_direct(0x11223344, "must not send"));
    assert(!mesh_radio_set_channel(1, "name", true, false));
    assert(message_count == 2 && messages[1].packet_id == pending_id);
    assert(mesh_radio_unread_channel(1) == 1);
    // The only automatic write on loss is ToRadio.want_config_id, not text.
    assert(sent_bytes[4] == 0x18);
    uint32_t first_nonce = config_nonce;
    size_t attempts = status.reconnect_attempts;
    fake_time = next_config_retry_us - 1; mesh_radio_poll();
    assert(status.reconnect_attempts == attempts);
    fake_time++; mesh_radio_poll();
    assert(status.reconnect_attempts == attempts + 1 && config_nonce != first_nonce);
    complete_sync(first_nonce);
    assert(!status.synchronized && status.reconnecting); // Stale completion ignored.
    uint32_t current_nonce = config_nonce;
    complete_sync(current_nonce);
    assert(status.synchronized && !status.reconnecting);
    assert(mesh_radio_unread_channel(1) == 1 && message_count == 2);
    // The original message can still receive its ACK after recovery.
    routing_from_radio(0x11223344, pending_id, 0);
    assert(messages[1].delivery == MESH_DELIVERY_ACKNOWLEDGED);
    const uint8_t rebooted[] = {0x40, 1};
    feed_radio(rebooted, sizeof(rebooted));
    assert(status.reconnecting && !status.synchronized && config_nonce != current_nonce);
    complete_sync(current_nonce);
    assert(status.reconnecting);
    complete_sync(config_nonce);
    assert(status.synchronized);

    // Garbage and truncated frames cannot keep a missing radio "online".
    fake_time += 20000000; mesh_radio_poll(); assert(probe_started_us);
    const uint8_t malformed[] = {0x12, 0xff};
    feed_radio(malformed, sizeof(malformed)); assert(probe_started_us);
    received_bytes[0] = 0x94; received_bytes[1] = 0xc3; received_bytes[2] = 1; received_bytes[3] = 200;
    received_size = 4; received_offset = 0; mesh_radio_poll(); assert(rx_size == 4);
    fake_time += 3000000; mesh_radio_poll(); assert(rx_size == 0);
    fake_time += 7000000; mesh_radio_poll(); assert(status.reconnecting);
    // Retries slow down to at most one config request every 30 seconds.
    for (int i = 0; i < 5; i++) { fake_time = next_config_retry_us; mesh_radio_poll(); }
    assert(config_retry_delay_us == 30000000 && next_config_retry_us - fake_time == 30000000);
    complete_sync(config_nonce); assert(status.synchronized);
    read_error = true; mesh_radio_poll(); assert(status.reconnecting);
    complete_sync(config_nonce); assert(status.synchronized);

    // Even initial power/UART setup failure retries without clearing history.
    reset(); uart_installed = false; power_error = 2;
    assert(!mesh_radio_start() && !status.started && status.reconnecting);
    channel_packet(0, 0x11223344, 123);
    if (history_fs_ready) assert(mesh_radio_flush_history());
    size_t saved_count = message_count;
    fake_time = next_config_retry_us; mesh_radio_poll(); assert(!status.started);
    power_error = 0; uart_config_error = 2;
    fake_time = next_config_retry_us; mesh_radio_poll(); assert(!status.started);
    uart_config_error = 0;
    int installs = driver_installs;
    fake_time = next_config_retry_us; mesh_radio_poll();
    assert(status.started && config_request_pending && driver_installs == installs + 1);
    assert(message_count == saved_count && mesh_radio_unread_channel(0) == 1);
    complete_sync(config_nonce); assert(status.synchronized && !status.reconnecting);
}

int main(void) {
    channel_tests();
    burst_and_revision_tests();
    unread_tests();
    reconnect_tests();
    puts("PASS: unread isolation/dedup/read/persistence/v5 migration/eviction; quiet-link probes, loss/reboot/UART recovery, nonce validation, retry backoff, no text resend");
}
