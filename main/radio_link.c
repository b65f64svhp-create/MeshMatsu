#include "radio_link.h"
#include "config_backup.h"
#include "map_trails.h"

#include <string.h>
#include <stddef.h>
#include <stdio.h>

#include "bsp/power.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#define RADIO_UART UART_NUM_1
#define RADIO_BAUD 38400
#define FRAME_MAX 512
#define RX_BUFFER_SIZE 1024
#define MESSAGE_HISTORY_LIMIT MESH_MESSAGE_HISTORY_MAX
#define LEGACY_MESSAGE_HISTORY_LIMIT 50
#define LEGACY_REACTION_HISTORY_LIMIT 80
#define MESSAGE_HISTORY_MAGIC 0x4d534748
#define LEGACY_HISTORY_VERSION 7
#define REACTION_HISTORY_LIMIT 320
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#else
#define EXT_RAM_BSS_ATTR
#endif
#define VALID_UNIX_TIME 1700000000U

static bool nb_origin_changed(void);
static void setup_poll(int64_t);
static void setup_reconnect(void);
static void setup_reset(void);
static void setup_response(const uint8_t *,size_t,uint32_t);
static void setup_error(uint32_t,uint32_t,uint32_t,uint32_t);
static void neighbor_poll(int64_t);
static void neighbor_reconnect(void);
static void neighbor_reset(void);
static void neighbor_error(uint32_t,uint32_t,uint32_t,uint32_t);
static void neighbor_receive(const uint8_t *,size_t,uint32_t,uint32_t,uint32_t,uint32_t,bool,uint32_t);
static void neighbor_module_dump(const uint8_t *,size_t);
static void neighbor_admin_response(const uint8_t *,size_t,uint32_t);
static void range_poll(int64_t);
static void range_error(uint32_t,uint32_t,uint32_t,uint32_t);
static void range_module_response(const uint8_t *,size_t,uint32_t);
static void range_receive_packet(const uint8_t *,size_t,uint32_t,uint32_t,uint32_t,uint32_t,bool,uint32_t,uint32_t,bool,float,bool,int32_t);
static bool management_local_busy(void);
static void security_response(const uint8_t *,size_t,uint32_t);
static void remote_response(uint32_t,uint32_t,uint32_t,bool,const uint8_t *,size_t,const uint8_t *,size_t);
static void management_ack(uint32_t,uint32_t,uint32_t,uint32_t);
static void management_poll(int64_t);
static void management_observe_contact(const mesh_node_t *);
static void management_complete(uint32_t);
static void decode_waypoint(uint32_t sender,uint32_t destination,uint32_t packet_id,uint32_t timestamp,uint32_t channel,const uint8_t *data,size_t size);
static void waypoint_ack(uint32_t sender,uint32_t destination,uint32_t request_id,uint32_t error_reason);
static void waypoint_echo(uint32_t packet_id,uint32_t hops);
static void waypoint_poll(int64_t now);
static void restore_waypoints(void);
static const char *TAG = "mesh_radio";
static uint8_t rx_buffer[RX_BUFFER_SIZE];
static size_t rx_size;
static mesh_radio_status_t status;
static mesh_node_t nodes[MESH_NODE_LIMIT];
static size_t node_count;
static mesh_radio_metadata_t radio_metadata;
static int64_t last_metadata_request_us;
static EXT_RAM_BSS_ATTR mesh_message_t messages[MESSAGE_HISTORY_LIMIT];
static size_t history_limit = 500;
static bool history_dirty;
static int64_t history_save_at_us;
static size_t message_count;
static bool message_unread[MESSAGE_HISTORY_LIMIT];
static uint16_t message_alerts;
static EXT_RAM_BSS_ATTR mesh_reaction_t reactions[REACTION_HISTORY_LIMIT];
static size_t reaction_count;
static mesh_trace_result_t traces[8];
static int64_t trace_started_us[8], last_trace_started_us;
static size_t trace_replace_index;
static uint32_t next_packet_id;
static int64_t last_config_request_us;
static uint32_t config_nonce;
static bool config_request_pending;
static int64_t last_radio_activity_us, last_rx_byte_us, probe_started_us, next_config_retry_us;
static int64_t last_heartbeat_us;
static int64_t config_retry_delay_us;
static bool uart_installed;
static void begin_radio_reconnect(void);
static bool history_fs_ready;
static wl_handle_t history_wl_handle = WL_INVALID_HANDLE;
static uint32_t latest_radio_time;
static int64_t latest_radio_time_us;
static int64_t message_sent_us[MESSAGE_HISTORY_LIMIT];
static uint8_t message_hop_limit[MESSAGE_HISTORY_LIMIT];
static uint32_t local_node_number;
static uint8_t admin_channel_index;
static char local_long_name[40];
static char local_short_name[5];
static uint8_t owner_user_raw[192];static size_t owner_user_size;
static uint32_t owner_user_origin;
static int64_t node_info_requested_at;
static bool node_info_requested;
static void cache_owner_user(const uint8_t *data,size_t size);
static uint8_t session_passkey[16];
static size_t session_passkey_size;
static int64_t session_passkey_received_us;
static mesh_favorite_status_t favorite_status;
static uint32_t favorite_request_id, favorite_verify_nonce;
static int64_t favorite_started_us, favorite_verify_at_us;
static bool favorite_seen, favorite_observed;
static mesh_node_control_status_t node_control;
static uint32_t node_control_id, node_control_nonce;
static int64_t node_control_started_us, node_control_verify_us;
static bool node_control_seen, node_control_ignored;
static bool owner_config_received;
static bool owner_identity_received;
static int64_t last_owner_request_us;
static int64_t owner_verify_at_us;

static struct {
    mesh_channel_t info;
    uint8_t raw[192];
    size_t size;
} channels[MESH_CHANNEL_LIMIT];
static int channel_verify_index = -1;
static int64_t channel_verify_at_us;
static int64_t last_channel_request_us[MESH_CHANNEL_LIMIT];
static void decode_channel(const uint8_t *data, size_t size);
static mesh_channel_operation_t channel_operation;
static struct {
    uint32_t origin, write_id, nonce; int64_t started, read_at;
    uint8_t settings[160]; size_t size;
    uint8_t roles[8], required, seen; bool matched;
} channel_operation_data;
static void channel_operation_observe(uint8_t index);
static void channel_operation_complete(uint32_t nonce);
static void channel_operation_poll(int64_t now);
static mesh_lora_config_t lora_config;
static uint8_t lora_raw[192];
static size_t lora_raw_size;
static int64_t last_lora_request_us;
static int64_t lora_verify_at_us;
static mesh_cfg_state_t cfg_state;
static bool (*cfg_identity_validator)(const uint8_t *,const uint8_t *);
static EXT_RAM_BSS_ATTR mesh_config_backup_t cfg_snapshot;
static mesh_cfg_record_t cfg_desired;
static uint32_t cfg_nonce,cfg_origin,cfg_write_id;
static bool cfg_bad,cfg_metadata_seen;
static int64_t cfg_started,cfg_read_at,cfg_loaded_us;
static void cfg_capture_reset(uint32_t nonce);
static void cfg_from(unsigned tag,const uint8_t *data,size_t size);
static void cfg_complete(uint32_t nonce);
static void cfg_poll(int64_t now);
static mesh_radio_keys_state_t keys_state;
static uint8_t keys_raw[224],keys_public[32],keys_private[32],keys_desired_public[32],keys_desired_private[32];
static size_t keys_raw_size;
static bool keys_loaded,keys_have_identity,keys_security_seen;
static uint8_t keys_channels_seen;
static uint32_t keys_origin,keys_nonce,keys_revision,keys_write_id;
static int64_t keys_started,keys_read_at,keys_loaded_us;
static void keys_config(const uint8_t *data,size_t size);
static void keys_complete(uint32_t nonce);
static void keys_poll(int64_t now);
static void decode_config(const uint8_t *data, size_t size);
static mesh_device_config_t device_config;
static uint8_t device_raw[192];
static size_t device_raw_size;
static mesh_role_save_t role_save;
static uint32_t role_write_id, role_read_id, role_origin;
static bool role_behavior_write;
static uint32_t behavior_mode, behavior_interval;
static int64_t role_started_us, role_read_at_us, last_device_request_us;
static mesh_position_config_t position_config;
static uint8_t position_raw[192];
static size_t position_raw_size;
static int64_t last_position_request_us;
static mesh_location_save_t location_save;
static struct {
    unsigned kind; // 1 position config, 2 fixed coordinates, 3 channel settings.
    uint32_t origin, write_id, nonce;
    int64_t started, read_at;
    bool config_seen, node_seen, channel_seen;
    mesh_position_config_t position;
    int32_t lat, lon;
    uint8_t channel;
    int precision;
    char name[12];
    bool uplink, downlink;
} location_op;
static bool location_in_dump;
static void location_observe_node(const mesh_node_t *node);
static void decode_position_config(const uint8_t *data, size_t size);
static void location_dump_field(uint32_t field);
static void location_dump_complete(uint32_t nonce);
static void location_poll(int64_t now);
static mesh_telemetry_config_t telemetry_config;
static mesh_telemetry_save_t telemetry_save;
static bool telemetry_enable_seen;
static uint8_t telemetry_module_raw[224], telemetry_config_raw[192];
static size_t telemetry_module_size, telemetry_config_size;
static struct {
    uint32_t origin, write_id, read_id;
    int64_t started_us, read_at_us, last_request_us;
    bool request_sent;
    mesh_telemetry_config_t desired;
} telemetry_config_op;
static bool decode_telemetry_config(const uint8_t *data, size_t size, uint32_t request_id);
static void telemetry_config_poll(int64_t now);
static void telemetry_config_error(uint32_t sender, uint32_t destination, uint32_t request_id, uint32_t error);


#ifndef HISTORY_PATH
#define HISTORY_PATH "/locfd/meshdeck-mui-history.bin"
#endif
#ifndef HISTORY_TEMP_PATH
#define HISTORY_TEMP_PATH "/locfd/meshdeck-mui-history.tmp"
#endif
#ifndef LEGACY_HISTORY_PATH
#define LEGACY_HISTORY_PATH "/locfd/meshdeck-history.bin"
#endif
#ifndef NODE_SEEN_PATH
#define NODE_SEEN_PATH "/locfd/meshdeck-mui-node-seen.bin"
#endif
#ifndef NODE_SEEN_TEMP_PATH
#define NODE_SEEN_TEMP_PATH "/locfd/meshdeck-mui-node-seen.tmp"
#endif
static struct {
    uint32_t magic, version, count;
    struct { uint32_t number, timestamp; } entries[MESH_NODE_LIMIT];
} node_seen;
static bool node_seen_dirty;
static int64_t node_seen_save_at_us;

typedef struct {
    uint32_t field;
    uint8_t wire;
    const uint8_t *value;
    size_t length;
    uint64_t integer;
} protobuf_field_t;
static mesh_device_metrics_t local_activity;
static uint32_t local_activity_node;
static struct { mesh_telemetry_request_t result; int64_t started_us; } telemetry_requests[8];
static unsigned telemetry_replace_index;
static int64_t last_telemetry_request_us;
static bool telemetry_request_sent;
static void telemetry_request_poll(int64_t now);
static void telemetry_request_reply(uint32_t sender, uint32_t destination, uint32_t request_id,uint8_t kind);
static void record_device_report(uint32_t node,const mesh_device_metrics_t *m,uint8_t kind);
static void telemetry_request_error(uint32_t sender, uint32_t destination, uint32_t request_id, uint32_t reason);
static bool decode_device_metrics(const uint8_t *data, size_t size, bool local_stats, mesh_device_metrics_t *out);
static bool metrics_replace(const mesh_device_metrics_t *old, const mesh_device_metrics_t *incoming);
static void decode_telemetry(uint32_t sender, uint32_t destination, uint32_t packet_id, uint32_t request_id, const uint8_t *data, size_t size);


typedef struct {
    char from[12], text[234];
    uint32_t timestamp, packet_id;
    mesh_delivery_state_t delivery;
    uint32_t peer_node;
    bool outgoing;
    uint8_t channel;
} v8_mesh_message_t;
_Static_assert(sizeof(v8_mesh_message_t) == 268, "v5-v8 message ABI");
static void migrate_v8_message(mesh_message_t *out, const v8_mesh_message_t *old) {
    memset(out, 0, sizeof(*out));
    memcpy(out, old, offsetof(v8_mesh_message_t, channel) + sizeof(old->channel));
}

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    v8_mesh_message_t messages[LEGACY_MESSAGE_HISTORY_LIMIT];
    uint64_t unread_mask;
    uint16_t reaction_count;
    mesh_reaction_t reactions[LEGACY_REACTION_HISTORY_LIMIT];
} stored_message_history_t;

typedef struct {
    uint32_t magic;
    uint16_t version, count;
    v8_mesh_message_t messages[LEGACY_MESSAGE_HISTORY_LIMIT];
    uint64_t unread_mask;
} stored_v6_message_history_t;
_Static_assert(sizeof(stored_v6_message_history_t) == offsetof(stored_message_history_t, reaction_count), "v6 history layout");

// Version 5 used the same messages, without the unread mask.
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    v8_mesh_message_t messages[LEGACY_MESSAGE_HISTORY_LIMIT];
} stored_v5_message_history_t;
_Static_assert(sizeof(stored_v5_message_history_t) == offsetof(stored_message_history_t, unread_mask), "v5 history layout");

typedef struct {
    char from[12];
    char text[234];
} legacy_mesh_message_t;

typedef struct {
    char from[12];
    char text[234];
    uint32_t timestamp;
} v2_mesh_message_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    v2_mesh_message_t messages[LEGACY_MESSAGE_HISTORY_LIMIT];
} stored_v2_message_history_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    legacy_mesh_message_t messages[LEGACY_MESSAGE_HISTORY_LIMIT];
} stored_legacy_message_history_t;

typedef struct {
    char from[12];
    char text[234];
    uint32_t timestamp;
    uint32_t packet_id;
    mesh_delivery_state_t delivery;
} v3_mesh_message_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    v3_mesh_message_t messages[LEGACY_MESSAGE_HISTORY_LIMIT];
} stored_v3_message_history_t;
static stored_v3_message_history_t v3_history_storage;

typedef struct {
    char from[12];
    char text[234];
    uint32_t timestamp;
    uint32_t packet_id;
    mesh_delivery_state_t delivery;
    uint32_t peer_node;
    bool outgoing;
} v4_mesh_message_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    v4_mesh_message_t messages[LEGACY_MESSAGE_HISTORY_LIMIT];
} stored_v4_message_history_t;
static stored_v4_message_history_t v4_history_storage;

static stored_message_history_t history_storage;
static stored_legacy_message_history_t legacy_history_storage;
static stored_v2_message_history_t v2_history_storage;

static uint32_t current_message_timestamp(void) {
    time_t now = time(NULL);
    if (now >= (time_t)VALID_UNIX_TIME && (uint64_t)now <= UINT32_MAX) return (uint32_t)now;
    if (latest_radio_time >= VALID_UNIX_TIME) {
        int64_t elapsed_seconds = (esp_timer_get_time() - latest_radio_time_us) / 1000000;
        if (elapsed_seconds >= 0 && (uint64_t)latest_radio_time + (uint64_t)elapsed_seconds <= UINT32_MAX) {
            return latest_radio_time + (uint32_t)elapsed_seconds;
        }
    }
    return 0;
}

static void initialize_message_history_storage(void) {
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 2,
        .allocation_unit_size = 0,
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl("/locfd", "locfd", &mount_config,
                                                     &history_wl_handle);
    if (err == ESP_OK) {
        history_fs_ready = true;
        ESP_LOGI(TAG, "Message history storage mounted on locfd");
    } else {
        struct stat mount_point;
        if (err == ESP_ERR_INVALID_STATE && stat("/locfd", &mount_point) == 0 &&
            S_ISDIR(mount_point.st_mode)) {
            history_fs_ready = true;
            ESP_LOGI(TAG, "Using the existing locfd history mount");
            return;
        }
        ESP_LOGW(TAG, "Could not mount locfd history storage: %s", esp_err_to_name(err));
    }
}

static uint32_t history_revision;
uint32_t mesh_radio_history_revision(void) { return history_revision; }

static void restore_node_seen(void) {
    memset(&node_seen, 0, sizeof(node_seen));
    node_seen_dirty = false; node_seen_save_at_us = 0;
    if (!history_fs_ready) return;
    FILE *file = fopen(NODE_SEEN_PATH, "rb");
    if (!file) return;
    bool valid = fread(&node_seen, sizeof(node_seen), 1, file) == 1 && fgetc(file) == EOF &&
        node_seen.magic == 0x4d4e534e && node_seen.version == 1 && node_seen.count <= MESH_NODE_LIMIT;
    fclose(file);
    for (size_t i = 0; valid && i < node_seen.count; i++) {
        valid = node_seen.entries[i].number && node_seen.entries[i].number != MESH_BROADCAST_NODE &&
            node_seen.entries[i].timestamp >= VALID_UNIX_TIME;
        for (size_t j = 0; j < i; j++) if (node_seen.entries[j].number == node_seen.entries[i].number) valid = false;
    }
    if (!valid) memset(&node_seen, 0, sizeof(node_seen));
}

static uint32_t note_node_seen(uint32_t number) {
    for (size_t i = 0; i < node_seen.count; i++) if (node_seen.entries[i].number == number) return node_seen.entries[i].timestamp;
    uint32_t now = current_message_timestamp();
    if (!number || number == MESH_BROADCAST_NODE || now < VALID_UNIX_TIME) return 0;
    size_t slot = node_seen.count;
    if (slot == MESH_NODE_LIMIT) {
        slot = 0;
        for (size_t i = 1; i < node_seen.count; i++) if (node_seen.entries[i].timestamp < node_seen.entries[slot].timestamp) slot = i;
    } else node_seen.count++;
    node_seen.entries[slot].number = number; node_seen.entries[slot].timestamp = now;
    if (!node_seen_dirty) node_seen_save_at_us = esp_timer_get_time() + 5000000;
    node_seen_dirty = true;
    return now;
}

