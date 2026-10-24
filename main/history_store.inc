// Included by radio_link.c: all history operations run on the app/radio task.
#ifndef HISTORY_BACKUP_PATH
#define HISTORY_BACKUP_PATH HISTORY_PATH ".bak"
#endif
#define CURRENT_HISTORY_VERSION 9
static mesh_history_archive_fn history_archive;
void mesh_radio_set_history_archive(mesh_history_archive_fn callback) { history_archive=callback; }
static void archive_history_message(size_t index) {
    if(!history_archive)return;
    char sender[40];mesh_radio_message_sender_name(&messages[index],sender,sizeof(sender));
    history_archive(&messages[index],reactions,reaction_count,sender);
}
static bool history_keep[MESSAGE_HISTORY_LIMIT];
static int64_t history_dirty_since_us;
static bool history_clear_scope;
static uint32_t history_clear_peer;
static uint8_t history_clear_channel;
typedef struct {
    uint32_t magic;
    uint16_t version, count, reactions, capacity;
    uint32_t checksum;
} history_header_t;
static bool history_capacity_valid(size_t n) { return n == 100 || n == 250 || n == 500 || n == 1000; }
static uint32_t history_checksum(uint32_t hash, const void *data, size_t size) {
    const uint8_t *bytes = data;
    for (size_t i = 0; i < size; i++) hash = (hash ^ bytes[i]) * 16777619U;
    return hash;
}
static bool reaction_targets_message(const mesh_reaction_t *r, const mesh_message_t *m) {
    return m->packet_id && r->reply_id == m->packet_id && r->peer_node == m->peer_node &&
        (m->peer_node || r->channel == m->channel);
}
static bool keep_history_reaction(const mesh_reaction_t *r) {
    bool removed = false;
    for (size_t i = 0; i < message_count; i++) if (reaction_targets_message(r, &messages[i])) {
        if (history_keep[i]) return true;
        removed = true;
    }
    if (history_clear_scope && r->peer_node == history_clear_peer && (history_clear_peer || r->channel == history_clear_channel)) return false;
    return !removed; // Keep orphan reactions outside an explicitly cleared chat.
}
static bool write_history_selection(size_t capacity) {
    if (!history_fs_ready) return false;
    history_header_t header = {.magic = MESSAGE_HISTORY_MAGIC, .version = CURRENT_HISTORY_VERSION, .capacity = capacity};
    for (size_t i = 0; i < message_count; i++) if (history_keep[i]) header.count++;
    for (size_t i = 0; i < reaction_count; i++) if (keep_history_reaction(&reactions[i])) header.reactions++;
    uint32_t hash = history_checksum(2166136261U, &header, offsetof(history_header_t, checksum));
    FILE *file = fopen(HISTORY_TEMP_PATH, "wb");
    if (!file) return false;
    bool ok = fwrite(&header, sizeof(header), 1, file) == 1;
    for (size_t i = 0; ok && i < message_count; i++) if (history_keep[i]) {
        uint8_t unread = message_unread[i] && !messages[i].outgoing;
        hash = history_checksum(hash, &messages[i], sizeof(messages[i]));
        hash = history_checksum(hash, &unread, 1);
        ok = fwrite(&messages[i], sizeof(messages[i]), 1, file) == 1 && fwrite(&unread, 1, 1, file) == 1;
    }
    for (size_t i = 0; ok && i < reaction_count; i++) if (keep_history_reaction(&reactions[i])) {
        hash = history_checksum(hash, &reactions[i], sizeof(reactions[i]));
        ok = fwrite(&reactions[i], sizeof(reactions[i]), 1, file) == 1;
    }
    header.checksum = hash;
    ok = ok && fseek(file, 0, SEEK_SET) == 0 && fwrite(&header, sizeof(header), 1, file) == 1 &&
        fflush(file) == 0 && fsync(fileno(file)) == 0;
    if (fclose(file)) ok = false;
    if (!ok) { remove(HISTORY_TEMP_PATH); return false; }
    // Keep a previous complete file even on FAT, where rename cannot replace.
    struct stat current;
    bool backed_up = false;
    if (stat(HISTORY_PATH, &current) == 0) {
        remove(HISTORY_BACKUP_PATH);
        backed_up = rename(HISTORY_PATH, HISTORY_BACKUP_PATH) == 0;
        if (!backed_up) { remove(HISTORY_TEMP_PATH); return false; }
    }
    if (rename(HISTORY_TEMP_PATH, HISTORY_PATH)) {
        if (backed_up) rename(HISTORY_BACKUP_PATH, HISTORY_PATH);
        remove(HISTORY_TEMP_PATH); return false;
    }
    return true;
}
static void persist_message_history(void) {
    history_revision++;
    if (!history_dirty) { history_dirty_since_us = esp_timer_get_time(); history_save_at_us = history_dirty_since_us + 2000000; }
    history_dirty = true;
}
bool mesh_radio_flush_history(void) {
    if (!history_dirty) return true;
    for (size_t i = 0; i < message_count; i++) history_keep[i] = true;
    if (!write_history_selection(history_limit)) {
        history_save_at_us = esp_timer_get_time() + 5000000;
        return false;
    }
    history_dirty = false; return true;
}
void mesh_radio_poll_history(bool busy) {
    int64_t now = esp_timer_get_time();
    if (history_dirty && now >= history_save_at_us && (!busy || now - history_dirty_since_us >= 10000000))
        mesh_radio_flush_history();
}
static size_t history_victim(const bool *keep) {
    for (unsigned pass = 0; pass < 2; pass++) for (size_t i = 0; i < message_count; i++) {
        if (keep && !keep[i]) continue;
        if (messages[i].delivery == MESH_DELIVERY_PENDING) continue;
        if (!pass && message_unread[i]) continue;
        return i;
    }
    return SIZE_MAX;
}
static void apply_history_selection(void) {
    size_t retained = 0;
    for (size_t i = 0; i < reaction_count; i++) if (keep_history_reaction(&reactions[i])) reactions[retained++] = reactions[i];
    reaction_count = retained; retained = 0;
    for (size_t i = 0; i < message_count; i++) if (history_keep[i]) {
        messages[retained] = messages[i]; message_unread[retained] = message_unread[i];
        message_sent_us[retained] = message_sent_us[i]; message_hop_limit[retained++] = message_hop_limit[i];
    }
    message_count = retained;
}
static void remove_history_message(size_t index) {
    archive_history_message(index);
    for (size_t i = 0; i < message_count; i++) history_keep[i] = i != index;
    apply_history_selection();
}
size_t mesh_radio_history_limit(void) { return history_limit; }
bool mesh_radio_set_history_limit(size_t limit) {
    if (!history_capacity_valid(limit)) return false;
    for (size_t i = 0; i < message_count; i++) history_keep[i] = true;
    size_t retained = message_count;
    while (retained > limit) {
        size_t index = history_victim(history_keep);
        if (index == SIZE_MAX) return false;
        history_keep[index] = false; retained--;
    }
    if (!write_history_selection(limit)) return false;
    for(size_t i=0;i<message_count;i++)if(!history_keep[i])archive_history_message(i);
    apply_history_selection(); history_limit = limit; history_dirty = false; history_revision++;
    return true;
}
bool mesh_radio_clear_thread(uint32_t peer, uint8_t channel, size_t *removed) {
    if (peer == MESH_BROADCAST_NODE || (!peer && channel >= MESH_CHANNEL_LIMIT)) return false;
    size_t count = 0, reactions_before = reaction_count;
    for (size_t i = 0; i < message_count; i++) {
        bool match = messages[i].peer_node == peer && (peer || messages[i].channel == channel);
        history_keep[i] = !match || messages[i].delivery == MESH_DELIVERY_PENDING;
        if (!history_keep[i]) count++;
    }
    history_clear_scope = true; history_clear_peer = peer; history_clear_channel = channel;
    bool saved = write_history_selection(history_limit);
    if (saved) apply_history_selection();
    history_clear_scope = false;
    if (!saved) return false;
    history_dirty = false; history_revision++;
    if (count || reactions_before != reaction_count) remove(HISTORY_BACKUP_PATH); // A cleared chat must not return through the previous snapshot.
    if (removed) *removed = count;
    return true;
}
bool mesh_radio_delete_message(const mesh_message_t *target) {
    if (!target) return false;
    size_t found = SIZE_MAX;
    for (size_t i = 0; i < message_count; i++) {
        const mesh_message_t *m = &messages[i];
        if (m->peer_node != target->peer_node || m->channel != target->channel || m->outgoing != target->outgoing ||
            m->packet_id != target->packet_id || m->timestamp != target->timestamp || m->reply_id != target->reply_id ||
            strcmp(m->from, target->from) || strcmp(m->text, target->text)) continue;
        if (found != SIZE_MAX || m->delivery == MESH_DELIVERY_PENDING) return false;
        found = i;
    }
    if (found == SIZE_MAX) return false;
    for (size_t i = 0; i < message_count; i++) history_keep[i] = i != found;
    if (!write_history_selection(history_limit)) return false;
    apply_history_selection(); history_dirty = false; history_revision++;
    remove(HISTORY_BACKUP_PATH); // Do not resurrect intentionally deleted messages on recovery.
    return true;
}
static bool restore_current_history(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    history_header_t header;
    bool ok = fread(&header, sizeof(header), 1, file) == 1 && header.magic == MESSAGE_HISTORY_MAGIC &&
        (header.version == CURRENT_HISTORY_VERSION || header.version == 8) && history_capacity_valid(header.capacity) &&
        header.count <= header.capacity && header.reactions <= REACTION_HISTORY_LIMIT;
    uint32_t hash = ok ? history_checksum(2166136261U, &header, offsetof(history_header_t, checksum)) : 0;
    size_t length = ok ? header.count * ((header.version == 8 ? sizeof(v8_mesh_message_t) : sizeof(messages[0])) + 1) + header.reactions * sizeof(reactions[0]) : 0;
    uint8_t buffer[256];
    for (size_t left = length; ok && left;) {
        size_t n = left < sizeof(buffer) ? left : sizeof(buffer);
        ok = fread(buffer, n, 1, file) == 1;
        if (ok) hash = history_checksum(hash, buffer, n);
        left -= n;
    }
    ok = ok && hash == header.checksum && fgetc(file) == EOF && fseek(file, sizeof(header), SEEK_SET) == 0;
    for (size_t i = 0; ok && i < header.count; i++) {
        uint8_t unread;
        if (header.version == 8) {
            v8_mesh_message_t old;
            ok = fread(&old, sizeof(old), 1, file) == 1;
            if (ok) migrate_v8_message(&messages[i], &old);
        } else ok = fread(&messages[i], sizeof(messages[i]), 1, file) == 1;
        ok = ok && fread(&unread, 1, 1, file) == 1;
        const uint8_t *raw = (const uint8_t *)&messages[i];
        ok = ok && unread <= 1 && raw[offsetof(mesh_message_t, outgoing)] <= 1 && messages[i].channel < MESH_CHANNEL_LIMIT &&
            messages[i].failure <= MESH_FAILURE_RESTARTED && messages[i].delivery >= MESH_DELIVERY_NONE && messages[i].delivery <= MESH_DELIVERY_FAILED && memchr(messages[i].text, 0, sizeof(messages[i].text)) && memchr(messages[i].from, 0, sizeof(messages[i].from));
        message_unread[i] = unread != 0;
    }
    if (ok && header.reactions) ok = fread(reactions, sizeof(reactions[0]), header.reactions, file) == header.reactions;
    fclose(file);
    if (!ok) return false;
    message_count = header.count; reaction_count = header.reactions; history_limit = header.capacity;
    bool changed = header.version != CURRENT_HISTORY_VERSION || strcmp(path, HISTORY_PATH) != 0;
    for (size_t i = 0; i < message_count; i++) {
        message_unread[i] = message_unread[i] && !messages[i].outgoing;
        if (messages[i].delivery == MESH_DELIVERY_PENDING) { messages[i].delivery = MESH_DELIVERY_FAILED; messages[i].failure = MESH_FAILURE_RESTARTED; changed = true; }
    }
    for (size_t i = 0; i < reaction_count; i++) reactions[i].emoji[sizeof(reactions[i].emoji) - 1] = 0;
    if (changed) persist_message_history();
    return true;
}
