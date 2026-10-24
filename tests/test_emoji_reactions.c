#define CHANNEL_TEST_MAIN channel_tests
#include "test_channel_messaging.c"
#include "../main/emoji_text.c"

static void reaction_packet(uint32_t sender, uint32_t destination, uint8_t channel,
                            uint32_t id, uint32_t target, const char *emoji, bool reaction, uint8_t port) {
    uint8_t data[256], packet[320]; size_t d = 0, p = 0;
    append_varint(data, sizeof(data), &d, 8); append_varint(data, sizeof(data), &d, port);
    append_bytes_field(data, sizeof(data), &d, 2, (const uint8_t *)emoji, strlen(emoji));
    data[d++] = 0x3d; memcpy(data + d, &target, 4); d += 4;
    uint32_t flag = reaction;
    data[d++] = 0x45; memcpy(data + d, &flag, 4); d += 4;
    packet[p++] = 0x0d; memcpy(packet + p, &sender, 4); p += 4;
    packet[p++] = 0x15; memcpy(packet + p, &destination, 4); p += 4;
    append_varint(packet, sizeof(packet), &p, 24); append_varint(packet, sizeof(packet), &p, channel);
    append_bytes_field(packet, sizeof(packet), &p, 4, data, d);
    packet[p++] = 0x35; memcpy(packet + p, &id, 4); p += 4;
    decode_mesh_packet(packet, p);
}

static unsigned drawn_glyphs, drawn_tokens;
static void check_layout(const mesh_text_token_t *token, int x, int y, void *context) {
    int width = *(int *)context;
    assert(x >= 0 && x + token->width <= width && y >= 0 && y % 22 == 0);
    if (token->glyph >= 0) drawn_glyphs++;
    drawn_tokens++;
}

static char rendered_text[256];
static size_t rendered_size;
static void collect_text(const mesh_text_token_t *token, int x, int y, void *context) {
    assert(token->glyph < 0);
    size_t size = strlen(token->text);
    assert(rendered_size + size < sizeof(rendered_text));
    memcpy(rendered_text + rendered_size, token->text, size + 1);
    rendered_size += size;
}

static void punctuation_tests(void) {
    mesh_text_token_t apostrophe = mesh_text_next("’", 10);
    assert(apostrophe.bytes == 3 && apostrophe.width == 10 && !strcmp(apostrophe.text, "'"));
    const char *sample = "it’s working";
    const char *variants[] = {"it's working", "it’s working", "it‘s working"};
    for (size_t i = 0; i < sizeof(variants)/sizeof(variants[0]); i++) {
        rendered_size = 0; rendered_text[0] = 0;
        assert(mesh_text_layout(variants[i], 120, 10, 22, 3, collect_text, NULL) == 1);
        assert(!strcmp(rendered_text, "it's working"));
    }
    rendered_size = 0; rendered_text[0] = 0;
    mesh_text_layout("“Yes”—it’s working…", 400, 10, 22, 3, collect_text, NULL);
    assert(!strcmp(rendered_text, "\"Yes\"-it's working..."));
    assert(mesh_text_next("…", 10).width == 30);
    // Received bytes and history stay intact; only the font representation changes.
    reset();
    reaction_packet(0x11223344, local_node_number, 0, 800, 0, sample, false, 1);
    reaction_packet(0x11223344, UINT32_MAX, 0, 801, 0, sample, false, 1);
    assert(message_count == 2);
    history_fs_ready = true; persist_message_history(); if (history_fs_ready) assert(mesh_radio_flush_history());
    memset(messages, 0, sizeof(messages)); message_count = 0;
    restore_message_history();
    assert(message_count == 2);
    for(size_t i = 0; i < message_count; i++) {
        assert(!strcmp(messages[i].text, sample));
        rendered_size = 0; rendered_text[0] = 0;
        mesh_text_layout(messages[i].text, 120, 10, 22, 3, collect_text, NULL);
        assert(!strcmp(rendered_text, "it's working"));
    }
    remove(HISTORY_PATH);
}

static void font_tests(void) {
    const char *samples[] = {"👍", "👎", "❤️", "😂", "‼️", "❓", "👍🏽", "🇨🇦", "👨‍👩‍👧‍👦", "1️⃣", "🏳️‍🌈"};
    for (size_t i = 0; i < sizeof(samples)/sizeof(samples[0]); i++) {
        mesh_text_token_t token = mesh_text_next(samples[i], 9);
        assert(token.glyph >= 0 && token.bytes == strlen(samples[i]));
        size_t visible = 0;
        for (unsigned y = 0; y < 20; y++) for (unsigned x = 0; x < 20; x++) if (mesh_emoji_alpha(token.glyph, x, y)) visible++;
        assert(visible > 20);
    }
    assert(mesh_text_next("❤", 9).glyph == mesh_text_next("❤️", 9).glyph);
    assert(mesh_text_next("é", 9).bytes == 2 && mesh_text_next("é", 9).glyph == -1);
    assert(strstr(mesh_text_next("\xf4\x8f\xbf\xbf", 9).text, "U+10FFFF"));
    const char *invalid[] = {"\xff", "\xc0\x80", "\xf0\x9f", "\xed\xa0\x80", "\xf4\x90\x80\x80"};
    for (size_t i = 0; i < sizeof(invalid)/sizeof(invalid[0]); i++) {
        size_t pos = 0;
        while (invalid[i][pos]) { mesh_text_token_t token = mesh_text_next(invalid[i] + pos, 9); assert(token.bytes); pos += token.bytes; }
    }
    int width = 100;
    drawn_glyphs = drawn_tokens = 0;
    const char *message = "Hi 👍🏽 world ❤️\nFlag 🇨🇦 family 👨‍👩‍👧‍👦";
    int lines = mesh_text_layout(message, width, 9, 22, 50, check_layout, &width);
    assert(lines == mesh_text_line_count(message, width, 9));
    assert(drawn_glyphs == 4 && drawn_tokens > 10);
    width = 22;
    mesh_text_layout("👍👍👍", width, 9, 22, 3, check_layout, &width);
    assert(mesh_text_line_count("👍👍👍", width, 9) == 3);
    mesh_text_layout("\xf4\x8f\xbf\xbf", width, 9, 22, 3, check_layout, &width);
    assert(mesh_text_layout("a\nb", 100, 9, 22, 1, NULL, NULL) == 1);
}