void mesh_radio_flush_node_cache(void) {
    if (!node_seen_dirty || !history_fs_ready) return;
    node_seen_save_at_us = esp_timer_get_time() + 60000000;
    node_seen.magic = 0x4d4e534e; node_seen.version = 1;
    FILE *file = fopen(NODE_SEEN_TEMP_PATH, "wb");
    if (!file) return;
    bool saved = fwrite(&node_seen, sizeof(node_seen), 1, file) == 1 && fflush(file) == 0 && fsync(fileno(file)) == 0;
    if (fclose(file) != 0) saved = false;
    if (saved && rename(NODE_SEEN_TEMP_PATH, NODE_SEEN_PATH) == 0) node_seen_dirty = false;
    else remove(NODE_SEEN_TEMP_PATH);
}

#include "history_store.inc"

static void restore_legacy_message_history(const char *path) {
    if (!history_fs_ready) return;
    memset(message_unread, 0, sizeof(message_unread));
    reaction_count = 0;

    FILE *file = fopen(path, "rb");
    if (!file) return;
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    bool header_ok = fread(&magic, sizeof(magic), 1, file) == 1 &&
                     fread(&version, sizeof(version), 1, file) == 1 &&
                     fread(&count, sizeof(count), 1, file) == 1 &&
                     magic == MESSAGE_HISTORY_MAGIC && count <= LEGACY_MESSAGE_HISTORY_LIMIT;
    if (!header_ok) {
        fclose(file);
        return;
    }

    if (version == 1) {
        rewind(file);
        bool read_ok = fread(&legacy_history_storage, sizeof(legacy_history_storage), 1, file) == 1 &&
                       fgetc(file) == EOF;
        fclose(file);
        if (!read_ok || legacy_history_storage.magic != MESSAGE_HISTORY_MAGIC ||
            legacy_history_storage.version != 1 || legacy_history_storage.count > LEGACY_MESSAGE_HISTORY_LIMIT) return;
        message_count = legacy_history_storage.count;
        for (size_t i = 0; i < message_count; i++) {
            memset(&messages[i], 0, sizeof(messages[i]));
            memcpy(messages[i].from, legacy_history_storage.messages[i].from, sizeof(messages[i].from));
            memcpy(messages[i].text, legacy_history_storage.messages[i].text, sizeof(messages[i].text));
            messages[i].outgoing = strcmp(messages[i].from, "You") == 0;
        }
        persist_message_history();
        return;
    }

    if (version == 2) {
        rewind(file);
        bool read_ok = fread(&v2_history_storage, sizeof(v2_history_storage), 1, file) == 1 &&
                       fgetc(file) == EOF;
        fclose(file);
        if (!read_ok || v2_history_storage.magic != MESSAGE_HISTORY_MAGIC ||
            v2_history_storage.version != 2 || v2_history_storage.count > LEGACY_MESSAGE_HISTORY_LIMIT) return;
        message_count = v2_history_storage.count;
        for (size_t i = 0; i < message_count; i++) {
            memset(&messages[i], 0, sizeof(messages[i]));
            memcpy(messages[i].from, v2_history_storage.messages[i].from, sizeof(messages[i].from));
            memcpy(messages[i].text, v2_history_storage.messages[i].text, sizeof(messages[i].text));
            messages[i].timestamp = v2_history_storage.messages[i].timestamp;
            messages[i].outgoing = strcmp(messages[i].from, "You") == 0;
        }
        persist_message_history();
        return;
    }

    if (version == 3) {
        rewind(file);
        bool ok = fread(&v3_history_storage, sizeof(v3_history_storage), 1, file) == 1 && fgetc(file) == EOF;
        fclose(file);
        if (!ok) return;
        message_count = count;
        for (size_t i = 0; i < message_count; i++) {
            const v3_mesh_message_t *old = &v3_history_storage.messages[i];
            messages[i] = (mesh_message_t){.timestamp = old->timestamp, .packet_id = old->packet_id,
                .delivery = old->delivery == MESH_DELIVERY_PENDING ? MESH_DELIVERY_FAILED : old->delivery,
                .outgoing = strcmp(old->from, "You") == 0};
            memcpy(messages[i].from, old->from, sizeof(old->from));
            memcpy(messages[i].text, old->text, sizeof(old->text));
        }
        persist_message_history();
        return;
    }

    if (version == 4) {
        rewind(file);
        bool ok = fread(&v4_history_storage, sizeof(v4_history_storage), 1, file) == 1 && fgetc(file) == EOF;
        fclose(file);
        if (!ok) return;
        message_count = count;
        for (size_t i = 0; i < message_count; i++) {
            const v4_mesh_message_t *old = &v4_history_storage.messages[i];
            messages[i] = (mesh_message_t){.timestamp = old->timestamp, .packet_id = old->packet_id,
                .delivery = old->delivery == MESH_DELIVERY_PENDING ? MESH_DELIVERY_FAILED : old->delivery,
                .peer_node = old->peer_node, .outgoing = old->outgoing, .channel = 0};
            memcpy(messages[i].from, old->from, sizeof(old->from));
            memcpy(messages[i].text, old->text, sizeof(old->text));
        }
        persist_message_history();
        return;
    }

    rewind(file);
    memset(&history_storage, 0, sizeof(history_storage));
    size_t stored_size = version == 5 ? sizeof(stored_v5_message_history_t) : version == 6 ? sizeof(stored_v6_message_history_t) : sizeof(history_storage);
    bool read_ok = fread(&history_storage, stored_size, 1, file) == 1 && fgetc(file) == EOF;
    fclose(file);
    if (!read_ok || history_storage.magic != MESSAGE_HISTORY_MAGIC ||
        (history_storage.version != 5 && history_storage.version != 6 && history_storage.version != LEGACY_HISTORY_VERSION) ||
        history_storage.count > LEGACY_MESSAGE_HISTORY_LIMIT || history_storage.reaction_count > LEGACY_REACTION_HISTORY_LIMIT) return;
    message_count = history_storage.count;
    for (size_t i = 0; i < message_count; i++) migrate_v8_message(&messages[i], &history_storage.messages[i]);
    reaction_count = history_storage.reaction_count;
    memcpy(reactions, history_storage.reactions, reaction_count * sizeof(reactions[0]));
    for (size_t i = 0; i < reaction_count; i++) reactions[i].emoji[sizeof(reactions[i].emoji) - 1] = 0;
    bool history_changed = true; // Every legacy format migrates to the current format.
    for (size_t i = 0; i < message_count; i++) {
        message_unread[i] = !messages[i].outgoing && (history_storage.unread_mask & (UINT64_C(1) << i)) != 0;
        if (messages[i].delivery == MESH_DELIVERY_PENDING) {
            messages[i].delivery = MESH_DELIVERY_FAILED;
            history_changed = true;
        }
    }
    if (history_changed) persist_message_history();
}

static void restore_message_history(void) {
    if (!history_fs_ready) return;
    history_limit = 500; history_dirty = false; message_count = reaction_count = 0;
    struct stat info;
    bool had_history = stat(HISTORY_PATH, &info) == 0 || stat(HISTORY_BACKUP_PATH, &info) == 0;
    if (restore_current_history(HISTORY_PATH)) return;
    restore_legacy_message_history(HISTORY_PATH);
    if (history_dirty) { mesh_radio_flush_history(); return; }
    if (restore_current_history(HISTORY_BACKUP_PATH)) { remove(HISTORY_PATH); return; }
    restore_legacy_message_history(HISTORY_BACKUP_PATH);
    if (history_dirty) { remove(HISTORY_PATH); mesh_radio_flush_history(); return; }
    if (had_history) return; // Never resurrect an old app import after clearing/corrupting current history.
    restore_legacy_message_history(LEGACY_HISTORY_PATH);
    if (history_dirty) mesh_radio_flush_history();
}

// Skip a protobuf value of the supplied wire type.
static bool skip_value(const uint8_t *data, size_t size, size_t *offset, uint8_t wire_type) {
    uint64_t length = 0;
    unsigned shift = 0;
    switch (wire_type) {
        case 0:
            while (*offset < size && shift < 64) {
                uint8_t byte = data[(*offset)++];
                length |= (uint64_t)(byte & 0x7f) << shift;
                if ((byte & 0x80) == 0) return true;
                shift += 7;
            }
            return false;
        case 1:
            if (size - *offset < 8) return false;
            *offset += 8;
            return true;
        case 2:
            while (*offset < size && shift < 64) {
                uint8_t byte = data[(*offset)++];
                length |= (uint64_t)(byte & 0x7f) << shift;
                if ((byte & 0x80) == 0) {
                    if (length > size - *offset) return false;
                    *offset += (size_t)length;
                    return true;
                }
                shift += 7;
            }
            return false;
        case 5:
            if (size - *offset < 4) return false;
            *offset += 4;
            return true;
        default:
            return false;
    }
}

static bool read_varint(const uint8_t *data, size_t size, size_t *offset, uint64_t *value) {
    *value = 0;
    for (unsigned shift = 0; shift < 64 && *offset < size; shift += 7) {
        uint8_t byte = data[(*offset)++];
        *value |= (uint64_t)(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) return true;
    }
    return false;
}

static bool read_field(const uint8_t *data, size_t size, size_t *offset, protobuf_field_t *field) {
    uint64_t tag;
    if (!read_varint(data, size, offset, &tag) || tag == 0) return false;
    field->field = (uint32_t)(tag >> 3);
    field->wire = (uint8_t)(tag & 7);
    field->value = NULL;
    field->length = 0;
    field->integer = 0;
    if (field->wire == 2) {
        uint64_t length;
        if (!read_varint(data, size, offset, &length) || length > size - *offset) return false;
        field->value = data + *offset;
        field->length = (size_t)length;
        *offset += (size_t)length;
        return true;
    }
    if (field->wire == 0) return read_varint(data, size, offset, &field->integer);
    size_t old_offset = *offset;
    if (!skip_value(data, size, offset, field->wire)) return false;
    field->value = data + old_offset;
    field->length = *offset - old_offset;
    return true;
}

static bool store_message(const char *from, const uint8_t *text, size_t text_size, uint32_t timestamp, uint32_t peer_node, bool outgoing, uint8_t channel) {
    if (message_count >= history_limit) {
        size_t victim = history_victim(NULL);
        if (victim == SIZE_MAX) return false;
        remove_history_message(victim);
    }
    mesh_message_t *message = &messages[message_count++];
    memset(message, 0, sizeof(*message));
    snprintf(message->from, sizeof(message->from), "%s", from);
    size_t copy = text_size < sizeof(message->text) - 1 ? text_size : sizeof(message->text) - 1;
    memcpy(message->text, text, copy);
    message->text[copy] = '\0';
    message->timestamp = timestamp ? timestamp : current_message_timestamp();
    message->peer_node = peer_node;
    message->outgoing = outgoing;
    message->channel = channel;
    message->packet_id = 0;
    message->delivery = MESH_DELIVERY_NONE;
    message_sent_us[message_count - 1] = 0;
    message_hop_limit[message_count - 1] = 0;
    message_unread[message_count - 1] = !outgoing;
    persist_message_history();
    return true;
}

#include "node_preferences.inc"
#include "node_aliases.inc"
#include "contact_trust.inc"

static void parse_routing_ack(const uint8_t *data, size_t size, uint32_t sender, uint32_t destination) {
    size_t offset = 0;
    uint32_t request_id = 0;
    uint32_t error_reason = 0;
    bool has_request_id = false;
    while (offset < size) {
        protobuf_field_t field;
        if (!read_field(data, size, &offset, &field)) return;
        if (field.field == 6 && field.wire == 5 && field.length == 4) {
            memcpy(&request_id, field.value, sizeof(request_id));
            has_request_id = true;
        } else if (field.field == 2 && field.wire == 2) {
            size_t routing_offset = 0;
            while (routing_offset < field.length) {
                protobuf_field_t routing_field;
                if (!read_field(field.value, field.length, &routing_offset, &routing_field)) return;
                if (routing_field.field == 3 && routing_field.wire == 0) {
                    error_reason = (uint32_t)routing_field.integer;
                }
            }
        }
    }
    if (!has_request_id || request_id == 0) return;
    if (channel_operation.state == MESH_FAVORITE_SAVING && request_id == channel_operation_data.write_id &&
        sender == channel_operation_data.origin && (destination == sender || !destination) && error_reason) {
        channel_operation.state = MESH_FAVORITE_FAILED; channel_operation.error_reason = error_reason; history_revision++;
    }
    if(keys_state==MESH_KEYS_SAVING&&request_id==keys_write_id&&sender==keys_origin&&
        (destination==sender||!destination)&&error_reason){keys_state=MESH_KEYS_FAILED;mesh_radio_keys_clear();mesh_keys_wipe(keys_desired_private,32);history_revision++;}
    if(cfg_state==CFG_SAVING&&request_id==cfg_write_id&&sender==cfg_origin&&(destination==sender||!destination)&&error_reason){cfg_state=CFG_FAILED;mesh_keys_wipe(&cfg_desired,sizeof(cfg_desired));mesh_keys_wipe(&cfg_snapshot,sizeof(cfg_snapshot));history_revision++;}
    setup_error(sender,destination,request_id,error_reason);
    neighbor_error(sender,destination,request_id,error_reason);
    range_error(sender,destination,request_id,error_reason);
    management_ack(sender,destination,request_id,error_reason);
    waypoint_ack(sender,destination,request_id,error_reason);
    telemetry_request_error(sender,destination,request_id,error_reason);
    telemetry_config_error(sender,destination,request_id,error_reason);
    if (location_save.state == MESH_LOCATION_SAVING && request_id == location_op.write_id &&
        sender == location_op.origin && (destination == location_op.origin || destination == 0) && error_reason && error_reason != 8) {
        location_save = (mesh_location_save_t){.state = MESH_LOCATION_FAILED, .error_reason = error_reason};
        history_revision++;
    }
    if (role_save.state == MESH_ROLE_SAVING && request_id == role_write_id && sender == role_origin &&
        (destination == role_origin || destination == 0) && error_reason && error_reason != 8) {
        role_save.state = MESH_ROLE_FAILED;
        role_save.error_reason = error_reason;
        history_revision++;
    }
    if (node_control.state == MESH_FAVORITE_SAVING && request_id == node_control_id &&
        sender == local_node_number && (destination == local_node_number || destination == 0) && error_reason) {
        node_control.state = MESH_FAVORITE_FAILED; node_control.error_reason = error_reason; history_revision++;
    }
    if (favorite_status.state == MESH_FAVORITE_SAVING && request_id == favorite_request_id &&
        sender == local_node_number && (destination == local_node_number || destination == 0) && error_reason) {
        favorite_status.state = MESH_FAVORITE_FAILED;
        favorite_status.error_reason = error_reason;
        history_revision++;
    }
    for (size_t i = 0; i < 8; i++) {
        mesh_trace_result_t *trace = &traces[i];
        if (trace->state == MESH_TRACE_WAITING && trace->request_id == request_id && error_reason &&
            (destination == local_node_number || (sender == local_node_number && destination == 0)) &&
            (sender == local_node_number || sender == trace->node)) {
            trace->state = MESH_TRACE_ERROR;
            trace->error_reason = error_reason;
            trace->completed_at = current_message_timestamp();
            history_revision++;
        }
    }
    for (size_t i = 0; i < message_count; i++) {
        if (messages[i].packet_id == request_id && messages[i].delivery == MESH_DELIVERY_PENDING) {
            // A local enqueue ACK or a relay hearing a DM is not recipient delivery.
            if (error_reason == 0 && messages[i].peer_node && sender != messages[i].peer_node) continue;
            messages[i].delivery = error_reason == 0 ? MESH_DELIVERY_ACKNOWLEDGED : MESH_DELIVERY_FAILED;
            messages[i].error_reason = error_reason;
            messages[i].failure = error_reason ? MESH_FAILURE_RADIO : MESH_FAILURE_UNKNOWN;
            message_sent_us[i] = 0;
            persist_message_history();
        }
    }
}

static void decode_metadata(const uint8_t *data, size_t size) {
    mesh_radio_metadata_t result = {.loaded = true};
    size_t offset = 0;
    while (offset < size) {
        protobuf_field_t field;
        if (!read_field(data, size, &offset, &field)) return;
        if (field.field == 1 && field.wire == 2) {
            if (field.length >= sizeof(result.firmware_version) || memchr(field.value, 0, field.length)) return;
            for (size_t i = 0; i < field.length; i++) {
                if (field.value[i] < 32 || field.value[i] > 126) return;
                result.firmware_version[i] = field.value[i];
            }
        } else if (field.field == 7 && field.wire == 0) result.role = (uint32_t)field.integer;
        else if (field.field == 9 && field.wire == 0) result.hardware_model = (uint32_t)field.integer;
    }
    radio_metadata = result;
    history_revision++;
}

