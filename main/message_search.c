#include "message_search.h"

static size_t bounded_length(const char *text, size_t limit) {
    size_t length = 0;
    if (text) while (length < limit && text[length]) ++length;
    return length;
}

static unsigned char ascii_lower(unsigned char byte) {
    return byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte;
}

static bool contains_query(const char *text, size_t capacity,
                           const char *query, size_t query_length) {
    size_t length = bounded_length(text, capacity);
    if (query_length > length) return false;
    for (size_t start = 0; start <= length - query_length; ++start) {
        size_t offset = 0;
        while (offset < query_length &&
               ascii_lower((unsigned char)text[start + offset]) ==
               ascii_lower((unsigned char)query[offset])) ++offset;
        if (offset == query_length) return true;
    }
    return false;
}

bool mesh_message_search_matches(const mesh_message_t *message,
                                 const char *resolved_sender,
                                 const char *query) {
    if (!message) return false;
    size_t length = bounded_length(query, MESH_MESSAGE_SEARCH_QUERY_MAX + 1);
    if (!length) return true;
    if (length > MESH_MESSAGE_SEARCH_QUERY_MAX) return false;
    return contains_query(message->text, sizeof(message->text), query, length) ||
           contains_query(resolved_sender, sizeof(((mesh_node_t *)0)->name), query, length) ||
           contains_query(message->from, sizeof(message->from), query, length);
}
