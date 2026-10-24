#define main message_tests
#include "test_direct_messages.c"
#undef main

static void channel_packet(uint8_t channel, uint32_t sender, uint32_t id) {
    uint8_t packet[] = {0x0d,0,0,0,0, 0x15,0xff,0xff,0xff,0xff,
                        0x22,6, 0x08,1,0x12,2,'h','i',
                        0x35,0,0,0,0, 0x18,channel};
    memcpy(packet + 1, &sender, 4); memcpy(packet + 19, &id, 4);
    decode_mesh_packet(packet, sizeof(packet));
}

#ifndef CHANNEL_TEST_MAIN
#define CHANNEL_TEST_MAIN main
#endif
int CHANNEL_TEST_MAIN(void) {
    message_tests();
    reset();
    memset(channels, 0, sizeof(channels));
    const uint8_t primary[] = {8, 0, 24, 1};
    const uint8_t secondary[] = {8, 1, 24, 2};
    decode_channel(primary, sizeof(primary)); decode_channel(secondary, sizeof(secondary));
    assert(!mesh_radio_send_channel(8, "bad"));
    assert(!mesh_radio_send_channel(2, "disabled"));
    assert(mesh_radio_send_channel(0, "primary"));
    assert(mesh_radio_send_channel(1, "secondary"));
    assert(sent_bytes[4] == 0x0a && sent_bytes[6] == 0x15);
    assert(memcmp(sent_bytes + 7, "\xff\xff\xff\xff", 4) == 0);
    // Actual wire field 3 (channel) precedes the decoded message in secondary sends.
    assert(sent_bytes[11] == 0x18 && sent_bytes[12] == 1 && sent_bytes[13] == 0x22);
    assert(messages[0].channel == 0 && messages[1].channel == 1);
    uint32_t id = messages[1].packet_id;
    routing_from_radio(local_node_number, id, 0);
    assert(messages[1].delivery == MESH_DELIVERY_ACKNOWLEDGED);
    assert(messages[0].delivery == MESH_DELIVERY_PENDING);
    channel_packet(1, 0x11223344, 123);
    channel_packet(1, 0x11223344, 123); // Same-channel retransmission deduplicates.
    channel_packet(0, 0x11223344, 123); // Same ID/sender on another channel stays distinct.
    channel_packet(8, 0x11223344, 124); // Invalid decoded channel ignored.
    assert(mesh_radio_channel_message_count(0) == 2);
    assert(mesh_radio_channel_message_count(1) == 2);
    assert(mesh_radio_conversation_count(0) == 2); // Old API remains primary-only.
    mesh_message_t message;
    assert(mesh_radio_get_channel_message(1, 1, &message) && !message.outgoing && message.channel == 1);
    assert(!mesh_radio_get_channel_message(0, 2, &message));
    assert(mesh_radio_send_direct(0x11223344, "dm"));
    assert(mesh_radio_conversation_count(0x11223344) == 1);
    assert(mesh_radio_channel_message_count(0) == 2 && mesh_radio_channel_message_count(1) == 2);
    history_fs_ready = true;
    persist_message_history(); if (history_fs_ready) assert(mesh_radio_flush_history());
    memset(messages, 0, sizeof(messages)); message_count = 0;
    restore_message_history();
    assert(mesh_radio_channel_message_count(0) == 2 && mesh_radio_channel_message_count(1) == 2);
    assert(mesh_radio_conversation_count(0x11223344) == 1);
    assert(messages[1].channel == 1 && messages[1].delivery == MESH_DELIVERY_ACKNOWLEDGED);
    remove(HISTORY_PATH);

    // v4 had no channel identity; retain channel messages in primary and DMs by peer.
    stored_v4_message_history_t old = {.magic = MESSAGE_HISTORY_MAGIC, .version = 4, .count = 2};
    strcpy(old.messages[0].from, "You"); strcpy(old.messages[0].text, "legacy channel");
    old.messages[0].outgoing = true; old.messages[0].delivery = MESH_DELIVERY_ACKNOWLEDGED;
    strcpy(old.messages[1].from, "You"); strcpy(old.messages[1].text, "legacy dm");
    old.messages[1].peer_node = 0x11223344; old.messages[1].outgoing = true;
    old.messages[1].delivery = MESH_DELIVERY_PENDING;
    FILE *file = fopen(HISTORY_PATH, "wb"); assert(file);
    assert(fwrite(&old, sizeof(old), 1, file) == 1); fclose(file);
    memset(messages, 0, sizeof(messages)); message_count = 0;
    restore_message_history();
    assert(message_count == 2 && mesh_radio_channel_message_count(0) == 1);
    assert(mesh_radio_channel_message_count(1) == 0 && mesh_radio_conversation_count(0x11223344) == 1);
    assert(messages[1].delivery == MESH_DELIVERY_FAILED && messages[1].outgoing);
    remove(HISTORY_PATH);
    puts("PASS: secondary wire channel, isolated receive/history, ACK correlation, v5 roundtrip and v4 migration");
    return 0;
}