static void parse_admin_response(const uint8_t *data, size_t size, uint32_t request_id) {
    // Validate the complete envelope before a response can verify a mutation.
    size_t check = 0; bool config_seen = false;
    while (check < size) {
        protobuf_field_t f;
        if (!read_field(data, size, &check, &f)) return;
        if (f.field == 6) { if (f.wire != 2 || config_seen) return; config_seen = true; }
    }
    setup_response(data,size,request_id);
    neighbor_admin_response(data,size,request_id);
    security_response(data,size,request_id);
    size_t offset = 0;
    while (offset < size) {
        protobuf_field_t field;
        if (!read_field(data, size, &offset, &field)) return;
        if (field.field == 13 && field.wire == 2) decode_metadata(field.value, field.length);
        else if (field.field == 8 && field.wire == 2) {range_module_response(field.value,field.length,request_id);decode_telemetry_config(field.value, field.length, request_id);}
        else if (field.field == 101 && field.wire == 2 && field.length <= sizeof(session_passkey)) {
            memcpy(session_passkey, field.value, field.length);
            session_passkey_size = field.length;
            session_passkey_received_us = esp_timer_get_time();
        } else if (field.field == 2 && field.wire == 2) {
            decode_channel(field.value, field.length);
        } else if (field.field == 6 && field.wire == 2) {
            uint32_t before = device_config.revision;
            decode_config(field.value, field.length);
            if (device_config.revision != before && role_save.state == MESH_ROLE_SAVING && role_read_id &&
                request_id == role_read_id && local_node_number == role_origin) {
                bool matches = device_config.role == role_save.desired && (!role_behavior_write ||
                    (device_config.rebroadcast_mode == behavior_mode && device_config.node_info_broadcast_secs == behavior_interval));
                role_save.state = matches ? MESH_ROLE_SAVED : MESH_ROLE_FAILED;
                history_revision++;
            }
        } else if (field.field == 4 && field.wire == 2) {
            owner_config_received = true;
            owner_identity_received = true;
            size_t owner_offset = 0;
            while (owner_offset < field.length) {
                protobuf_field_t owner_field;
                if (!read_field(field.value, field.length, &owner_offset, &owner_field)) return;
                if (owner_field.field == 2 && owner_field.wire == 2) {
                    size_t copy = owner_field.length < sizeof(local_long_name) - 1 ?
                                  owner_field.length : sizeof(local_long_name) - 1;
                    memcpy(local_long_name, owner_field.value, copy);
                    local_long_name[copy] = '\0';
                } else if (owner_field.field == 3 && owner_field.wire == 2) {
                    size_t copy = owner_field.length < sizeof(local_short_name) - 1 ?
                                  owner_field.length : sizeof(local_short_name) - 1;
                    memcpy(local_short_name, owner_field.value, copy);
                    local_short_name[copy] = '\0';
                }
            }
            cache_owner_user(field.value,field.length);
        }
    }
}

static void update_node_user(uint32_t number, const uint8_t *data, size_t size);

static void store_reaction(uint32_t sender, uint32_t packet_id, uint32_t reply_id,
                           uint32_t peer, uint8_t channel, uint32_t timestamp,
                           const uint8_t *emoji, size_t size) {
    if (!reply_id || !size || size >= sizeof(reactions[0].emoji) || memchr(emoji, 0, size)) return;
    for (size_t i = 0; i < reaction_count; i++) {
        const mesh_reaction_t *old = &reactions[i];
        if (old->sender != sender || old->peer_node != peer || old->channel != channel) continue;
        if ((packet_id && old->packet_id == packet_id) ||
            (old->reply_id == reply_id && strlen(old->emoji) == size && !memcmp(old->emoji, emoji, size))) return;
    }
    if (reaction_count == REACTION_HISTORY_LIMIT) {
        memmove(reactions, reactions + 1, (reaction_count - 1) * sizeof(reactions[0]));
        reaction_count--;
    }
    mesh_reaction_t *reaction = &reactions[reaction_count++];
    *reaction = (mesh_reaction_t){.sender = sender, .packet_id = packet_id, .reply_id = reply_id,
        .peer_node = peer, .channel = channel, .timestamp = timestamp ? timestamp : current_message_timestamp()};
    memcpy(reaction->emoji, emoji, size);
    persist_message_history();
}

static bool decode_trace_payload(const uint8_t *data, size_t size, mesh_trace_result_t *result) {
    size_t offset = 0;
    while (offset < size) {
        protobuf_field_t field;
        if (!read_field(data, size, &offset, &field)) return false;
        if (field.field == 1 || field.field == 3) {
            uint8_t *count = field.field == 1 ? &result->route_count : &result->return_count;
            uint32_t *route = field.field == 1 ? result->route : result->return_route;
            if (field.wire == 5) {
                if (*count >= MESH_TRACE_HOPS) return false;
                memcpy(&route[(*count)++], field.value, 4);
            } else if (field.wire == 2) {
                if (field.length % 4 || field.length / 4 > MESH_TRACE_HOPS - *count) return false;
                for (size_t pos = 0; pos < field.length; pos += 4) memcpy(&route[(*count)++], field.value + pos, 4);
            } else return false;
        } else if (field.field == 2 || field.field == 4) {
            uint8_t *count = field.field == 2 ? &result->snr_count : &result->return_snr_count;
            int32_t *snr = field.field == 2 ? result->snr : result->return_snr;
            if (field.wire == 0) {
                if (*count >= MESH_TRACE_HOPS + 1) return false;
                snr[(*count)++] = (int32_t)(uint32_t)field.integer;
            } else if (field.wire == 2) {
                size_t pos = 0;
                while (pos < field.length) {
                    uint64_t value;
                    if (*count >= MESH_TRACE_HOPS + 1 || !read_varint(field.value, field.length, &pos, &value)) return false;
                    snr[(*count)++] = (int32_t)(uint32_t)value;
                }
            } else return false;
        }
    }
    return true;
}

static void accept_trace_reply(uint32_t sender, uint32_t destination, uint32_t request_id,
                               const uint8_t *payload, size_t size) {
    if (!request_id || destination != local_node_number) return;
    for (size_t i = 0; i < 8; i++) {
        mesh_trace_result_t *trace = &traces[i];
        if (trace->request_id != request_id || trace->node != sender || trace->origin != local_node_number ||
            (trace->state != MESH_TRACE_WAITING && trace->state != MESH_TRACE_TIMEOUT)) continue;
        mesh_trace_result_t result = {.node = trace->node, .origin = trace->origin,
            .request_id = request_id, .requested_at = trace->requested_at, .state = MESH_TRACE_COMPLETE,
            .completed_at = current_message_timestamp()};
        if (!decode_trace_payload(payload, size, &result)) return;
        *trace = result;
        history_revision++;
        return;
    }
}

// Position uses optional sfixed32 fields: zero is valid; absence is not a fix.
static bool decode_position(const uint8_t *data, size_t size, mesh_node_t *node) {
    mesh_node_t parsed = {0};
    bool latitude = false, longitude = false;
    uint32_t reported_time = 0, fix_time = 0;
    size_t offset = 0;
    while (offset < size) {
        protobuf_field_t field;
        if (!read_field(data, size, &offset, &field)) return false;
        if (field.field == 1 || field.field == 2) {
            if (field.wire != 5 || field.length != 4) return false;
            if (field.field == 1) { memcpy(&parsed.latitude_i, field.value, 4); latitude = true; }
            else { memcpy(&parsed.longitude_i, field.value, 4); longitude = true; }
        } else if ((field.field == 4 || field.field == 7) && field.wire == 5) {
            uint32_t value; memcpy(&value, field.value, 4);
            if (field.field == 7) fix_time = value; else reported_time = value;
        } else if (field.field == 23 && field.wire == 0) {
            if (field.integer > 32) return false;
            parsed.position_precision = (uint8_t)field.integer;
        }
    }
    if (!latitude || !longitude || parsed.latitude_i < -900000000 || parsed.latitude_i > 900000000 ||
        parsed.longitude_i < -1800000000 || parsed.longitude_i > 1800000000) return false;
    parsed.position_time = fix_time ? fix_time : reported_time;
    if (node->position_available && node->position_time &&
        (!parsed.position_time || parsed.position_time < node->position_time)) return false;
    node->position_available = true;
    node->latitude_i = parsed.latitude_i; node->longitude_i = parsed.longitude_i;
    node->position_time = parsed.position_time; node->position_precision = parsed.position_precision;
    return true;
}

static void copy_node_position(mesh_node_t *to, const mesh_node_t *from) {
    to->position_available = from->position_available;
    to->latitude_i = from->latitude_i; to->longitude_i = from->longitude_i;
    to->position_time = from->position_time; to->position_precision = from->position_precision;
}

static void record_node_trail(const mesh_node_t *node){
    if(node->is_ignored){mesh_trails_forget(node->number);return;}
    if(node->position_available)mesh_trails_add(node->number,node->latitude_i,node->longitude_i,node->position_time,node->position_precision,current_message_timestamp());
}

static void update_node_position(uint32_t number, const uint8_t *data, size_t size) {
    if (!number || number == MESH_BROADCAST_NODE) return;
    for (size_t i = 0; i < node_count; i++) if (nodes[i].number == number) {
        if (decode_position(data, size, &nodes[i])) {record_node_trail(&nodes[i]);history_revision++;}
        return;
    }
    if (node_count >= MESH_NODE_LIMIT) return;
    mesh_node_t node = {.number = number};
    if (!decode_position(data, size, &node)) return;
    snprintf(node.name, sizeof(node.name), "!%08lx", (unsigned long)number);
    record_node_trail(&node);nodes[node_count++] = node; status.node_count = (uint16_t)node_count; history_revision++;
}

static void decode_data_message(const uint8_t *data, size_t size, uint32_t sender, uint32_t timestamp, uint32_t destination, uint32_t packet_id, uint32_t channel) {
    for (size_t i = 0; i < node_count; i++) if (nodes[i].number == sender && nodes[i].is_ignored) return;
    size_t offset = 0;
    const uint8_t *payload = NULL;
    size_t payload_size = 0;
    uint64_t portnum = 0;
    uint32_t reply_id = 0, emoji = 0, request_id = 0;
    while (offset < size) {
        protobuf_field_t field;
        if (!read_field(data, size, &offset, &field)) return;
        if (field.field == 1 && field.wire == 0) portnum = field.integer;
        else if (field.field == 2 && field.wire == 2) { payload = field.value; payload_size = field.length; }
        else if (field.field == 6 && field.wire == 5 && field.length == 4) memcpy(&request_id, field.value, 4);
        else if (field.field == 7 && field.wire == 5 && field.length == 4) memcpy(&reply_id, field.value, 4);
        else if (field.field == 8 && field.wire == 5 && field.length == 4) memcpy(&emoji, field.value, 4);
    }
    if ((portnum == 1 || (portnum == 32 && emoji)) && payload && payload_size) {
        char from[12];
        snprintf(from, sizeof(from), "!%08lx", (unsigned long)sender);
        if (destination != MESH_BROADCAST_NODE && destination != local_node_number) return;
        if (destination == MESH_BROADCAST_NODE && channel >= MESH_CHANNEL_LIMIT) return;
        uint32_t peer = destination == MESH_BROADCAST_NODE ? 0 : sender;
        if (emoji) {
            store_reaction(sender, packet_id, reply_id, peer, peer ? 0 : (uint8_t)channel, timestamp, payload, payload_size);
            return;
        }
        for (size_t i = 0; packet_id && i < message_count; i++) {
            if (!messages[i].outgoing && messages[i].packet_id == packet_id &&
                messages[i].peer_node == peer && messages[i].channel == (peer ? 0 : channel) && strcmp(messages[i].from, from) == 0) return;
        }
        if (!store_message(from, payload, payload_size, timestamp, peer, false, peer ? 0 : (uint8_t)channel)) return;
        if (sender && sender != local_node_number && sender != MESH_BROADCAST_NODE && !mesh_radio_node_muted(sender))
            message_alerts |= peer ? (1u << 8) : (1u << channel);
        messages[message_count - 1].packet_id = packet_id;
        messages[message_count - 1].reply_id = reply_id;
        persist_message_history();
    } else if (portnum == 8 && payload) {
        decode_waypoint(sender,destination,packet_id,timestamp,channel,payload,payload_size);
    } else if (portnum == 3 && payload) {
        if(destination&&destination!=MESH_BROADCAST_NODE&&destination!=local_node_number)return;
        update_node_position(sender, payload, payload_size);
    } else if (portnum == 67 && payload) {
        decode_telemetry(sender, destination, packet_id, request_id, payload, payload_size);
    } else if (portnum == 70) {
        accept_trace_reply(sender, destination, request_id, payload, payload_size);
    } else if (portnum == 4 && payload) {
        update_node_user(sender, payload, payload_size);
    } else if (portnum == 5) {
        parse_routing_ack(data, size, sender, destination);
    } else if (portnum == 6 && payload && sender == local_node_number &&
               (destination == local_node_number || destination == 0)) {
        parse_admin_response(payload, payload_size, request_id);
    }
}

static bool decode_user(const uint8_t *data, size_t size, mesh_node_t *node, bool *has_long_name) {
    node->user_info_available = true; // Role 0 (CLIENT) may be omitted by proto3.
    size_t offset = 0;
    bool long_name_seen = false;
    char long_name[40] = {0};
    while (offset < size) {
        protobuf_field_t field;
        if (!read_field(data, size, &offset, &field)) return false;
        if (field.wire == 2 && field.field == 1) {
            size_t copy = field.length < sizeof(node->user_id) - 1 ? field.length : sizeof(node->user_id) - 1;
            memcpy(node->user_id, field.value, copy); node->user_id[copy] = '\0';
        } else if (field.wire == 0 && field.field == 7) node->role = (uint32_t)field.integer;
        else if (field.wire == 2 && field.field == 8 && field.length == 32) {
            memcpy(node->public_key, field.value, 32); node->public_key_available = true;
        } else if (field.wire == 2 && field.field == 2) {
            size_t copy = field.length < sizeof(long_name) - 1 ? field.length : sizeof(long_name) - 1;
            memcpy(long_name, field.value, copy);
            long_name[copy] = '\0';
            long_name_seen = true;
            node->names_received = true;
        } else if (field.wire == 2 && field.field == 3) {
            size_t copy = field.length < sizeof(node->short_name) - 1 ? field.length : sizeof(node->short_name) - 1;
            memcpy(node->short_name, field.value, copy);
            node->short_name[copy] = '\0';
            node->names_received = true;
        }
    }
    if (has_long_name) *has_long_name = long_name_seen && long_name[0];
    snprintf(node->name, sizeof(node->name), "%s", long_name_seen && long_name[0] ? long_name : node->short_name);
    return true;
}

static void cache_owner_user(const uint8_t *data,size_t size) {
    mesh_node_t own={0};
    if(local_node_number&&size&&size<=sizeof(owner_user_raw)&&decode_user(data,size,&own,NULL)&&own.names_received){
        memcpy(owner_user_raw,data,size);owner_user_size=size;owner_user_origin=local_node_number;
    }
}

static bool node_name_default(uint32_t number,const char *name) {
    char expected[24];snprintf(expected,sizeof(expected),"Meshtastic %04lx",(unsigned long)(number&0xffff));
    size_t i=0;while(name[i]&&expected[i]){unsigned char a=name[i],b=expected[i];if(a>='A'&&a<='Z')a+=32;if(b>='A'&&b<='Z')b+=32;if(a!=b)return false;i++;}
    return !name[i]&&!expected[i];
}

static void copy_node_identity(mesh_node_t *to, const mesh_node_t *from) {
    memcpy(to->name, from->name, sizeof(to->name));
    memcpy(to->short_name, from->short_name, sizeof(to->short_name));
    memcpy(to->user_id, from->user_id, sizeof(to->user_id));
    memcpy(to->public_key, from->public_key, sizeof(to->public_key));
    to->names_received = from->names_received;
    to->public_key_available = from->public_key_available;
    to->user_info_available = from->user_info_available;
    to->role = from->role;
}

static void cache_local_identity(const mesh_node_t *node) {
    if (!local_node_number || node->number != local_node_number || !node->names_received) return;
    snprintf(local_long_name, sizeof(local_long_name), "%s", node->name);
    snprintf(local_short_name, sizeof(local_short_name), "%s", node->short_name);
    owner_identity_received = true;
}

static void decode_node(const uint8_t *data, size_t size) {
    mesh_node_t node = {.favorite_known = true};
    bool long_name_seen=false;
    const uint8_t *user_data=NULL;size_t user_size=0;
    size_t offset = 0;
    while (offset < size) {
        protobuf_field_t field;
        if (!read_field(data, size, &offset, &field)) return;
        if (field.field == 1 && field.wire == 0) node.number = (uint32_t)field.integer;
        else if (field.field == 2 && field.wire == 2) { if (!decode_user(field.value, field.length, &node, &long_name_seen)) return;user_data=field.value;user_size=field.length; }
        else if (field.field == 3 && field.wire == 2) { decode_position(field.value, field.length, &node); }
        else if (field.field == 6 && field.wire == 2) {
            decode_device_metrics(field.value, field.length, false, &node.metrics);
            node.metrics.cached = true; // NodeInfo has no independent metrics timestamp.
        }
        else if (field.field == 4 && field.wire == 5 && field.length == 4) {
            memcpy(&node.snr, field.value, 4); node.snr_available = isfinite(node.snr);
        }
        else if (field.field == 5 && field.wire == 5 && field.length == 4) memcpy(&node.last_heard, field.value, 4);
        else if (field.field == 10 && field.wire == 0) node.is_favorite = field.integer != 0;
        else if (field.field == 11 && field.wire == 0) node.is_ignored = field.integer != 0;
        else if (field.field == 8 && field.wire == 0) node.via_mqtt = field.integer != 0;
        else if (field.field == 9 && field.wire == 0) {
            node.hops_away = (uint32_t)field.integer;
            node.hops_away_available = true;
        }
    }
    if (!node.number) return;
    management_observe_contact(&node);
    node.first_seen = note_node_seen(node.number);
    if (node.via_mqtt) node.snr_available = false;
    location_observe_node(&node);
    // Only the post-write, nonce-matched config dump can verify a change.
    if (favorite_status.state == MESH_FAVORITE_SAVING && favorite_verify_nonce &&
        config_request_pending && config_nonce == favorite_verify_nonce && node.number == favorite_status.node) {
        favorite_seen = true;
        favorite_observed = node.is_favorite;
    }
    if (node_control.state == MESH_FAVORITE_SAVING && node_control_nonce &&
        config_request_pending && config_nonce == node_control_nonce && node.number == node_control.node) {
        node_control_seen = true; node_control_ignored = node.is_ignored;
    }
    if(node.number==local_node_number&&user_data)cache_owner_user(user_data,user_size);
    cache_local_identity(&node);
    if (!node.name[0]) snprintf(node.name, sizeof(node.name), "!%08lx", (unsigned long)node.number);
    for (size_t i = 0; i < node_count; i++) {
        if (nodes[i].number == node.number) {
            if (!metrics_replace(&nodes[i].metrics, &node.metrics)) node.metrics = nodes[i].metrics;
            else if(!node.is_ignored)record_device_report(node.number,&node.metrics,2);
            if (!node.user_info_available) copy_node_identity(&node, &nodes[i]);
            else {
                // A User containing only a short name/key/role must not discard a learned long name.
                bool keep_known_name=(!long_name_seen&&(!node.short_name[0]||!node_name_default(node.number,nodes[i].name)))||
                    (node_name_default(node.number,node.name)&&!node_name_default(node.number,nodes[i].name));
                if(keep_known_name&&nodes[i].name[0]&&nodes[i].name[0]!='!')snprintf(node.name,sizeof(node.name),"%s",nodes[i].name);
                if(!node.short_name[0])snprintf(node.short_name,sizeof(node.short_name),"%s",nodes[i].short_name);
                node.names_received=node.names_received||nodes[i].names_received;
            }
            if (!node.position_available || (nodes[i].position_available && nodes[i].position_time &&
                (!node.position_time || node.position_time < nodes[i].position_time))) copy_node_position(&node, &nodes[i]);
            node.rssi = nodes[i].rssi; node.rssi_available = nodes[i].rssi_available;
            if (!node.snr_available) { node.snr = nodes[i].snr; node.snr_available = nodes[i].snr_available; }
            if (!node.last_heard) node.last_heard = nodes[i].last_heard;
            if (!node.hops_away_available && nodes[i].hops_away_available) {
                node.hops_away = nodes[i].hops_away;
                node.hops_away_available = true;
            }
            if (node.name[0] == '!' && nodes[i].name[0] != '!') {
                snprintf(node.name, sizeof(node.name), "%s", nodes[i].name);
            }
            if (node.is_ignored) { mesh_tlm_forget(node.number);node.position_available = false; node.metrics = (mesh_device_metrics_t){0};
                node.public_key_available = false; memset(node.public_key, 0, sizeof(node.public_key)); }
            record_node_trail(&node);nodes[i] = node;
            history_revision++;
            return;
        }
    }
    if(node.is_ignored){mesh_tlm_forget(node.number);mesh_trails_forget(node.number);}
    if (node_count < sizeof(nodes) / sizeof(nodes[0])) { if(!node.is_ignored)record_device_report(node.number,&node.metrics,2);record_node_trail(&node);nodes[node_count++] = node; history_revision++; }
}

