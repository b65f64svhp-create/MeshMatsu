#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/message_search.c"

int main(void) {
    mesh_message_t message = {
        .from = "!aabbccdd", .text = "It's working in Greenview!",
        .timestamp = 123, .packet_id = 456, .delivery = MESH_DELIVERY_ACKNOWLEDGED,
        .peer_node = 0xaabbccdd, .outgoing = false, .channel = 1,
    };
    mesh_message_t before = message;
    assert(mesh_message_search_matches(&message, "Hilltop Base", "WORKING"));
    assert(mesh_message_search_matches(&message, "Hilltop Base", "it's"));
    assert(mesh_message_search_matches(&message, "Hilltop Base", "TOP ba"));
    assert(mesh_message_search_matches(&message, "Hilltop Base", "!AABBCCDD"));
    assert(!mesh_message_search_matches(&message, "Hilltop Base", "not received"));
    assert(!mesh_message_search_matches(&message, "Hilltop Base", "work in"));
    assert(mesh_message_search_matches(&message, "Hilltop Base", ""));
    assert(mesh_message_search_matches(&message, NULL, NULL));
    assert(!mesh_message_search_matches(NULL, "Hilltop Base", ""));
    assert(!memcmp(&before, &message, sizeof(message)));

    // Display names are resolved at search time; retained sender IDs stay useful.
    assert(!mesh_message_search_matches(&message, "Renamed Radio", "hilltop"));
    assert(mesh_message_search_matches(&message, "Renamed Radio", "renamed"));
    assert(mesh_message_search_matches(&message, NULL, "AABB"));
    strcpy(message.from, "legacy");
    assert(mesh_message_search_matches(&message, "", "LEGACY"));
    message.outgoing = true;
    assert(mesh_message_search_matches(&message, "You", "you"));
    assert(!mesh_message_search_matches(&message, NULL, "you"));

    strcpy(message.text, "It\xe2\x80\x99s working \xf0\x9f\x93\xa1 Caf\xc3\xa9");
    assert(mesh_message_search_matches(&message, "R\xc3\xa9p\xc3\xa9teur", "It\xe2\x80\x99s"));
    assert(mesh_message_search_matches(&message, NULL, "\xf0\x9f\x93\xa1"));
    assert(mesh_message_search_matches(&message, NULL, "CAF\xc3\xa9"));
    assert(mesh_message_search_matches(&message, "R\xc3\xa9p\xc3\xa9teur", "r\xc3\xa9p"));
    assert(!mesh_message_search_matches(&message, NULL, "CAF\xc3\x89"));
    assert(!mesh_message_search_matches(&message, NULL, "it's"));

    // Fields may fill their storage; neither search nor fallback reads past them.
    memset(&message, 0, sizeof(message));
    memset(message.text, 'x', sizeof(message.text));
    memcpy(message.text + sizeof(message.text) - 4, "TAIL", 4);
    memset(message.from, 'Y', sizeof(message.from));
    char sender[sizeof(((mesh_node_t *)0)->name)];
    memset(sender, 'z', sizeof(sender));
    memcpy(sender + sizeof(sender) - 4, "NAME", 4);
    assert(mesh_message_search_matches(&message, sender, "tail"));
    assert(mesh_message_search_matches(&message, sender, "name"));
    assert(mesh_message_search_matches(&message, sender, "yyyyyyyyyyyy"));
    assert(!mesh_message_search_matches(&message, sender, "yyyyyyyyyyyyy"));
    assert(!mesh_message_search_matches(&message, sender, "xxxxxxxxxxxxtailx"));
    message.text[3] = 0;
    assert(!mesh_message_search_matches(&message, NULL, "tail"));

    char query[MESH_MESSAGE_SEARCH_QUERY_MAX + 2];
    memset(message.text, 'q', sizeof(message.text));
    memset(query, 'Q', sizeof(query));
    query[MESH_MESSAGE_SEARCH_QUERY_MAX] = 0;
    assert(mesh_message_search_matches(&message, NULL, query));
    query[MESH_MESSAGE_SEARCH_QUERY_MAX] = 'Q';
    query[MESH_MESSAGE_SEARCH_QUERY_MAX + 1] = 0;
    assert(!mesh_message_search_matches(&message, NULL, query));
    // Oversized non-terminated input still stops at the fixed query bound.
    char oversized[MESH_MESSAGE_SEARCH_QUERY_MAX + 1];
    memset(oversized, 'Q', sizeof(oversized));
    assert(!mesh_message_search_matches(&message, NULL, oversized));
    memset(&message, 0, sizeof(message));
    assert(!mesh_message_search_matches(&message, NULL, "x"));

    puts("PASS: bounded message/name/ID search, ASCII case, exact UTF-8, empty and oversized queries, no mutation");
}
