// App-only notification preferences; radio ignore is a separate verified admin operation.
#ifndef NODE_PREFS_PATH
#define NODE_PREFS_PATH "/locfd/meshdeck-mui-node-prefs.bin"
#endif
#ifndef NODE_PREFS_TEMP_PATH
#define NODE_PREFS_TEMP_PATH NODE_PREFS_PATH ".tmp"
#endif
static struct {
    uint32_t magic, count;
    uint32_t muted[MESH_NODE_LIMIT];
    uint32_t checksum;
} node_preferences;
static bool read_node_preferences(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    __typeof__(node_preferences) loaded;
    bool ok = fread(&loaded, sizeof(loaded), 1, file) == 1 && fgetc(file) == EOF;
    fclose(file);
    ok = ok && loaded.magic == 0x4d4e5031 && loaded.count <= MESH_NODE_LIMIT &&
        loaded.checksum == history_checksum(2166136261U, &loaded, offsetof(__typeof__(loaded), checksum));
    for (size_t i = 0; ok && i < loaded.count; i++) {
        if (!loaded.muted[i] || loaded.muted[i] == MESH_BROADCAST_NODE) ok = false;
        for (size_t j = 0; j < i; j++) if (loaded.muted[j] == loaded.muted[i]) ok = false;
    }
    if (ok) node_preferences = loaded;
    return ok;
}
static void load_node_preferences(void) {
    memset(&node_preferences, 0, sizeof(node_preferences));
    if (history_fs_ready && !read_node_preferences(NODE_PREFS_PATH)) read_node_preferences(NODE_PREFS_PATH ".bak");
}
bool mesh_radio_node_muted(uint32_t number) {
    for (size_t i = 0; i < node_preferences.count; i++) if (node_preferences.muted[i] == number) return true;
    return false;
}
bool mesh_radio_set_node_muted(uint32_t number, bool muted) {
    if (!number || number == MESH_BROADCAST_NODE || number == local_node_number || !history_fs_ready) return false;
    if (mesh_radio_node_muted(number) == muted) return true;
    __typeof__(node_preferences) next = node_preferences;
    if (muted) {
        if (next.count == MESH_NODE_LIMIT) return false;
        next.muted[next.count++] = number;
    } else for (size_t i = 0; i < next.count; i++) if (next.muted[i] == number) {
        memmove(&next.muted[i], &next.muted[i+1], (next.count-i-1)*sizeof(next.muted[0]));
        next.muted[--next.count] = 0; break;
    }
    next.magic = 0x4d4e5031;
    next.checksum = history_checksum(2166136261U, &next, offsetof(__typeof__(next), checksum));
    FILE *file = fopen(NODE_PREFS_TEMP_PATH, "wb");
    if (!file) return false;
    bool ok = fwrite(&next, sizeof(next), 1, file) == 1 && fflush(file) == 0 && fsync(fileno(file)) == 0;
    if (fclose(file)) ok = false;
    // FAT rename cannot replace an existing file. Keep a recoverable backup.
    const char *backup = NODE_PREFS_PATH ".bak";
    bool backed_up = false;
    if (ok && access(NODE_PREFS_PATH, F_OK) == 0) {
        remove(backup); backed_up = rename(NODE_PREFS_PATH, backup) == 0; ok = backed_up;
    }
    if (ok) ok = rename(NODE_PREFS_TEMP_PATH, NODE_PREFS_PATH) == 0;
    if (!ok) { if (backed_up) rename(backup, NODE_PREFS_PATH); remove(NODE_PREFS_TEMP_PATH); return false; }
    node_preferences = next; history_revision++; return true;
}
static uint32_t message_sender_number(const mesh_message_t *message) {
    if (message->outgoing) return local_node_number;
    if (message->peer_node) return message->peer_node;
    if (message->from[0] != '!' || message->from[9]) return 0;
    uint32_t number = 0;
    for (unsigned i = 1; i <= 8; i++) {
        unsigned char c = message->from[i]; unsigned nibble;
        if (c >= '0' && c <= '9') nibble = c-'0';
        else if (c >= 'a' && c <= 'f') nibble = c-'a'+10;
        else if (c >= 'A' && c <= 'F') nibble = c-'A'+10;
        else return 0;
        number = (number << 4) | nibble;
    }
    return number;
}
bool mesh_radio_alert_unread(bool direct, uint8_t channel) {
    for (size_t i = 0; i < message_count; i++) {
        const mesh_message_t *m = &messages[i];
        if (!message_unread[i] || m->outgoing || (direct ? !m->peer_node : m->peer_node || m->channel != channel)) continue;
        uint32_t sender = message_sender_number(m);
        if (mesh_radio_node_muted(sender)) continue;
        bool ignored = false;
        for (size_t n = 0; n < node_count; n++) if (nodes[n].number == sender) { ignored = nodes[n].is_ignored; break; }
        if (!ignored) return true;
    }
    return false;
}