static void update_node_user(uint32_t number, const uint8_t *data, size_t size) {
    if (!number || number == MESH_BROADCAST_NODE || number == local_node_number || !data || !size) return;
    mesh_node_t announced = {0};
    bool has_long_name = false;
    if (!decode_user(data, size, &announced, &has_long_name)) return;

    for (size_t i = 0; i < node_count; i++) {
        if (nodes[i].number == number) {
            if (!announced.name[0] || (!has_long_name && nodes[i].name[0] != '!' && !node_name_default(number,nodes[i].name))) snprintf(announced.name, sizeof(announced.name), "%s", nodes[i].name);
            if(!announced.short_name[0])snprintf(announced.short_name,sizeof(announced.short_name),"%s",nodes[i].short_name);
            announced.names_received=announced.names_received||nodes[i].names_received;
            copy_node_identity(&nodes[i], &announced);
            history_revision++;
            return;
        }
    }
    // Node announcements can arrive before a NodeInfo snapshot or a valid clock.
    if(node_count<MESH_NODE_LIMIT){
        announced.number=number;announced.first_seen=note_node_seen(number);
        if(!announced.name[0])snprintf(announced.name,sizeof(announced.name),"!%08lx",(unsigned long)number);
        nodes[node_count++]=announced;status.node_count=(uint16_t)node_count;history_revision++;
    }
}

static void update_node_last_heard(uint32_t number, uint32_t timestamp, bool hops_available, uint32_t hops) {
    if (!number || number == local_node_number || timestamp < VALID_UNIX_TIME) return;
    for (size_t i = 0; i < node_count; i++) {
        if (nodes[i].number == number) {
            if (!nodes[i].first_seen) nodes[i].first_seen = note_node_seen(number);
            nodes[i].last_heard = timestamp;
            if (hops_available) {
                nodes[i].hops_away = hops;
                nodes[i].hops_away_available = true;
            }
            return;
        }
    }
    if (node_count >= sizeof(nodes) / sizeof(nodes[0])) return;

    mesh_node_t node = {0};
    node.number = number;
    node.first_seen = note_node_seen(number);
    node.last_heard = timestamp;
    node.hops_away = hops;
    node.hops_away_available = hops_available;
    snprintf(node.name, sizeof(node.name), "!%08lx", (unsigned long)number);
    nodes[node_count++] = node;
    status.node_count = (uint16_t)node_count;
}

static bool manage_field(const uint8_t *,size_t,uint32_t,protobuf_field_t *);
static void decode_mesh_packet(const uint8_t *data, size_t size) {
    size_t offset = 0;
    uint32_t sender = 0;
    uint32_t destination = 0;
    uint32_t channel = 0;
    uint32_t packet_id = 0;
    uint32_t timestamp = 0;
    uint32_t hop_limit = 0;
    uint32_t hop_start = 0;
    bool packet_pki=false;const uint8_t *packet_key=NULL;size_t packet_key_size=0;
    float snr = 0; int32_t rssi = 0;
    bool have_snr = false, have_rssi = false, via_mqtt = false;
    const uint8_t *decoded = NULL;
    size_t decoded_size = 0;
    while (offset < size) {
        protobuf_field_t field;
        if (!read_field(data, size, &offset, &field)) return;
        if (field.field == 1 && field.wire == 5 && field.length == 4) memcpy(&sender, field.value, 4);
        else if (field.field == 2 && field.wire == 5 && field.length == 4) memcpy(&destination, field.value, 4);
        else if (field.field == 3 && field.wire == 0) channel = (uint32_t)field.integer;
        else if (field.field == 6 && field.wire == 5 && field.length == 4) memcpy(&packet_id, field.value, 4);
        else if (field.field == 4 && field.wire == 2) { decoded = field.value; decoded_size = field.length; }
        else if (field.field == 7 && field.wire == 5 && field.length == 4) memcpy(&timestamp, field.value, 4);
        else if (field.field == 9 && field.wire == 0) hop_limit = (uint32_t)field.integer;
        else if (field.field == 15 && field.wire == 0) hop_start = (uint32_t)field.integer;
        else if (field.field == 8 && field.wire == 5) { memcpy(&snr, field.value, 4); have_snr = isfinite(snr); }
        else if (field.field == 12 && field.wire == 0) { rssi = (int32_t)(uint32_t)field.integer; have_rssi = true; }
        else if (field.field == 14 && field.wire == 0) via_mqtt = field.integer != 0;
        else if(field.field==16&&field.wire==2){packet_key=field.value;packet_key_size=field.length;}
        else if(field.field==17&&field.wire==0)packet_pki=field.integer==1;
    }

    if(sender!=local_node_number&&decoded){
        protobuf_field_t port,payload,request;
        if(manage_field(decoded,decoded_size,1,&port)&&port.wire==0&&port.integer==6){
            if(manage_field(decoded,decoded_size,2,&payload)&&payload.wire==2&&manage_field(decoded,decoded_size,6,&request)&&request.wire==5&&request.length==4){
                uint32_t id;memcpy(&id,request.value,4);remote_response(sender,destination,id,packet_pki,packet_key,packet_key_size,payload.value,payload.length);
            }
            return;
        }
    }
    if(decoded)neighbor_receive(decoded,decoded_size,sender,destination,packet_id,timestamp,via_mqtt,channel);
    if(decoded)range_receive_packet(decoded,decoded_size,sender,destination,packet_id,channel,via_mqtt,hop_start,hop_limit,have_snr,snr,have_rssi,rssi);
    // A relay rebroadcast keeps the original sender and packet ID but has a
    // lower hop_limit. Hearing that copy is Meshtastic's implicit ACK for a
    // broadcast message. The locally queued copy is already in our history.
    if (sender == local_node_number && packet_id != 0) {
        waypoint_echo(packet_id,hop_limit);
        // The radio generates Routing ACK/NAK packets locally, with a new packet
        // ID and Data.request_id pointing to our original send. Process them
        // before suppressing our own text echoes and relayed copies.
        if (decoded) {
            size_t data_offset = 0;
            while (data_offset < decoded_size) {
                protobuf_field_t field;
                if (!read_field(decoded, decoded_size, &data_offset, &field)) break;
                if (field.field == 1 && field.wire == 0 && (field.integer == 3 || field.integer == 5 || field.integer == 6 || field.integer == 67)) {
                    // Local admin replies carry owner names and the session key too.
                    decode_data_message(decoded, decoded_size, sender, timestamp, destination, packet_id, channel);
                    return;
                }
            }
        }
        {
            for (size_t i = 0; i < message_count; i++) {
                if (messages[i].peer_node == 0 && messages[i].packet_id == packet_id &&
                    hop_limit < message_hop_limit[i] && messages[i].delivery == MESH_DELIVERY_PENDING) {
                    messages[i].delivery = MESH_DELIVERY_ACKNOWLEDGED;
                    message_sent_us[i] = 0;
                    persist_message_history();
                    break;
                }
            }
        }
        return; // Do not add our own relayed message a second time.
    }
    if (timestamp >= VALID_UNIX_TIME) {
        latest_radio_time = timestamp;
        latest_radio_time_us = esp_timer_get_time();
    }
    // NodeInfo is only a snapshot from the radio's database. Refresh the
    // sender's last-heard time for every received packet so Active stays live.
    uint32_t heard_at = timestamp >= VALID_UNIX_TIME ? timestamp : current_message_timestamp();
    bool hops_available = hop_start >= hop_limit && hop_start > 0;
    update_node_last_heard(sender, heard_at, hops_available, hop_start - hop_limit);
    if (!via_mqtt && (have_snr || have_rssi)) for (size_t i = 0; i < node_count; i++) if (nodes[i].number == sender) {
        if (have_snr) { nodes[i].snr = snr; nodes[i].snr_available = true; }
        if (have_rssi) { nodes[i].rssi = rssi; nodes[i].rssi_available = true; }
        history_revision++;
        break;
    }
    if (decoded) decode_data_message(decoded, decoded_size, sender, timestamp, destination, packet_id, channel);
}

static void decode_my_info(const uint8_t *data, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        protobuf_field_t field;
        if (!read_field(data, size, &offset, &field)) return;
        if (field.field == 1 && field.wire == 0) local_node_number = (uint32_t)field.integer;
    }
    if(nb_origin_changed())neighbor_reset();
    // NodeDB can arrive before MyNodeInfo on some firmware versions.
    for (size_t i = 0; i < node_count; i++) cache_local_identity(&nodes[i]);
}

static void decode_channel(const uint8_t *data, size_t size) {
    if (size > sizeof(channels[0].raw)) return;
    size_t offset = 0;
    uint32_t index = 0;
    mesh_channel_t info = {.loaded = true};
    while (offset < size) {
        protobuf_field_t field;
        if (!read_field(data, size, &offset, &field)) return;
        if (field.field == 1 && field.wire == 0) index = (uint32_t)field.integer;
        else if (field.field == 3 && field.wire == 0) info.role = (uint8_t)field.integer;
        else if (field.field == 2 && field.wire == 2) {
            size_t settings_offset = 0;
            while (settings_offset < field.length) {
                protobuf_field_t setting;
                if (!read_field(field.value, field.length, &settings_offset, &setting)) return;
                if (setting.field == 3 && setting.wire == 2) {
                    size_t n = setting.length < sizeof(info.name) - 1 ? setting.length : sizeof(info.name) - 1;
                    memcpy(info.name, setting.value, n); info.name[n] = '\0';
                } else if (setting.field == 2 && setting.wire == 2) {
                    info.has_key = setting.length != 0 && !(setting.length == 1 && setting.value[0] == 0);
                    info.default_key = setting.length == 1 && setting.value[0] != 0;
                    static const uint8_t default_prefix[15] = {0xd4,0xf1,0xbb,0x3a,0x20,0x29,0x07,0x59,0xf0,0xbc,0xff,0xab,0xcf,0x4e,0x69};
                    if (setting.length == 16 && !memcmp(setting.value, default_prefix, 15)) info.default_key = true;
                } else if (setting.field == 5 && setting.wire == 0) info.uplink_enabled = setting.integer != 0;
                else if (setting.field == 6 && setting.wire == 0) info.downlink_enabled = setting.integer != 0;
                else if (setting.field == 7 && setting.wire == 2) {
                    info.has_position_precision = true;
                    size_t m = 0;
                    while (m < setting.length) {
                        protobuf_field_t module;
                        if (!read_field(setting.value, setting.length, &m, &module)) return;
                        if (module.field == 1) {
                            if (module.wire != 0 || module.integer > UINT32_MAX) return;
                            info.position_precision = (uint32_t)module.integer;
                        }
                    }
                }
            }
        }
    }
    if (index >= MESH_CHANNEL_LIMIT) return;
    info.index = (uint8_t)index;
    info.revision = channels[index].info.revision + 1;
    channels[index].info = info;
    memcpy(channels[index].raw, data, size);
    channels[index].size = size;
    if (index == location_op.channel) location_dump_field(10);
    channel_operation_observe((uint8_t)index);
    if(location_in_dump&&config_request_pending&&keys_nonce==config_nonce&&
        (keys_state==MESH_KEYS_READING||keys_state==MESH_KEYS_SAVING))keys_channels_seen|=1u<<index;
    if (index == admin_channel_index) admin_channel_index = 0;
    if (strcmp(info.name, "admin") == 0) admin_channel_index = (uint8_t)index;
}

static void decode_config(const uint8_t *data, size_t size) {
    size_t check = 0; bool device_seen = false;
    while (check < size) {
        protobuf_field_t f;
        if (!read_field(data,size,&check,&f)) return;
        if (f.field == 1) { if (f.wire != 2 || device_seen) return; device_seen = true; }
    }
    size_t offset = 0;
    while (offset < size) {
        protobuf_field_t config;
        if (!read_field(data, size, &offset, &config)) return;
        if(config.field==8&&config.wire==2){keys_config(config.value,config.length);continue;}
        if (config.field == 2 && config.wire == 2) {
            decode_position_config(config.value, config.length);
            continue;
        }
        if (config.field == 1 && config.wire == 2) {
            if (config.length > sizeof(device_raw)) { device_config.loaded = false; continue; }
            mesh_device_config_t info = {.loaded = true, .revision = device_config.revision + 1};
            size_t inner = 0; unsigned seen = 0;
            while (inner < config.length) {
                protobuf_field_t field;
                if (!read_field(config.value, config.length, &inner, &field)) { device_config.loaded = false; return; }
                if (field.field == 1 || field.field == 6 || field.field == 7) {
                    unsigned bit = 1u << field.field;
                    if (field.wire != 0 || field.integer > UINT32_MAX || (seen & bit)) { device_config.loaded = false; return; }
                    seen |= bit;
                    if (field.field == 1) info.role = (uint32_t)field.integer;
                    else if (field.field == 6) info.rebroadcast_mode = (uint32_t)field.integer;
                    else info.node_info_broadcast_secs = (uint32_t)field.integer;
                }
            }
            memcpy(device_raw, config.value, config.length); device_raw_size = config.length;
            device_config = info; history_revision++;
            continue;
        }
        if (config.field != 6 || config.wire != 2) continue;
        if (config.length > sizeof(lora_raw)) return;
        mesh_lora_config_t info = {.loaded = true, .revision = lora_config.revision + 1};
        size_t inner = 0;
        while (inner < config.length) {
            protobuf_field_t field;
            if (!read_field(config.value, config.length, &inner, &field)) return;
            if (field.wire != 0) continue;
            if (field.field == 1) info.use_preset = field.integer != 0;
            else if (field.field == 2) info.modem_preset = field.integer;
            else if (field.field == 7) info.region = field.integer;
            else if (field.field == 8) {
                if (field.integer > 7) return;
                info.hop_limit = field.integer;
            }
        }
        memcpy(lora_raw, config.value, config.length);
        lora_raw_size = config.length;
        lora_config = info;
    }
}

static void decode_from_radio(const uint8_t *data, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        protobuf_field_t field;
        if (!read_field(data, size, &offset, &field)) return;

        if(field.wire==2)cfg_from(field.field,field.value,field.length);

        // FromRadio.node_info is field 4 (length-delimited); config_complete_id
        // is field 7 (varint). These are sufficient for the initial sync view.
        if (field.field == 4 && field.wire == 2) {
            location_in_dump = true;
            decode_node(field.value, field.length);
            location_in_dump = false;
            status.node_count = (uint16_t)node_count;
            continue;
        }
        if (field.field == 13 && field.wire == 2) {
            decode_metadata(field.value, field.length);
            continue;
        }
        if (field.field == 3 && field.wire == 2) {
            decode_my_info(field.value, field.length);
            continue;
        }
        if (field.field == 5 && field.wire == 2) {
            location_in_dump = true;
            decode_config(field.value, field.length);
            location_in_dump = false;
            continue;
        }
        if (field.field == 9 && field.wire == 2) {
            neighbor_module_dump(field.value,field.length);
            decode_telemetry_config(field.value, field.length, 0);
            continue;
        }
        if (field.field == 10 && field.wire == 2) {
            location_in_dump = true;
            decode_channel(field.value, field.length);
            location_in_dump = false;
            continue;
        }
        if (field.field == 2 && field.wire == 2) {
            decode_mesh_packet(field.value, field.length);
            continue;
        }
        if (field.field == 7 && field.wire == 0) {
            if (config_request_pending && field.integer == config_nonce) {
                cfg_complete((uint32_t)field.integer);
                keys_complete((uint32_t)field.integer);
                management_complete((uint32_t)field.integer);
                location_dump_complete((uint32_t)field.integer);
                channel_operation_complete((uint32_t)field.integer);
                if (node_control.state == MESH_FAVORITE_SAVING && node_control_nonce == config_nonce) {
                    bool verified = node_control.kind == MESH_NODE_REMOVE ? !node_control_seen :
                        node_control_seen && node_control_ignored == (node_control.kind == MESH_NODE_IGNORE);
                    node_control.state = verified ? MESH_FAVORITE_SAVED : MESH_FAVORITE_FAILED;
                    if (verified && node_control.kind == MESH_NODE_REMOVE) {
                        mesh_trails_forget(node_control.node);
                        for (size_t i = 0; i < node_count; i++) if (nodes[i].number == node_control.node) {
                            memmove(&nodes[i], &nodes[i+1], (node_count-i-1)*sizeof(nodes[0])); node_count--; status.node_count = (uint16_t)node_count; break;
                        }
                    }
                    history_revision++;
                }
                if (favorite_status.state == MESH_FAVORITE_SAVING && favorite_verify_nonce == config_nonce) {
                    favorite_status.state = favorite_seen && favorite_observed == favorite_status.desired ?
                        MESH_FAVORITE_SAVED : MESH_FAVORITE_FAILED;
                    history_revision++;
                }
                status.synchronized = true;
                status.reconnecting = false;
                config_request_pending = false;
                config_retry_delay_us = 10000000;
            }
            continue;
        }
        if (field.field == 8 && field.wire == 0 && field.integer) {
            begin_radio_reconnect();
            continue;
        }
    }
}

