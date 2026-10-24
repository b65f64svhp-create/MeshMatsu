// Snapshot results so new arrivals and evictions cannot move a selected message.
static EXT_RAM_BSS_ATTR mesh_message_t search_results[MESH_MESSAGE_HISTORY_MAX];
static char search_query[MESH_MESSAGE_SEARCH_QUERY_MAX + 1], search_feedback[80];
static size_t search_count, search_selection;
static uint32_t search_peer;
static uint8_t search_channel;
static bool search_all;
static bool search_same_message(const mesh_message_t *a, const mesh_message_t *b) {
    return a->peer_node == b->peer_node && a->channel == b->channel && a->outgoing == b->outgoing &&
        a->packet_id == b->packet_id && a->timestamp == b->timestamp &&
        !strcmp(a->from, b->from) && !strcmp(a->text, b->text);
}
static void refresh_message_search(void) {
    search_count = search_selection = 0;
    size_t count = search_all ? mesh_radio_message_count() : search_peer ?
        mesh_radio_conversation_count(search_peer) : mesh_radio_channel_message_count(search_channel);
    for (size_t i = count; i && search_count < MESH_MESSAGE_HISTORY_MAX; i--) {
        mesh_message_t message;
        bool found = search_all ? mesh_radio_get_message(i - 1, &message) : search_peer ?
            mesh_radio_get_conversation_message(search_peer, i - 1, &message) :
            mesh_radio_get_channel_message(search_channel, i - 1, &message);
        char name[40];
        if (!found) continue;
        mesh_radio_message_sender_name(&message, name, sizeof(name));
        if (mesh_message_search_matches(&message, name, search_query)) search_results[search_count++] = message;
    }
    search_feedback[0] = '\0';
}
static void open_message_search(void) {
    search_peer = message_peer; search_channel = message_channel; search_all = false;
    search_query[0] = '\0'; chat_actions.modal = 0;
    refresh_message_search(); current_screen = SCREEN_MESSAGE_SEARCH;
}
static void open_search_result(void) {
    if (search_selection >= search_count) return;
    const mesh_message_t *target = &search_results[search_selection];
    size_t count = target->peer_node ? mesh_radio_conversation_count(target->peer_node) :
        mesh_radio_channel_message_count(target->channel);
    for (size_t i = 0; i < count; i++) {
        mesh_message_t message;
        bool found = target->peer_node ? mesh_radio_get_conversation_message(target->peer_node, i, &message) :
            mesh_radio_get_channel_message(target->channel, i, &message);
        if (found && search_same_message(&message, target)) {
            open_thread(target->peer_node, target->channel);
            message_scroll_offset = count - 1 - i;
            mesh_message_capture_anchor(message_peer,message_channel,message_scroll_offset,&message_anchor);
            return;
        }
    }
    snprintf(search_feedback, sizeof(search_feedback), "Message no longer retained. Tab refreshes results.");
}
static bool message_search_navigation(int key) {
    if (current_screen != SCREEN_MESSAGE_SEARCH) return false;
    if (key == BSP_INPUT_NAVIGATION_KEY_UP) { if (search_selection) search_selection--; }
    else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) { if (search_selection + 1 < search_count) search_selection++; }
    else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) open_search_result();
    else return false;
    return true;
}
static bool message_search_key(char key) {
    if (current_screen != SCREEN_MESSAGE_SEARCH) return false;
    if (key == '\r' || key == '\n') open_search_result();
    else if (key == '\t') { search_all = !search_all; refresh_message_search(); }
    else if (key == '\b' || key == 0x7f) { mesh_compose_backspace(search_query); refresh_message_search(); }
    else if ((unsigned char)key >= 32) {
        size_t n = strlen(search_query);
        if (n < MESH_MESSAGE_SEARCH_QUERY_MAX) { search_query[n] = key; search_query[n + 1] = '\0'; refresh_message_search(); }
        else snprintf(search_feedback, sizeof(search_feedback), "Search limit: 48 bytes.");
    }
    return true;
}