#ifndef EMOJI_TEST_MAIN
#define EMOJI_TEST_MAIN main
#endif
int EMOJI_TEST_MAIN(void) {
    channel_tests(); font_tests(); punctuation_tests(); reset();
    const uint32_t alice = 0x11223344, bob = 0xaabbccdd;
    assert(mesh_radio_send_text("React to this"));
    mesh_message_t target = messages[0];
    reaction_packet(alice, UINT32_MAX, 0, 200, target.packet_id, "👍", true, 1);
    assert(message_count == 1 && reaction_count == 1 && mesh_radio_reaction_count(&target) == 1);
    assert(mesh_radio_unread_total(false) == 0); // Tapbacks aren't extra text/unread messages.
    reaction_packet(alice, UINT32_MAX, 0, 200, target.packet_id, "👍", true, 1);
    reaction_packet(alice, UINT32_MAX, 0, 201, target.packet_id, "👍", true, 1);
    assert(reaction_count == 1); // Packet replay and same-sender repeat deduplicate.
    reaction_packet(bob, UINT32_MAX, 0, 202, target.packet_id, "❤️", true, 1);
    reaction_packet(alice, UINT32_MAX, 1, 203, target.packet_id, "👎", true, 1);
    assert(mesh_radio_reaction_count(&target) == 2); // Same ID on another channel doesn't attach.
    mesh_reaction_t found;
    assert(mesh_radio_get_reaction(&target, 0, &found) && found.sender == alice && !strcmp(found.emoji, "👍"));
    assert(!mesh_radio_get_reaction(&target, 2, &found));
    reaction_packet(alice, bob, 0, 204, target.packet_id, "👍", true, 1);
    assert(reaction_count == 3); // Other people's direct reactions are ignored.
    reaction_packet(alice, UINT32_MAX, 0, 205, 0, "👍", true, 1);
    assert(reaction_count == 3); // A tapback without a target cannot become plain text.
    reaction_packet(alice, UINT32_MAX, 0, 206, target.packet_id, "A normal reply", false, 1);
    assert(message_count == 2 && !strcmp(messages[1].text, "A normal reply"));
    assert(mesh_radio_unread_channel(0) == 1);
    assert(mesh_radio_send_direct(alice, "DM target"));
    mesh_message_t dm = messages[2];
    reaction_packet(alice, local_node_number, 0, 207, dm.packet_id, "😂", true, 1);
    reaction_packet(bob, local_node_number, 0, 208, dm.packet_id, "👍", true, 1);
    assert(mesh_radio_reaction_count(&dm) == 1); // Correct peer only.
    reaction_packet(alice, local_node_number, 0, 209, dm.packet_id, "🔥", true, 32);
    assert(mesh_radio_reaction_count(&dm) == 2); // Also accept flagged REPLY_APP reactions.

    history_fs_ready = true; persist_message_history(); if (history_fs_ready) assert(mesh_radio_flush_history());
    memset(reactions, 0, sizeof(reactions)); reaction_count = 0;
    memset(messages, 0, sizeof(messages)); message_count = 0;
    restore_message_history();
    assert(message_count == 3 && mesh_radio_reaction_count(&target) == 2 && mesh_radio_reaction_count(&dm) == 2);
    assert(mesh_radio_unread_channel(0) == 1);
    remove(HISTORY_PATH);
    stored_v6_message_history_t old = {.magic = MESSAGE_HISTORY_MAGIC, .version = 6, .count = 1, .unread_mask = 1};
    old.messages[0] = (v8_mesh_message_t){.packet_id = 900, .channel = 1}; strcpy(old.messages[0].text, "v12 message");
    FILE *file = fopen(HISTORY_PATH, "wb"); assert(file);
    assert(fwrite(&old, sizeof(old), 1, file) == 1); fclose(file);
    restore_message_history();
    assert(message_count == 1 && mesh_radio_unread_channel(1) == 1 && reaction_count == 0);
    // Reaction may arrive before its parent; it attaches when the message arrives.
    reaction_packet(alice, UINT32_MAX, 1, 300, 901, "🙏", true, 1);
    channel_packet(1, alice, 901);
    assert(mesh_radio_reaction_count(&messages[1]) == 1);
    for (uint32_t i = 0; i < REACTION_HISTORY_LIMIT+20; i++) reaction_packet(alice, UINT32_MAX, 1, 1000 + i, 2000 + i, "👍", true, 1);
    assert(reaction_count == REACTION_HISTORY_LIMIT);
    remove(HISTORY_PATH);
    puts("PASS: smart punctuation receive/render/history, emoji sequences, UTF-8 safety/wrapping, tapback target/channel/peer isolation, dedup, v7 persistence/v6 unread migration, orphan attachment and reaction bound");
    return 0;
}