static bool append_varint(uint8_t *out, size_t capacity, size_t *pos, uint64_t value) {
    do {
        if (*pos >= capacity) return false;
        uint8_t byte = (uint8_t)(value & 0x7f);
        value >>= 7;
        out[(*pos)++] = value ? (uint8_t)(byte | 0x80) : byte;
    } while (value);
    return true;
}

static bool append_bytes_field(uint8_t *out, size_t capacity, size_t *pos, uint32_t field,
                               const uint8_t *data, size_t size) {
    if (!append_varint(out, capacity, pos, ((uint64_t)field << 3) | 2) ||
        !append_varint(out, capacity, pos, size) || size > capacity - *pos) return false;
    memcpy(out + *pos, data, size);
    *pos += size;
    return true;
}

static bool send_admin_payload_with_id(const uint8_t *payload, size_t payload_size, uint32_t *out_id) {
    if (!status.started || status.reconnecting || !local_node_number || !payload || payload_size == 0 || payload_size > 233) return false;
    uint8_t data[280];
    size_t d = 0;
    if (!append_varint(data, sizeof(data), &d, 8) ||
        !append_varint(data, sizeof(data), &d, 6) ||
        !append_bytes_field(data, sizeof(data), &d, 2, payload, payload_size) ||
        !append_varint(data, sizeof(data), &d, 24) ||
        !append_varint(data, sizeof(data), &d, 1)) return false; // Data.want_response.

    uint8_t packet[320];
    size_t p = 0;
    if (p + 5 > sizeof(packet)) return false;
    packet[p++] = 0x15; // MeshPacket.to (fixed32).
    memcpy(packet + p, &local_node_number, sizeof(local_node_number)); p += sizeof(local_node_number);
    if (admin_channel_index &&
        (!append_varint(packet, sizeof(packet), &p, 24) ||
         !append_varint(packet, sizeof(packet), &p, admin_channel_index))) return false;
    if (!append_bytes_field(packet, sizeof(packet), &p, 4, data, d)) return false;
    if (p + 6 > sizeof(packet)) return false;
    packet[p++] = 0x35; // MeshPacket.id (fixed32).
    uint32_t id = next_packet_id++;
    if (!id) id = next_packet_id++;
    memcpy(packet + p, &id, sizeof(id)); p += sizeof(id);
    packet[p++] = 0x50; packet[p++] = 0x01; // Ask for a reliable ACK/NAK.
    packet[p++] = 0x58; packet[p++] = 0x46; // RELIABLE priority.

    uint8_t frame[FRAME_MAX + 4];
    size_t frame_size = 4;
    if (!append_bytes_field(frame, sizeof(frame), &frame_size, 1, packet, p)) return false;
    size_t body_size = frame_size - 4;
    if (body_size > FRAME_MAX) return false;
    frame[0] = 0x94; frame[1] = 0xc3;
    frame[2] = (uint8_t)(body_size >> 8); frame[3] = (uint8_t)body_size;
    if (uart_write_bytes(RADIO_UART, frame, frame_size) != (int)frame_size) return false;
    if (out_id) *out_id = id;
    return true;
}

static bool send_admin_payload(const uint8_t *payload, size_t payload_size) {
    return send_admin_payload_with_id(payload, payload_size, NULL);
}

static bool valid_radio_payload(const uint8_t *data, size_t size) {
    size_t offset = 0;
    bool known = false;
    while (offset < size) {
        protobuf_field_t field;
        if (!read_field(data, size, &offset, &field)) return false;
        if ((field.wire == 2 && (field.field == 2 || field.field == 3 || field.field == 4 || field.field == 5 || field.field == 9 || field.field == 10 || field.field == 13)) ||
            (field.wire == 0 && (field.field == 7 || field.field == 8))) known = true;
    }
    return known;
}

static void consume_frames(void) {
    for (;;) {
        size_t start = 0;
        while (start + 1 < rx_size && (rx_buffer[start] != 0x94 || rx_buffer[start + 1] != 0xc3)) start++;
        if (start) {
            memmove(rx_buffer, rx_buffer + start, rx_size - start);
            rx_size -= start;
        }
        if (rx_size < 4) return;

        size_t payload_size = ((size_t)rx_buffer[2] << 8) | rx_buffer[3];
        if (payload_size == 0 || payload_size > FRAME_MAX) {
            memmove(rx_buffer, rx_buffer + 1, --rx_size);
            continue;
        }
        if (rx_size < payload_size + 4) return;

        if (valid_radio_payload(rx_buffer + 4, payload_size)) {
            status.frames_received++;
            last_radio_activity_us = esp_timer_get_time();
            probe_started_us = 0;
            decode_from_radio(rx_buffer + 4, payload_size);
        }
        size_t frame_size = payload_size + 4;
        memmove(rx_buffer, rx_buffer + frame_size, rx_size - frame_size);
        rx_size -= frame_size;
    }
}

static bool start_radio_uart(void) {
    esp_err_t err = bsp_power_set_radio_state(BSP_POWER_RADIO_STATE_APPLICATION);
    if (err != ESP_OK) return false;
    const uart_config_t config = {
        .baud_rate = RADIO_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    err = uart_param_config(RADIO_UART, &config);
    if (err == ESP_OK) err = uart_set_pin(RADIO_UART, 16, 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err == ESP_OK && !uart_installed) {
        err = uart_driver_install(RADIO_UART, RX_BUFFER_SIZE * 2, RX_BUFFER_SIZE, 0, NULL, 0);
        if (err == ESP_OK) uart_installed = true;
    }
    if (err != ESP_OK) return false;
    status.started = true;
    uart_flush_input(RADIO_UART);
    return true;
}

static void begin_radio_reconnect(void) {
    setup_reconnect();
    neighbor_reconnect();
    owner_user_size=0;owner_user_origin=0;
    mesh_radio_keys_clear();keys_nonce=0;
    cfg_nonce=0;mesh_keys_wipe(&cfg_snapshot,sizeof(cfg_snapshot));
    telemetry_config.loaded = false; telemetry_module_size = telemetry_config_size = 0;
    telemetry_enable_seen = false;
    telemetry_config_op.read_id = 0; telemetry_config_op.request_sent = false;
    for(size_t i=0;i<8;i++)if(telemetry_requests[i].result.state==MESH_TELEMETRY_WAITING) {
        telemetry_requests[i].result.state=MESH_TELEMETRY_INTERRUPTED;history_revision++;
    }
    position_config.loaded = false; position_raw_size = 0; last_position_request_us = 0;
    location_op.nonce = 0;
    location_op.config_seen = location_op.node_seen = location_op.channel_seen = false;
    device_config.loaded = false; device_raw_size = 0; last_device_request_us = 0;
    role_read_id = 0;
    radio_metadata = (mesh_radio_metadata_t){0};
    last_metadata_request_us = 0;
    if (node_control.state == MESH_FAVORITE_SAVING) { node_control.state = MESH_FAVORITE_INTERRUPTED; history_revision++; }
    if (favorite_status.state == MESH_FAVORITE_SAVING) {
        favorite_status.state = MESH_FAVORITE_INTERRUPTED;
        history_revision++;
    }
    for (size_t i = 0; i < 8; i++) if (traces[i].state == MESH_TRACE_WAITING) {
        traces[i].state = MESH_TRACE_INTERRUPTED;
        traces[i].completed_at = current_message_timestamp();
        history_revision++;
    }
    if (channel_operation.state == MESH_FAVORITE_SAVING) channel_operation_data.nonce = 0;
    status.synchronized = false;
    status.reconnecting = true;
    config_request_pending = false; // Reject completion from the previous session.
    config_retry_delay_us = 10000000;
    next_config_retry_us = esp_timer_get_time();
    probe_started_us = 0;
    memset(session_passkey, 0, sizeof(session_passkey));
    session_passkey_size = 0;
    owner_config_received = false;
    last_owner_request_us = owner_verify_at_us = 0;
    admin_channel_index = 0;
    for (size_t i = 0; i < MESH_CHANNEL_LIMIT; i++) {
        channels[i].info.loaded = false;
        channels[i].size = 0;
    }
    memset(last_channel_request_us, 0, sizeof(last_channel_request_us));
    channel_verify_index = -1;
    channel_verify_at_us = 0;
    lora_config.loaded = false;
    lora_raw_size = 0;
    last_lora_request_us = lora_verify_at_us = 0;
}

bool mesh_radio_start(void) {
    setup_reset();
    mesh_tlm_reset();mesh_trails_reset();
    keys_state=MESH_KEYS_IDLE;mesh_radio_keys_clear();mesh_keys_wipe(keys_desired_private,32);
    owner_user_size=0;owner_user_origin=0;node_info_requested=false;
    message_alerts = 0;
    telemetry_enable_seen = false;
    telemetry_config = (mesh_telemetry_config_t){0}; telemetry_save = (mesh_telemetry_save_t){0};
    telemetry_module_size = telemetry_config_size = 0; memset(&telemetry_config_op, 0, sizeof(telemetry_config_op));
    memset(telemetry_requests,0,sizeof(telemetry_requests));telemetry_replace_index=0;
    last_telemetry_request_us=0;telemetry_request_sent=false;
    local_activity = (mesh_device_metrics_t){0}; local_activity_node=0;
    position_config = (mesh_position_config_t){0}; position_raw_size = 0; last_position_request_us = 0;
    location_save = (mesh_location_save_t){0}; memset(&location_op, 0, sizeof(location_op));
    device_config = (mesh_device_config_t){0}; device_raw_size = 0;
    role_save = (mesh_role_save_t){0}; role_write_id = role_read_id = role_origin = 0;
    role_started_us = role_read_at_us = last_device_request_us = 0;
    radio_metadata = (mesh_radio_metadata_t){0};
    last_metadata_request_us = 0;
    channel_operation = (mesh_channel_operation_t){0};
    node_control = (mesh_node_control_status_t){0};
    favorite_status = (mesh_favorite_status_t){0};
    favorite_request_id = favorite_verify_nonce = 0;
    favorite_started_us = favorite_verify_at_us = session_passkey_received_us = 0;
    favorite_seen = favorite_observed = false;
    memset(traces, 0, sizeof(traces));
    memset(trace_started_us, 0, sizeof(trace_started_us));
    last_trace_started_us = 0; trace_replace_index = 0;
    memset(&status, 0, sizeof(status));
    rx_size = 0;
    node_count = 0;
    message_count = 0;
    reaction_count = 0;
    memset(message_unread, 0, sizeof(message_unread));
    config_nonce = esp_random();
    config_request_pending = false;
    last_radio_activity_us = last_rx_byte_us = esp_timer_get_time();
    last_heartbeat_us = last_radio_activity_us;
    probe_started_us = 0;
    next_config_retry_us = 0;
    config_retry_delay_us = 10000000;
    latest_radio_time = 0;
    latest_radio_time_us = 0;
    memset(message_sent_us, 0, sizeof(message_sent_us));
    memset(message_hop_limit, 0, sizeof(message_hop_limit));
    local_node_number = 0;
    neighbor_reset();
    admin_channel_index = 0;
    local_long_name[0] = '\0';
    local_short_name[0] = '\0';
    session_passkey_size = 0;
    owner_config_received = false;
    owner_identity_received = false;
    last_owner_request_us = 0;
    owner_verify_at_us = 0;
    memset(channels, 0, sizeof(channels));
    memset(last_channel_request_us, 0, sizeof(last_channel_request_us));
    channel_verify_index = -1;
    channel_verify_at_us = 0;
    memset(&lora_config, 0, sizeof(lora_config));
    lora_raw_size = 0;
    last_lora_request_us = lora_verify_at_us = 0;
    initialize_message_history_storage();
    load_node_preferences();
    load_node_aliases();
    trust_load();
    mesh_radio_management_clear();
    restore_node_seen();
    restore_message_history();
    restore_waypoints();
    next_packet_id = esp_random();
    if (next_packet_id == 0) next_packet_id = 1;

    if (!start_radio_uart()) {
        begin_radio_reconnect();
        next_config_retry_us = esp_timer_get_time() + config_retry_delay_us;
        ESP_LOGW(TAG, "Radio unavailable; retrying automatically");
        return false;
    }

    status.started = true;
    last_config_request_us = 0;
    uart_flush_input(RADIO_UART);
    ESP_LOGI(TAG, "Radio UART ready; requesting node database");
    return true;
}

void mesh_radio_request_config(void) {
    if (!status.started) return;
    // A node absent from the new dump must not retain a supposedly confirmed
    // favorite flag from an older database. RF announcements do not set this.
    for (size_t i = 0; i < node_count; i++) nodes[i].favorite_known = false;
    history_revision++;
    // Meshtastic serial frame: magic 94 C3, two-byte payload length, then
    // ToRadio.want_config_id (field 3), with a new nonce for every attempt.
    uint8_t request[16] = {0x94, 0xc3, 0, 0, 0x18};
    size_t length = 5;
    if (++config_nonce == 0) config_nonce++;
    append_varint(request, sizeof(request), &length, config_nonce);
    request[3] = (uint8_t)(length - 4);
    status.synchronized = false;
    config_request_pending = true;
    cfg_capture_reset(config_nonce);
    if(keys_state==MESH_KEYS_READING||keys_state==MESH_KEYS_SAVING){keys_nonce=config_nonce;keys_security_seen=false;keys_channels_seen=0;}

    if (channel_operation.state == MESH_FAVORITE_SAVING && esp_timer_get_time() >= channel_operation_data.read_at) {
        channel_operation_data.nonce = config_nonce; channel_operation_data.seen = 0; channel_operation_data.matched = true;
    }
    if (location_save.state == MESH_LOCATION_SAVING && esp_timer_get_time() >= location_op.read_at) {
        location_op.nonce = config_nonce;
        location_op.config_seen = location_op.node_seen = location_op.channel_seen = false;
    }
    uart_write_bytes(RADIO_UART, request, length);
    last_config_request_us = esp_timer_get_time();
    next_config_retry_us = last_config_request_us + config_retry_delay_us;
    if (status.reconnecting) status.reconnect_attempts++;
}

void mesh_radio_poll(void) {
    int64_t now_us = esp_timer_get_time();
    setup_poll(now_us);
    neighbor_poll(now_us);
    range_poll(now_us);
    if (history_fs_ready && node_seen_dirty && now_us >= node_seen_save_at_us) mesh_radio_flush_node_cache();
    if (!status.started) {
        if (now_us < next_config_retry_us) return;
        status.reconnect_attempts++;
        if (!start_radio_uart()) {
            config_retry_delay_us = config_retry_delay_us < 30000000 ? config_retry_delay_us * 2 : 30000000;
            if (config_retry_delay_us > 30000000) config_retry_delay_us = 30000000;
            next_config_retry_us = now_us + config_retry_delay_us;
            return;
        }
        next_config_retry_us = now_us;
    }
    // Drain input before checking deadlines so an already-arrived reply wins.
    if (rx_size && now_us - last_rx_byte_us > 2000000) rx_size = 0;
    uint8_t incoming[128];
    // Drain a bounded burst. Reduced-idle polling must still keep up with
    // 38400-baud input rather than processing only 128 bytes per wake.
    for (unsigned batch = 0; batch < 16; batch++) {
        int received = uart_read_bytes(RADIO_UART, incoming, sizeof(incoming), 0);
        if (received <= 0) {
            if (received < 0 && !status.reconnecting) begin_radio_reconnect();
            break;
        }
        last_rx_byte_us = now_us;
        size_t available = RX_BUFFER_SIZE - rx_size;
        if ((size_t)received > available) rx_size = 0;
        memcpy(rx_buffer + rx_size, incoming, received);
        rx_size += (size_t)received;
        consume_frames();
    }

    trust_poll(now_us);
    management_poll(now_us);
    keys_poll(now_us);
    cfg_poll(now_us);
    waypoint_poll(now_us);
    channel_operation_poll(now_us);
    location_poll(now_us);
    telemetry_request_poll(now_us);
    telemetry_config_poll(now_us);
    if (radio_metadata.pending && now_us - last_metadata_request_us >= 10000000) {
        radio_metadata.pending = false;
        history_revision++;
    }
    for (size_t i = 0; i < 8; i++) if (traces[i].state == MESH_TRACE_WAITING &&
        now_us - trace_started_us[i] >= 120000000) {
        traces[i].state = MESH_TRACE_TIMEOUT;
        traces[i].completed_at = current_message_timestamp();
        history_revision++;
    }

    if (favorite_status.state == MESH_FAVORITE_SAVING) {
        if (now_us - favorite_started_us >= 20000000) {
            favorite_status.state = MESH_FAVORITE_TIMEOUT;
            history_revision++;
        } else if (!favorite_verify_nonce && now_us >= favorite_verify_at_us) {
            mesh_radio_request_config();
            favorite_verify_nonce = config_nonce;
            favorite_seen = false;
        }
    }

    if (node_control.state == MESH_FAVORITE_SAVING) {
        if (now_us - node_control_started_us >= 20000000) { node_control.state = MESH_FAVORITE_TIMEOUT; history_revision++; }
        else if (!node_control_nonce && now_us >= node_control_verify_us) {
            mesh_radio_request_config(); node_control_nonce = config_nonce; node_control_seen = false;
        }
    }
    // Keep the serial API session awake even when mesh packets are arriving
    // continuously. This local heartbeat has no expected reply and is not
    // evidence that the radio is online.
    if (status.synchronized && now_us - last_heartbeat_us >= 15000000) {
        static const uint8_t heartbeat[] = {0x94, 0xc3, 0, 2, 0x3a, 0};
        last_heartbeat_us = now_us;
        if (uart_write_bytes(RADIO_UART, heartbeat, sizeof(heartbeat)) != (int)sizeof(heartbeat)) begin_radio_reconnect();
    }

    // Quiet mesh traffic is normal. Ask the local radio for an owner reply
    // before declaring the serial connection lost; this never transmits text.
    if (status.synchronized && !probe_started_us && now_us - last_radio_activity_us >= 20000000) {
        if (!local_node_number) begin_radio_reconnect();
        else {
            last_owner_request_us = 0;
            mesh_radio_request_owner();
            probe_started_us = now_us;
        }
    }
    if (probe_started_us && now_us - probe_started_us >= 10000000) begin_radio_reconnect();
    // Fetch the radio's owner and seed the admin session automatically once
    // MyNodeInfo arrives after startup. Settings can then display current
    // radio names immediately, even if MeshDeck has just been rebooted.
    if (!status.reconnecting && local_node_number && !owner_config_received) mesh_radio_request_owner();
    if (!status.reconnecting && local_node_number && !lora_config.loaded) mesh_radio_request_lora();
    if (role_save.state == MESH_ROLE_SAVING) {
        if (now_us - role_started_us >= 60000000 || (local_node_number && local_node_number != role_origin)) {
            role_save.state = MESH_ROLE_TIMEOUT;
            history_revision++;
        } else if (status.synchronized && !status.reconnecting && now_us >= role_read_at_us) {
            last_device_request_us = 0;
            mesh_radio_request_device();
            role_read_at_us = now_us + 3000000;
        }
    }
    if (lora_verify_at_us && now_us >= lora_verify_at_us) {
        lora_verify_at_us = 0;
        last_lora_request_us = 0;
        mesh_radio_request_lora();
    }

    for (size_t i = 0; i < message_count; i++) {
        if (messages[i].delivery == MESH_DELIVERY_PENDING && message_sent_us[i] &&
            now_us - message_sent_us[i] >= 90000000) {
            messages[i].delivery = MESH_DELIVERY_FAILED;
            messages[i].failure = MESH_FAILURE_TIMEOUT;
            message_sent_us[i] = 0;
            persist_message_history();
        }
    }
    if (owner_verify_at_us && now_us >= owner_verify_at_us) {
        owner_verify_at_us = 0;
        last_owner_request_us = 0;
        mesh_radio_request_owner();
    }

    if (channel_verify_index >= 0 && now_us >= channel_verify_at_us) {
        uint8_t index = (uint8_t)channel_verify_index;
        channel_verify_index = -1;
        last_channel_request_us[index] = 0;
        mesh_radio_request_channel(index);
    }

    // Keep asking until the radio returns config_complete_id. Receiving other
    // FromRadio frames (for example log or packet frames) does not mean that
    // the node/config dump has started or finished.
    if (!status.synchronized && now_us >= next_config_retry_us) {
        if (config_request_pending && !status.reconnecting) begin_radio_reconnect();
        mesh_radio_request_config();
        if (config_retry_delay_us < 30000000) {
            config_retry_delay_us *= 2;
            if (config_retry_delay_us > 30000000) config_retry_delay_us = 30000000;
        }
    }
}

mesh_radio_status_t mesh_radio_status(void) {
    return status;
}

size_t mesh_radio_node_count(void) { return node_count; }

bool mesh_radio_get_node(size_t index, mesh_node_t *out_node) {
    if (!out_node || index >= node_count) return false;
    *out_node = nodes[index];
    node_apply_alias(out_node);
    return true;
}

size_t mesh_radio_message_count(void) { return message_count; }

bool mesh_radio_get_message(size_t index, mesh_message_t *out_message) {
    if (!out_message || index >= message_count) return false;
    *out_message = messages[index];
    return true;
}

void mesh_radio_message_sender_name(const mesh_message_t *message, char *out, size_t size) {
    if (!out || !size) return;
    if (!message) { snprintf(out, size, "Unknown"); return; }
    if (message->outgoing) { snprintf(out, size, "You"); return; }
    uint32_t number = message->peer_node;
    // Channel history already stores the full sender ID as ! + eight hex digits.
    // Resolve at draw time so old messages follow later NodeInfo/name updates.
    if (!number && message->from[0] == '!' && message->from[9] == '\0') {
        bool valid = true;
        for (size_t i = 1; i <= 8; i++) {
            unsigned char c = message->from[i];
            unsigned nibble;
            if (c >= '0' && c <= '9') nibble = c - '0';
            else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') nibble = c - 'A' + 10;
            else { valid = false; break; }
            number = (number << 4) | nibble;
        }
        if (!valid) number = 0;
    }
    const char *alias=mesh_radio_nickname(number);
    if(alias[0]){snprintf(out,size,"%s",alias);return;}
    for (size_t i = 0; number && number != MESH_BROADCAST_NODE && i < node_count; i++) {
        if (nodes[i].number != number) continue;
        if (nodes[i].name[0]) { snprintf(out, size, "%.39s", nodes[i].name); return; }
        if (nodes[i].short_name[0]) { snprintf(out, size, "%.4s", nodes[i].short_name); return; }
        break;
    }
    if (number && number != MESH_BROADCAST_NODE) snprintf(out, size, "!%08lx", (unsigned long)number);
    else snprintf(out, size, "%.*s", (int)sizeof(message->from), message->from[0] ? message->from : "Unknown");
}

static bool message_in_thread(const mesh_message_t *message, uint32_t peer, uint8_t channel) {
    return message->peer_node == peer && (peer || message->channel == channel);
}

static size_t unread_in_thread(uint32_t peer, uint8_t channel) {
    size_t count = 0;
    for (size_t i = 0; i < message_count; i++) {
        if (message_unread[i] && !messages[i].outgoing && message_in_thread(&messages[i], peer, channel)) count++;
    }
    return count;
}

size_t mesh_radio_unread_channel(uint8_t channel) {
    return channel < MESH_CHANNEL_LIMIT ? unread_in_thread(0, channel) : 0;
}

size_t mesh_radio_unread_direct(uint32_t peer) {
    return peer && peer != MESH_BROADCAST_NODE ? unread_in_thread(peer, 0) : 0;
}

size_t mesh_radio_unread_total(bool direct) {
    size_t count = 0;
    for (size_t i = 0; i < message_count; i++) {
        if (message_unread[i] && !messages[i].outgoing && (messages[i].peer_node != 0) == direct) count++;
    }
    return count;
}

void mesh_radio_mark_read(uint32_t peer, uint8_t channel) {
    bool changed = false;
    for (size_t i = 0; i < message_count; i++) {
        if (message_unread[i] && message_in_thread(&messages[i], peer, channel)) {
            message_unread[i] = false;
            changed = true;
        }
    }
    if (changed) persist_message_history();
}

bool mesh_radio_first_unread(uint32_t peer,uint8_t channel,size_t *index) {
    if(peer==MESH_BROADCAST_NODE||(!peer&&channel>=MESH_CHANNEL_LIMIT))return false;
    size_t position=0;
    for(size_t i=0;i<message_count;i++)if(message_in_thread(&messages[i],peer,channel)) {
        if(message_unread[i]&&!messages[i].outgoing){if(index)*index=position;return true;}
        position++;
    }
    return false;
}
void mesh_radio_mark_read_range(uint32_t peer,uint8_t channel,size_t first,size_t end) {
    if(peer==MESH_BROADCAST_NODE||(!peer&&channel>=MESH_CHANNEL_LIMIT)||end<=first)return;
    size_t position=0;bool changed=false;
    for(size_t i=0;i<message_count&&position<end;i++)if(message_in_thread(&messages[i],peer,channel)) {
        if(position>=first&&message_unread[i]&&!messages[i].outgoing){message_unread[i]=false;changed=true;}
        position++;
    }
    if(changed)persist_message_history();
}

static bool reaction_matches(const mesh_reaction_t *reaction, const mesh_message_t *message) {
    return message && message->packet_id && reaction->reply_id == message->packet_id &&
           reaction->peer_node == message->peer_node && (message->peer_node || reaction->channel == message->channel);
}

size_t mesh_radio_reaction_count(const mesh_message_t *message) {
    size_t count = 0;
    for (size_t i = 0; i < reaction_count; i++) if (reaction_matches(&reactions[i], message)) count++;
    return count;
}

bool mesh_radio_get_reaction(const mesh_message_t *message, size_t index, mesh_reaction_t *out) {
    if (!out) return false;
    for (size_t i = 0; i < reaction_count; i++) {
        if (reaction_matches(&reactions[i], message) && !index--) { *out = reactions[i]; return true; }
    }
    return false;
}

static bool transmit_chat_data(uint32_t destination, uint8_t channel, const uint8_t *data,
                               size_t data_size, uint32_t *out_id, uint8_t *out_hops) {
    if (!status.started || status.reconnecting || (!status.synchronized && status.frames_received == 0)) return false;
    uint8_t packet[320];
    size_t p = 0;
    packet[p++] = 0x15; // MeshPacket.to, fixed32
    memcpy(packet + p, &destination, 4); p += 4;
    if (channel) { packet[p++] = 0x18; packet[p++] = channel; }
    if (!append_bytes_field(packet, sizeof(packet), &p, 4, data, data_size)) return false; // MeshPacket.decoded
    packet[p++] = 0x35; // MeshPacket.id is fixed32 (field 6, wire type 5).
    uint32_t id = next_packet_id++;
    if (!id) id = next_packet_id++;
    memcpy(packet + p, &id, sizeof(id));
    p += sizeof(id);
    uint8_t hops = lora_config.loaded ? lora_config.hop_limit : 3;
    packet[p++] = 0x48; packet[p++] = hops; // Use the radio's configured hop limit.
    packet[p++] = 0x50; packet[p++] = 0x01; // MeshPacket.want_ack = true; broadcasts use implicit ACKs.
    packet[p++] = 0x58; packet[p++] = 0x46; // MeshPacket.priority = RELIABLE (70).

    uint8_t frame[FRAME_MAX + 4];
    size_t frame_size = 4;
    if (!append_bytes_field(frame, sizeof(frame), &frame_size, 1, packet, p) || frame_size - 4 > FRAME_MAX) return false;
    size_t body_size = frame_size - 4;
    frame[0] = 0x94; frame[1] = 0xc3;
    frame[2] = (uint8_t)(body_size >> 8); frame[3] = (uint8_t)body_size;
    if (uart_write_bytes(RADIO_UART, frame, frame_size) != (int)frame_size) return false;
    *out_id = id;
    *out_hops = hops;
    return true;
}

bool mesh_radio_request_node_info(uint32_t number) {
    mesh_node_t target;
    if(!number||number==MESH_BROADCAST_NODE||number==local_node_number||!mesh_radio_get_node_by_number(number,&target)||target.is_ignored||
        !status.synchronized||status.reconnecting||config_request_pending||keys_state==MESH_KEYS_SAVING)return false;
    int64_t now=esp_timer_get_time();
    if(node_info_requested&&now-node_info_requested_at<60000000)return false;
    if(!owner_user_size||owner_user_origin!=local_node_number){mesh_radio_request_owner();return false;}
    // Send our complete actual User, preserving license/hardware/public-key/unknown fields.
    uint8_t data[224];size_t n=0;uint32_t id;uint8_t hops;
    if(!append_varint(data,sizeof(data),&n,8)||!append_varint(data,sizeof(data),&n,4)||
        !append_bytes_field(data,sizeof(data),&n,2,owner_user_raw,owner_user_size)||
        !append_varint(data,sizeof(data),&n,24)||!append_varint(data,sizeof(data),&n,1)||
        !transmit_chat_data(number,0,data,n,&id,&hops))return false;
    node_info_requested=true;node_info_requested_at=now;return true;
}

mesh_trace_start_t mesh_radio_start_traceroute(uint32_t node) {
    if (!node || node == MESH_BROADCAST_NODE || !local_node_number || node == local_node_number) return MESH_TRACE_START_INVALID;
    if (!status.started || status.reconnecting || (!status.synchronized && !status.frames_received)) return MESH_TRACE_START_OFFLINE;
    for (size_t i = 0; i < 8; i++) if (traces[i].state == MESH_TRACE_WAITING) return MESH_TRACE_START_BUSY;
    int64_t now = esp_timer_get_time();
    if (last_trace_started_us && now - last_trace_started_us < 30000000) return MESH_TRACE_START_COOLDOWN;
    // Empty RouteDiscovery payload, port 70, Data.want_response = true.
    const uint8_t data[] = {0x08, 0x46, 0x12, 0x00, 0x18, 0x01};
    uint32_t id; uint8_t hops;
    if (!transmit_chat_data(node, 0, data, sizeof(data), &id, &hops)) return MESH_TRACE_START_WRITE_FAILED;
    size_t slot = 8;
    for (size_t i = 0; i < 8; i++) if (traces[i].node == node) { slot = i; break; }
    if (slot == 8) for (size_t i = 0; i < 8; i++) if (!traces[i].node) { slot = i; break; }
    if (slot == 8) { slot = trace_replace_index; trace_replace_index = (trace_replace_index + 1) % 8; }
    traces[slot] = (mesh_trace_result_t){.node = node, .origin = local_node_number, .request_id = id,
        .requested_at = current_message_timestamp(), .state = MESH_TRACE_WAITING};
    trace_started_us[slot] = last_trace_started_us = now;
    history_revision++;
    return MESH_TRACE_START_OK;
}

bool mesh_radio_get_traceroute(uint32_t node, mesh_trace_result_t *out) {
    if (!out) return false;
    for (size_t i = 0; i < 8; i++) if (traces[i].node == node && node) { *out = traces[i]; return true; }
    return false;
}

unsigned mesh_radio_traceroute_elapsed(uint32_t node) {
    for (size_t i = 0; i < 8; i++) if (traces[i].node == node && node) {
        int64_t elapsed = esp_timer_get_time() - trace_started_us[i];
        return elapsed > 0 ? (unsigned)(elapsed / 1000000) : 0;
    }
    return 0;
}

static bool send_text_with_reply(uint32_t destination, uint8_t channel, const char *text, uint32_t reply_id) {
    if (!text || !text[0] || (message_count >= history_limit && history_victim(NULL) == SIZE_MAX)) return false;
    size_t text_size = strlen(text);
    if (text_size > MESH_TEXT_MAX_LENGTH) return false;
    uint8_t data[256];
    size_t pos = 0;
    data[pos++] = 0x08; data[pos++] = 0x01;
    if (!append_bytes_field(data, sizeof(data), &pos, 2, (const uint8_t *)text, text_size)) return false;
    if (reply_id) { data[pos++] = 0x3d; memcpy(data + pos, &reply_id, 4); pos += 4; }
    uint32_t id; uint8_t hops;
    if (!transmit_chat_data(destination, channel, data, pos, &id, &hops)) return false;
    if (!store_message("You", (const uint8_t *)text, text_size, current_message_timestamp(),
                  destination == MESH_BROADCAST_NODE ? 0 : destination, true, channel)) return false;
    messages[message_count - 1].packet_id = id;
    messages[message_count - 1].delivery = MESH_DELIVERY_PENDING;
    messages[message_count - 1].reply_id = reply_id;
    message_sent_us[message_count - 1] = esp_timer_get_time();
    message_hop_limit[message_count - 1] = hops;
    persist_message_history();
    return true;
}

static bool send_text_to(uint32_t destination, uint8_t channel, const char *text) {
    return send_text_with_reply(destination, channel, text, 0);
}
bool mesh_radio_send_reply(uint32_t peer, uint8_t channel, const char *text, uint32_t reply_id) {
    if (!reply_id || peer == MESH_BROADCAST_NODE || peer == local_node_number || channel >= MESH_CHANNEL_LIMIT) return false;
    return send_text_with_reply(peer ? peer : MESH_BROADCAST_NODE, peer ? 0 : channel, text, reply_id);
}
bool mesh_radio_get_reply_parent(const mesh_message_t *reply, mesh_message_t *out) {
    if (!reply || !out || !reply->reply_id) return false;
    const mesh_message_t *found = NULL;
    for (size_t i = 0; i < message_count; i++) {
        const mesh_message_t *m = &messages[i];
        if (m->packet_id == reply->reply_id && m->peer_node == reply->peer_node && (m->peer_node || m->channel == reply->channel)) {
            if (found) return false; // Ambiguous packet IDs: never attribute a quote to the wrong sender.
            found = m;
        }
    }
    if (!found) return false;
    *out = *found; return true;
}
#include "delivery_text.inc"

bool mesh_radio_retry_message(const mesh_message_t *target) {
    if (!target || !target->outgoing || target->delivery != MESH_DELIVERY_FAILED || !target->packet_id) return false;
    for (size_t i = 0; i < message_count; i++) {
        const mesh_message_t *message = &messages[i];
        if (message->outgoing && message->delivery == MESH_DELIVERY_FAILED && message->packet_id == target->packet_id &&
            message->peer_node == target->peer_node && message->channel == target->channel && !strcmp(message->text, target->text) && message->reply_id == target->reply_id) {
            // Snapshot before eviction; a retry may replace the oldest record.
            mesh_message_t copy = *message;
            return send_text_with_reply(copy.peer_node ? copy.peer_node : MESH_BROADCAST_NODE, copy.peer_node ? 0 : copy.channel, copy.text, copy.reply_id);
        }
    }
    return false;
}

bool mesh_radio_send_reaction(const mesh_message_t *target, const char *emoji) {
    if (!target || !target->packet_id || !local_node_number || !emoji || !emoji[0]) return false;
    size_t size = strlen(emoji);
    if (size >= sizeof(reactions[0].emoji)) return false;
    bool retained = false;
    for (size_t i = 0; i < message_count; i++) {
        const mesh_message_t *message = &messages[i];
        if (message->packet_id == target->packet_id && message->peer_node == target->peer_node &&
            message->channel == target->channel && message->outgoing == target->outgoing &&
            !strcmp(message->from, target->from)) { retained = true; break; }
    }
    if (!retained) return false;
    uint32_t destination = target->peer_node ? target->peer_node : MESH_BROADCAST_NODE;
    uint8_t channel = target->peer_node ? 0 : target->channel;
    if (target->peer_node && (destination == MESH_BROADCAST_NODE || destination == local_node_number)) return false;
    if (!target->peer_node && (channel >= MESH_CHANNEL_LIMIT || !channels[channel].info.loaded || !channels[channel].info.role)) return false;
    uint8_t data[96] = {0x08, 0x01}; // TEXT_MESSAGE_APP
    size_t pos = 2;
    if (!append_bytes_field(data, sizeof(data), &pos, 2, (const uint8_t *)emoji, size)) return false;
    data[pos++] = 0x3d; memcpy(data + pos, &target->packet_id, 4); pos += 4; // Data.reply_id
    uint32_t flag = 1;
    data[pos++] = 0x45; memcpy(data + pos, &flag, 4); pos += 4; // Data.emoji
    uint32_t id; uint8_t hops;
    if (!transmit_chat_data(destination, channel, data, pos, &id, &hops)) return false;
    store_reaction(local_node_number, id, target->packet_id, target->peer_node, channel,
                   current_message_timestamp(), (const uint8_t *)emoji, size);
    return true;
}

bool mesh_radio_request_owner(void) {
    if (!status.started || !local_node_number) return false;
    int64_t now_us = esp_timer_get_time();
    if (last_owner_request_us && now_us - last_owner_request_us < 3000000) return true;
    static const uint8_t request_owner[] = {0x18, 0x01}; // AdminMessage.get_owner_request = true.
    if (!send_admin_payload(request_owner, sizeof(request_owner))) return false;
    last_owner_request_us = now_us;
    return true;
}

bool mesh_radio_admin_ready(void) {
    return !mesh_radio_setup_busy() && !mesh_radio_neighbor_config_busy() && !management_local_busy() && role_save.state!=MESH_ROLE_SAVING && cfg_state!=CFG_SAVING && keys_state!=MESH_KEYS_SAVING && status.started && !status.reconnecting && local_node_number != 0 && session_passkey_size != 0;
}

mesh_radio_metadata_t mesh_radio_metadata(void) { return radio_metadata; }

bool mesh_radio_request_metadata(void) {
    if (!status.started || status.reconnecting || !local_node_number) return false;
    int64_t now = esp_timer_get_time();
    if (last_metadata_request_us && now - last_metadata_request_us < 3000000) return true;
    const uint8_t request[] = {0x60, 0x01}; // AdminMessage.get_device_metadata_request (12).
    if (!send_admin_payload(request, sizeof(request))) return false;
    last_metadata_request_us = now;
    radio_metadata.pending = true;
    history_revision++;
    return true;
}

unsigned mesh_radio_rx_age_seconds(void) {
    int64_t elapsed = esp_timer_get_time() - last_radio_activity_us;
    return elapsed > 0 ? (unsigned)(elapsed / 1000000) : 0;
}

bool mesh_radio_get_device(mesh_device_config_t *out) {
    if (!out) return false;
    *out = device_config; return out->loaded;
}
mesh_role_save_t mesh_radio_role_save(void) { return role_save; }

bool mesh_radio_request_device(void) {
    if (!status.started || status.reconnecting || !local_node_number) return false;
    int64_t now = esp_timer_get_time();
    if (last_device_request_us && now - last_device_request_us < 3000000) return true;
    const uint8_t request[] = {0x28, 0}; // get_config_request = DEVICE_CONFIG, explicitly present zero.
    uint32_t id;
    if (!send_admin_payload_with_id(request, sizeof(request), &id)) return false;
    last_device_request_us = now;
    if (role_save.state == MESH_ROLE_SAVING && now >= role_read_at_us) role_read_id = id;
    return true;
}

bool mesh_radio_role_supported(uint32_t role) {
    if (role == 3 || role == 4 || role > 12) return false; // Deprecated roles are read-only.
    if (role != 12 || (device_config.loaded && device_config.role == 12)) return true;
    unsigned major = 0, minor = 0;
    return radio_metadata.loaded && sscanf(radio_metadata.firmware_version, "%u.%u", &major, &minor) == 2 &&
        (major > 2 || (major == 2 && minor >= 7));
}

static bool set_device_fields(uint32_t role, uint32_t expected_revision, bool behavior, uint32_t mode, uint32_t interval) {
    if (favorite_status.state == MESH_FAVORITE_SAVING || lora_verify_at_us || owner_verify_at_us || channel_verify_at_us) return false;
    if (telemetry_save.state == MESH_TELEMETRY_CONFIG_SAVING || location_save.state == MESH_LOCATION_SAVING || !status.synchronized || !device_config.loaded || device_config.revision != expected_revision ||
        (behavior ? (!mesh_radio_behavior_supported(role, mode) || interval < 3600) : !mesh_radio_role_supported(role)) || !mesh_radio_admin_ready() || role_save.state == MESH_ROLE_SAVING || (node_control.state == MESH_FAVORITE_SAVING || channel_operation.state == MESH_FAVORITE_SAVING)) return false;
    if (esp_timer_get_time() - session_passkey_received_us >= 240000000) {
        last_owner_request_us = 0; mesh_radio_request_owner(); return false;
    }
    uint8_t device[192]; size_t n = 0, offset = 0;
    while (offset < device_raw_size) {
        size_t start = offset; protobuf_field_t field;
        if (!read_field(device_raw, device_raw_size, &offset, &field)) return false;
        if ((!behavior && field.field == 1) || (behavior && (field.field == 6 || field.field == 7))) continue; // Retain every other field byte-for-byte.
        if (offset - start > sizeof(device) - n) return false;
        memcpy(device + n, device_raw + start, offset - start); n += offset - start;
    }
    if (behavior) {
        if (!append_varint(device,sizeof(device),&n,48) || !append_varint(device,sizeof(device),&n,mode) ||
            !append_varint(device,sizeof(device),&n,56) || !append_varint(device,sizeof(device),&n,interval)) return false;
    } else if (!append_varint(device, sizeof(device), &n, 8) || !append_varint(device, sizeof(device), &n, role)) return false;
    uint8_t config[208], admin[240]; size_t c = 0, a = 0;
    if (!append_bytes_field(config, sizeof(config), &c, 1, device, n) ||
        !append_bytes_field(admin, sizeof(admin), &a, 34, config, c) ||
        !append_bytes_field(admin, sizeof(admin), &a, 101, session_passkey, session_passkey_size) ||
        !send_admin_payload_with_id(admin, a, &role_write_id)) return false;
    role_save = (mesh_role_save_t){.state = MESH_ROLE_SAVING, .desired = role};
    role_origin = local_node_number; role_read_id = 0;
    role_behavior_write = behavior; behavior_mode = mode; behavior_interval = interval;
    role_started_us = esp_timer_get_time(); role_read_at_us = role_started_us + 1500000;
    history_revision++; return true;
}

bool mesh_radio_set_role(uint32_t role, uint32_t expected_revision) {
    return set_device_fields(role,expected_revision,false,0,0);
}
bool mesh_radio_behavior_supported(uint32_t role, uint32_t mode) {
    if (mode > 5) return false;
    if (mode == 1) return role == 4; // Repeater only; other roles silently behave as ALL.
    if (mode == 4) return role == 5 || role == 6 || role == 10; // Protocol-permitted non-forwarding roles.
    return true;
}
bool mesh_radio_set_behavior(uint32_t mode, uint32_t interval, uint32_t expected_revision) {
    return set_device_fields(device_config.role,expected_revision,true,mode,interval);
}
bool mesh_radio_get_node_by_number(uint32_t number, mesh_node_t *out_node) {
    if (!number || !out_node) return false;
    for (size_t i = 0; i < node_count; i++) if (nodes[i].number == number) {
        *out_node = nodes[i];
        node_apply_alias(out_node);
        return true;
    }
    return false;
}

mesh_favorite_status_t mesh_radio_favorite_status(void) { return favorite_status; }

mesh_favorite_start_t mesh_radio_set_favorite(uint32_t number, bool favorite) {
    mesh_node_t node;
    if (!number || number == MESH_BROADCAST_NODE || number == local_node_number ||
        !mesh_radio_get_node_by_number(number, &node) || !node.favorite_known) return MESH_FAVORITE_START_INVALID;
    if (telemetry_save.state == MESH_TELEMETRY_CONFIG_SAVING || location_save.state == MESH_LOCATION_SAVING || favorite_status.state == MESH_FAVORITE_SAVING || (node_control.state == MESH_FAVORITE_SAVING || channel_operation.state == MESH_FAVORITE_SAVING) ||
        role_save.state == MESH_ROLE_SAVING || lora_verify_at_us || owner_verify_at_us || channel_verify_at_us) return MESH_FAVORITE_START_BUSY;
    if (!status.started || status.reconnecting || !status.synchronized || config_request_pending)
        return MESH_FAVORITE_START_OFFLINE;
    int64_t now = esp_timer_get_time();
    // Admin session keys expire after 300 seconds. Refresh early; the user can
    // retry after the local owner reply. Never queue a deferred mutation.
    if (!mesh_radio_admin_ready() || now - session_passkey_received_us >= 240000000) {
        last_owner_request_us = 0;
        mesh_radio_request_owner();
        return MESH_FAVORITE_START_NOT_READY;
    }
    uint8_t admin[40]; size_t size = 0;
    if (!append_varint(admin, sizeof(admin), &size, (favorite ? 39U : 40U) << 3) ||
        !append_varint(admin, sizeof(admin), &size, number) ||
        !append_bytes_field(admin, sizeof(admin), &size, 101, session_passkey, session_passkey_size) ||
        !send_admin_payload_with_id(admin, size, &favorite_request_id)) return MESH_FAVORITE_START_WRITE_FAILED;
    node_control = (mesh_node_control_status_t){0};
    favorite_status = (mesh_favorite_status_t){.node = number, .desired = favorite, .state = MESH_FAVORITE_SAVING};
    favorite_started_us = now;
    favorite_verify_at_us = now + 1500000;
    favorite_verify_nonce = 0;
    favorite_seen = false;
    history_revision++;
    return MESH_FAVORITE_START_OK;
}

mesh_node_control_status_t mesh_radio_node_control_status(void) { return node_control; }
mesh_favorite_start_t mesh_radio_control_node(uint32_t number, mesh_node_control_kind_t kind) {
    mesh_node_t node;
    if (!number || number == local_node_number || number == MESH_BROADCAST_NODE || kind > MESH_NODE_REMOVE ||
        !mesh_radio_get_node_by_number(number, &node) || !node.favorite_known) return MESH_FAVORITE_START_INVALID;
    if (favorite_status.state == MESH_FAVORITE_SAVING || (node_control.state == MESH_FAVORITE_SAVING || channel_operation.state == MESH_FAVORITE_SAVING) ||
        telemetry_save.state == MESH_TELEMETRY_CONFIG_SAVING || location_save.state == MESH_LOCATION_SAVING ||
        role_save.state == MESH_ROLE_SAVING || lora_verify_at_us || owner_verify_at_us || channel_verify_at_us) return MESH_FAVORITE_START_BUSY;
    if (!status.synchronized || status.reconnecting || config_request_pending) return MESH_FAVORITE_START_OFFLINE;
    int64_t now = esp_timer_get_time();
    if (!mesh_radio_admin_ready() || now - session_passkey_received_us >= 240000000) {
        last_owner_request_us = 0; mesh_radio_request_owner(); return MESH_FAVORITE_START_NOT_READY;
    }
    uint8_t admin[40]; size_t size = 0;
    uint32_t field = kind == MESH_NODE_REMOVE ? 38 : kind == MESH_NODE_IGNORE ? 47 : 48;
    if (!append_varint(admin, sizeof(admin), &size, field << 3) || !append_varint(admin, sizeof(admin), &size, number) ||
        !append_bytes_field(admin, sizeof(admin), &size, 101, session_passkey, session_passkey_size) ||
        !send_admin_payload_with_id(admin, size, &node_control_id)) return MESH_FAVORITE_START_WRITE_FAILED;
    favorite_status = (mesh_favorite_status_t){0};
    node_control = (mesh_node_control_status_t){.node = number, .kind = kind, .state = MESH_FAVORITE_SAVING};
    node_control_started_us = now; node_control_verify_us = now + 1500000; node_control_nonce = 0;
    node_control_seen = false; history_revision++; return MESH_FAVORITE_START_OK;
}

bool mesh_radio_set_owner(const char *long_name, const char *short_name) {
    if(mesh_radio_setup_busy()||mesh_radio_neighbor_config_busy())return false;
    if(cfg_state==CFG_SAVING||keys_state==MESH_KEYS_SAVING)return false;
    if (telemetry_save.state == MESH_TELEMETRY_CONFIG_SAVING || location_save.state == MESH_LOCATION_SAVING || (node_control.state == MESH_FAVORITE_SAVING || channel_operation.state == MESH_FAVORITE_SAVING)) return false;
    if (!long_name || !short_name || !long_name[0] || !short_name[0] ||
        strlen(long_name) > 39 || strlen(short_name) > 4 || session_passkey_size == 0) return false;

    uint8_t user[80];
    size_t user_size = 0;
    if (!append_bytes_field(user, sizeof(user), &user_size, 2,
                            (const uint8_t *)long_name, strlen(long_name)) ||
        !append_bytes_field(user, sizeof(user), &user_size, 3,
                            (const uint8_t *)short_name, strlen(short_name))) return false;

    uint8_t owner_update[128];
    size_t update_size = 0;
    if (!append_bytes_field(owner_update, sizeof(owner_update), &update_size, 32, user, user_size) ||
        !append_bytes_field(owner_update, sizeof(owner_update), &update_size, 101,
                            session_passkey, session_passkey_size)) return false;

    if (!send_admin_payload(owner_update, update_size)) return false;
    owner_verify_at_us = esp_timer_get_time() + 1500000;
    return true;
}

bool mesh_radio_get_owner(char *long_name, size_t long_name_size,
                          char *short_name, size_t short_name_size) {
    if (long_name && long_name_size) snprintf(long_name, long_name_size, "%s", local_long_name);
    if (short_name && short_name_size) snprintf(short_name, short_name_size, "%s", local_short_name);
    return owner_identity_received;
}

bool mesh_radio_send_text(const char *text) { return send_text_to(MESH_BROADCAST_NODE, 0, text); }

bool mesh_radio_send_direct(uint32_t destination, const char *text) {
    if (!local_node_number || !destination || destination == MESH_BROADCAST_NODE || destination == local_node_number) return false;
    return send_text_to(destination, 0, text);
}

uint32_t mesh_radio_local_node(void) { return local_node_number; }

size_t mesh_radio_conversation_count(uint32_t peer_node) {
    size_t count = 0;
    for (size_t i = 0; i < message_count; i++) if (messages[i].peer_node == peer_node && (peer_node || messages[i].channel == 0)) count++;
    return count;
}

bool mesh_radio_get_conversation_message(uint32_t peer_node, size_t index, mesh_message_t *out) {
    if (!out) return false;
    for (size_t i = 0; i < message_count; i++) {
        if (messages[i].peer_node == peer_node && (peer_node || messages[i].channel == 0) && index-- == 0) { *out = messages[i]; return true; }
    }
    return false;
}

// Most recently active conversation first, including peers absent from NodeDB.
bool mesh_radio_get_direct_peer(size_t index, uint32_t *peer_node) {
    uint32_t seen[MESSAGE_HISTORY_LIMIT];
    size_t count = 0;
    for (size_t i = message_count; i > 0; i--) {
        uint32_t peer = messages[i - 1].peer_node;
        if (!peer) continue;
        bool duplicate = false;
        for (size_t j = 0; j < count; j++) if (seen[j] == peer) duplicate = true;
        if (duplicate) continue;
        if (count == index) { if (peer_node) *peer_node = peer; return true; }
        seen[count++] = peer;
    }
    return false;
}

size_t mesh_radio_direct_peer_count(void) {
    size_t count = 0;
    while (mesh_radio_get_direct_peer(count, NULL)) count++;
    return count;
}

bool mesh_radio_get_channel(uint8_t index, mesh_channel_t *out) {
    if (index >= MESH_CHANNEL_LIMIT || !out || !channels[index].info.loaded) return false;
    *out = channels[index].info;
    return true;
}

bool mesh_radio_request_channel(uint8_t index) {
    if (index >= MESH_CHANNEL_LIMIT || !status.started || !local_node_number) return false;
    int64_t now = esp_timer_get_time();
    if (last_channel_request_us[index] && now - last_channel_request_us[index] < 3000000) return true;
    uint8_t request[] = {0x08, (uint8_t)(index + 1)}; // Admin channel requests are one-based.
    if (!send_admin_payload(request, sizeof(request))) return false;
    last_channel_request_us[index] = now;
    return true;
}

bool mesh_radio_set_channel_location(uint8_t index, const char *name, bool uplink, bool downlink,
                                     int precision, uint32_t expected_revision) {
    if (index >= MESH_CHANNEL_LIMIT || !name || strlen(name) > 11 ||
        !channels[index].info.loaded || !channels[index].info.role || !mesh_radio_admin_ready()) return false;
    if (channels[index].info.revision != expected_revision || precision < -1 || precision > 32 ||
        telemetry_save.state == MESH_TELEMETRY_CONFIG_SAVING || location_save.state == MESH_LOCATION_SAVING || role_save.state == MESH_ROLE_SAVING ||
        favorite_status.state == MESH_FAVORITE_SAVING || (node_control.state == MESH_FAVORITE_SAVING || channel_operation.state == MESH_FAVORITE_SAVING) || !status.synchronized || status.reconnecting) return false;
    // Public/default-key channels use a conservative maximum even on older firmware.
    if (precision > 15 && (!channels[index].info.has_key || channels[index].info.default_key)) return false;
    if (esp_timer_get_time() - session_passkey_received_us >= 240000000) {
        last_owner_request_us = 0; mesh_radio_request_owner(); return false;
    }
    // Preserve all unedited fields verbatim, including keys and unknown future settings.
    uint8_t channel[192]; size_t c = 0, offset = 0;
    bool has_settings = false;
    while (offset < channels[index].size) {
        size_t start = offset;
        protobuf_field_t field;
        if (!read_field(channels[index].raw, channels[index].size, &offset, &field)) return false;
        if (field.field == 2 && field.wire == 2) {
            uint8_t settings[160]; size_t n = 0, inner = 0;
            while (inner < field.length) {
                size_t begin = inner;
                protobuf_field_t setting;
                if (!read_field(field.value, field.length, &inner, &setting)) return false;
                if (setting.field == 3 || setting.field == 5 || setting.field == 6 ||
                    (precision >= 0 && setting.field == 7)) continue;
                if (inner - begin > sizeof(settings) - n) return false;
                memcpy(settings + n, field.value + begin, inner - begin); n += inner - begin;
            }
            if (precision >= 0) {
                uint8_t module[160]; size_t mn = 0, scan = 0;
                // Retain mute and all unknown nested module fields.
                while (scan < field.length) {
                    protobuf_field_t sf;
                    if (!read_field(field.value, field.length, &scan, &sf)) return false;
                    if (sf.field != 7 || sf.wire != 2) continue;
                    size_t mo = 0;
                    while (mo < sf.length) {
                        size_t begin = mo; protobuf_field_t mf;
                        if (!read_field(sf.value, sf.length, &mo, &mf)) return false;
                        if (mf.field == 1) continue;
                        if (mo - begin > sizeof(module) - mn) return false;
                        memcpy(module + mn, sf.value + begin, mo - begin); mn += mo - begin;
                    }
                }
                if (!append_varint(module, sizeof(module), &mn, 8) ||
                    !append_varint(module, sizeof(module), &mn, (unsigned)precision) ||
                    !append_bytes_field(settings, sizeof(settings), &n, 7, module, mn)) return false;
            }
            if (!append_bytes_field(settings, sizeof(settings), &n, 3, (const uint8_t *)name, strlen(name)) ||
                !append_varint(settings, sizeof(settings), &n, 40) || !append_varint(settings, sizeof(settings), &n, uplink) ||
                !append_varint(settings, sizeof(settings), &n, 48) || !append_varint(settings, sizeof(settings), &n, downlink) ||
                !append_bytes_field(channel, sizeof(channel), &c, 2, settings, n)) return false;
            has_settings = true;
        } else {
            if (offset - start > sizeof(channel) - c) return false;
            memcpy(channel + c, channels[index].raw + start, offset - start); c += offset - start;
        }
    }
    if (!has_settings) return false;
    uint8_t admin[240]; size_t n = 0;
    if (!append_bytes_field(admin, sizeof(admin), &n, 33, channel, c) ||
        !append_bytes_field(admin, sizeof(admin), &n, 101, session_passkey, session_passkey_size) ||
        !send_admin_payload_with_id(admin, n, &location_op.write_id)) return false;
    location_save = (mesh_location_save_t){.state = MESH_LOCATION_SAVING};
    location_op.kind = 3; location_op.origin = local_node_number; location_op.channel = index;
    location_op.precision = precision; location_op.uplink = uplink; location_op.downlink = downlink;
    snprintf(location_op.name, sizeof(location_op.name), "%s", name);
    location_op.started = esp_timer_get_time(); location_op.read_at = location_op.started + 1500000;
    location_op.nonce = 0;
    channel_verify_index = index;
    channel_verify_at_us = esp_timer_get_time() + 1500000;
    return true;
}

bool mesh_radio_set_channel(uint8_t index, const char *name, bool uplink, bool downlink) {
    if (index >= MESH_CHANNEL_LIMIT) return false;
    return mesh_radio_set_channel_location(index, name, uplink, downlink, -1, channels[index].info.revision);
}

#ifndef CHANNEL_IMPORT_PATH
#define CHANNEL_IMPORT_PATH "/locfd/meshdeck-channel-import.bin"
#endif

static bool read_shared_channel(uint8_t *settings, size_t *size, mesh_channel_t *info) {
    FILE *file = fopen(CHANNEL_IMPORT_PATH, "rb");
    if (!file) return false;
    *size = fread(settings, 1, 160, file);
    bool ok = !ferror(file) && fgetc(file) == EOF && *size != 0;
    fclose(file);
    if (!ok) return false;
    memset(info, 0, sizeof(*info));
    size_t offset = 0;
    bool name_seen = false, key_seen = false;
    while (offset < *size) {
        protobuf_field_t f;
        if (!read_field(settings, *size, &offset, &f)) return false;
        if (f.field == 3 && f.wire == 2) {
            if (name_seen || f.length >= sizeof(info->name)) return false;
            memcpy(info->name, f.value, f.length); info->name[f.length] = '\0';
            name_seen = true;
        } else if (f.field == 2 && f.wire == 2) {
            if (key_seen || (f.length != 1 && f.length != 16 && f.length != 32)) return false;
            info->has_key = f.length > 1 || f.value[0] != 0;
            info->default_key = f.length == 1 && f.value[0] != 0;
            key_seen = true;
        } else if (f.field == 5 && f.wire == 0) info->uplink_enabled = f.integer != 0;
        else if (f.field == 6 && f.wire == 0) info->downlink_enabled = f.integer != 0;
    }
    info->loaded = true;
    info->role = 2;
    return key_seen;
}

bool mesh_radio_get_shared_channel(mesh_channel_t *out) {
    if (!out) return false;
    uint8_t settings[160]; size_t size;
    return read_shared_channel(settings, &size, out);
}

bool mesh_radio_add_shared_channel(uint8_t index) {
    if (telemetry_save.state == MESH_TELEMETRY_CONFIG_SAVING || location_save.state == MESH_LOCATION_SAVING || (node_control.state == MESH_FAVORITE_SAVING || channel_operation.state == MESH_FAVORITE_SAVING)) return false;
    // Never overwrite an enabled channel or the primary slot.
    if (index == 0 || index >= MESH_CHANNEL_LIMIT || !channels[index].info.loaded ||
        channels[index].info.role != 0 || !mesh_radio_admin_ready()) return false;
    uint8_t settings[160]; size_t size;
    mesh_channel_t info;
    if (!read_shared_channel(settings, &size, &info)) return false;
    uint8_t channel[192]; size_t c = 0;
    if (!append_varint(channel, sizeof(channel), &c, 8) || !append_varint(channel, sizeof(channel), &c, index) ||
        !append_bytes_field(channel, sizeof(channel), &c, 2, settings, size) ||
        !append_varint(channel, sizeof(channel), &c, 24) || !append_varint(channel, sizeof(channel), &c, 2)) return false;
    uint8_t admin[240]; size_t n = 0;
    if (!append_bytes_field(admin, sizeof(admin), &n, 33, channel, c) ||
        !append_bytes_field(admin, sizeof(admin), &n, 101, session_passkey, session_passkey_size) ||
        !send_admin_payload(admin, n)) return false;
    channel_verify_index = index;
    channel_verify_at_us = esp_timer_get_time() + 1500000;
    return true;
}

bool mesh_radio_shared_channel_matches(uint8_t index) {
    if (index >= MESH_CHANNEL_LIMIT || !channels[index].info.loaded) return false;
    uint8_t shared[160]; size_t size;
    mesh_channel_t info;
    if (!read_shared_channel(shared, &size, &info) || channels[index].info.role != 2 ||
        strcmp(channels[index].info.name, info.name) != 0 ||
        channels[index].info.uplink_enabled != info.uplink_enabled ||
        channels[index].info.downlink_enabled != info.downlink_enabled) return false;
    const uint8_t *expected_key = NULL; size_t expected_size = 0, offset = 0;
    while (offset < size) {
        protobuf_field_t f;
        if (!read_field(shared, size, &offset, &f)) return false;
        if (f.field == 2 && f.wire == 2) { expected_key = f.value; expected_size = f.length; }
    }
    offset = 0;
    while (offset < channels[index].size) {
        protobuf_field_t f;
        if (!read_field(channels[index].raw, channels[index].size, &offset, &f)) return false;
        if (f.field != 2 || f.wire != 2) continue;
        size_t inner = 0;
        while (inner < f.length) {
            protobuf_field_t key;
            if (!read_field(f.value, f.length, &inner, &key)) return false;
            if (key.field == 2 && key.wire == 2) return expected_key && key.length == expected_size && memcmp(key.value, expected_key, key.length) == 0;
        }
    }
    return false;
}

bool mesh_radio_send_channel(uint8_t channel, const char *text) {
    if (channel >= MESH_CHANNEL_LIMIT || !channels[channel].info.loaded || !channels[channel].info.role) return false;
    return send_text_to(MESH_BROADCAST_NODE, channel, text);
}

size_t mesh_radio_channel_message_count(uint8_t channel) {
    size_t count = 0;
    for (size_t i = 0; i < message_count; i++) if (messages[i].peer_node == 0 && messages[i].channel == channel) count++;
    return count;
}

bool mesh_radio_get_channel_message(uint8_t channel, size_t index, mesh_message_t *out) {
    if (!out || channel >= MESH_CHANNEL_LIMIT) return false;
    for (size_t i = 0; i < message_count; i++) {
        if (messages[i].peer_node == 0 && messages[i].channel == channel && index-- == 0) { *out = messages[i]; return true; }
    }
    return false;
}

bool mesh_radio_get_lora(mesh_lora_config_t *out) {
    if (!out || !lora_config.loaded) return false;
    *out = lora_config;
    return true;
}

bool mesh_radio_request_lora(void) {
    if (!status.started || !local_node_number) return false;
    int64_t now = esp_timer_get_time();
    if (last_lora_request_us && now - last_lora_request_us < 3000000) return true;
    const uint8_t request[] = {0x28, 5}; // get_config_request = LORA_CONFIG.
    if (!send_admin_payload(request, sizeof(request))) return false;
    last_lora_request_us = now;
    return true;
}

static bool region_matches_hardware(uint32_t region) {
    switch (region) {
        case 0: case 1: case 3: case 5: case 6: case 7: case 8: case 9: case 10:
        case 11: case 12: case 15: case 17: case 18: case 20: case 21: case 24: case 25: case 26:
            return true;
        default: return false;
    }
}

bool mesh_radio_set_lora(uint32_t region, uint32_t preset, uint8_t hops, bool use_preset) {
    if (telemetry_save.state == MESH_TELEMETRY_CONFIG_SAVING || location_save.state == MESH_LOCATION_SAVING || (node_control.state == MESH_FAVORITE_SAVING || channel_operation.state == MESH_FAVORITE_SAVING) || !lora_config.loaded || !mesh_radio_admin_ready() || hops > 7 ||
        (region != lora_config.region && !region_matches_hardware(region)) ||
        (preset != lora_config.modem_preset && (preset > 9 || preset == 1 || preset == 2))) return false;
    uint8_t lora[192]; size_t n = 0, offset = 0;
    while (offset < lora_raw_size) {
        size_t start = offset;
        protobuf_field_t field;
        if (!read_field(lora_raw, lora_raw_size, &offset, &field)) return false;
        if (field.field == 1 || field.field == 2 || field.field == 7 || field.field == 8) continue;
        if (offset - start > sizeof(lora) - n) return false;
        memcpy(lora + n, lora_raw + start, offset - start); n += offset - start;
    }
    if (!append_varint(lora, sizeof(lora), &n, 8) || !append_varint(lora, sizeof(lora), &n, use_preset) ||
        !append_varint(lora, sizeof(lora), &n, 16) || !append_varint(lora, sizeof(lora), &n, preset) ||
        !append_varint(lora, sizeof(lora), &n, 56) || !append_varint(lora, sizeof(lora), &n, region) ||
        !append_varint(lora, sizeof(lora), &n, 64) || !append_varint(lora, sizeof(lora), &n, hops)) return false;
    uint8_t config[208]; size_t c = 0;
    if (!append_bytes_field(config, sizeof(config), &c, 6, lora, n)) return false;
    uint8_t admin[240]; size_t a = 0;
    if (!append_bytes_field(admin, sizeof(admin), &a, 34, config, c) ||
        !append_bytes_field(admin, sizeof(admin), &a, 101, session_passkey, session_passkey_size) ||
        !send_admin_payload(admin, a)) return false;
    lora_verify_at_us = esp_timer_get_time() + 1500000;
    return true;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int base64_digit(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

static bool decode_channel_key(const char *text, uint8_t key[32], size_t *key_size) {
    if (!text || !key || !key_size) return false;
    size_t length = strlen(text);
    if (!length || length > 66) return false;
    bool prefixed = length >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    const char *hex = prefixed ? text + 2 : text;
    size_t hex_length = prefixed ? length - 2 : length;
    bool is_hex = hex_length == 32 || hex_length == 64;
    for (size_t i = 0; is_hex && i < hex_length; i++) if (hex_digit(hex[i]) < 0) is_hex = false;
    if (is_hex) {
        for (size_t i = 0; i < hex_length / 2; i++) key[i] = (uint8_t)((hex_digit(hex[2*i]) << 4) | hex_digit(hex[2*i+1]));
        *key_size = hex_length / 2;
        return true;
    }
    if (prefixed) return false;
    size_t chars = length, padding = 0;
    while (chars && text[chars - 1] == '=') { chars--; padding++; }
    if (padding > 2 || chars % 4 == 1 || (padding && (length % 4 || padding != (4 - chars % 4) % 4))) return false;
    uint32_t accumulator = 0;
    unsigned bits = 0;
    size_t size = 0;
    for (size_t i = 0; i < chars; i++) {
        int digit = base64_digit(text[i]);
        if (digit < 0) return false;
        accumulator = (accumulator << 6) | (uint32_t)digit;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (size >= 32) return false;
            key[size++] = (uint8_t)(accumulator >> bits);
        }
    }
    if (bits && (accumulator & ((1u << bits) - 1u))) return false;
    if (size != 1 && size != 16 && size != 32) return false;
    *key_size = size;
    return true;
}

bool mesh_radio_channel_key_valid(const char *text) {
    uint8_t key[32] = {0}; size_t size;
    bool valid = decode_channel_key(text, key, &size);
    memset(key, 0, sizeof(key));
    return valid;
}

bool mesh_radio_add_manual_channel(uint8_t index, const char *name, const char *text) {
    if (telemetry_save.state == MESH_TELEMETRY_CONFIG_SAVING || location_save.state == MESH_LOCATION_SAVING || (node_control.state == MESH_FAVORITE_SAVING || channel_operation.state == MESH_FAVORITE_SAVING)) return false;
    if (index == 0 || index >= MESH_CHANNEL_LIMIT || !name || !name[0] || strlen(name) > 11 ||
        !channels[index].info.loaded || channels[index].info.role != 0 || !mesh_radio_admin_ready()) return false;
    uint8_t key[32] = {0}; size_t key_size;
    if (!decode_channel_key(text, key, &key_size)) { memset(key, 0, sizeof(key)); return false; }
    uint8_t settings[64]; size_t s = 0;
    bool encoded = append_bytes_field(settings, sizeof(settings), &s, 2, key, key_size) &&
                   append_bytes_field(settings, sizeof(settings), &s, 3, (const uint8_t *)name, strlen(name));
    memset(key, 0, sizeof(key));
    if (!encoded) return false;
    uint8_t channel[80]; size_t c = 0;
    if (!append_varint(channel, sizeof(channel), &c, 8) || !append_varint(channel, sizeof(channel), &c, index) ||
        !append_bytes_field(channel, sizeof(channel), &c, 2, settings, s) ||
        !append_varint(channel, sizeof(channel), &c, 24) || !append_varint(channel, sizeof(channel), &c, 2)) return false;
    uint8_t admin[112]; size_t a = 0;
    if (!append_bytes_field(admin, sizeof(admin), &a, 33, channel, c) ||
        !append_bytes_field(admin, sizeof(admin), &a, 101, session_passkey, session_passkey_size) ||
        !send_admin_payload(admin, a)) return false;
    channel_verify_index = index;
    channel_verify_at_us = esp_timer_get_time() + 1500000;
    return true;
}

bool mesh_radio_manual_channel_matches(uint8_t index, const char *name, const char *text) {
    if (index == 0 || index >= MESH_CHANNEL_LIMIT || !name || !channels[index].info.loaded ||
        channels[index].info.role != 2 || strcmp(channels[index].info.name, name)) return false;
    uint8_t expected[32] = {0}; size_t expected_size;
    if (!decode_channel_key(text, expected, &expected_size)) { memset(expected, 0, sizeof(expected)); return false; }
    bool match = false;
    size_t offset = 0;
    while (offset < channels[index].size) {
        protobuf_field_t field;
        if (!read_field(channels[index].raw, channels[index].size, &offset, &field)) break;
        if (field.field != 2 || field.wire != 2) continue;
        size_t inner = 0;
        while (inner < field.length) {
            protobuf_field_t key;
            if (!read_field(field.value, field.length, &inner, &key)) break;
            if (key.field == 2 && key.wire == 2) { match = key.length == expected_size && memcmp(key.value, expected, key.length) == 0; break; }
        }
    }
    memset(expected, 0, sizeof(expected));
    return match;
}

#include "waypoints.inc"

#include "channel_management.inc"

#include "radio_management.inc"
#include "radio_range.inc"
#include "radio_neighbors.inc"
#include "radio_setup.inc"

#include "radio_keys.inc"
#include "radio_config_backup.inc"

#include "radio_location.inc"

#include "map_trails.inc"

#include "telemetry_data.inc"

#include "radio_telemetry.inc"

#include "radio_telemetry_settings.inc"

uint16_t mesh_radio_take_message_alerts(void) {
    uint16_t events = message_alerts;
    message_alerts = 0;
    return events;
}
