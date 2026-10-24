#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "telemetry_data.h"
#include "security_contacts.h"
#include "range_test.h"
#include "neighbor_info.h"
#include "first_use.h"

#define MESH_BROADCAST_NODE UINT32_MAX
#define MESH_TEXT_MAX_LENGTH 200 // UTF-8 bytes, excluding the terminating NUL.
#define MESH_NODE_LIMIT 128
#define MESH_MESSAGE_HISTORY_MAX 1000

typedef struct {
    bool started;
    bool synchronized;
    bool reconnecting;
    uint16_t node_count;
    uint32_t frames_received;
    uint32_t reconnect_attempts;
} mesh_radio_status_t;

typedef struct {
    bool available, cached;
    bool has_battery, has_voltage, has_channel_use, has_airtime, has_uptime;
    uint32_t battery, uptime, sample_time, packet_id;
    float voltage, channel_use, airtime;
    int64_t received_us;
} mesh_device_metrics_t;

typedef struct {
    uint32_t number;
    char name[40];
    char advertised_name[40]; // Getter preserves the radio name when a local nickname is shown.
    char short_name[5];
    bool names_received;
    float snr;
    uint32_t last_heard;
    uint32_t hops_away;
    bool hops_away_available;
    bool is_favorite; // Radio NodeInfo.is_favorite, not an app-only bookmark.
    bool is_ignored; // Radio NodeInfo.is_ignored; this affects radio reception.
    bool favorite_known; // A complete NodeInfo snapshot has been received.
    char user_id[40];
    uint8_t public_key[32];
    bool public_key_available, user_info_available;
    uint32_t role, first_seen;
    bool snr_available, rssi_available, via_mqtt;
    int32_t rssi;
    bool position_available;
    int32_t latitude_i, longitude_i; // Radio-reported coordinates, 1e-7 degrees.
    uint32_t position_time; // Reported fix time, zero when unknown.
    uint8_t position_precision; // Radio privacy precision, zero when unspecified.
    mesh_device_metrics_t metrics;
} mesh_node_t;

// Received reports only: these getters never request or transmit telemetry.
bool mesh_radio_get_node_metrics(uint32_t node, mesh_device_metrics_t *out);
bool mesh_radio_get_local_activity(mesh_device_metrics_t *out);
typedef enum { MESH_TELEMETRY_WAITING=1, MESH_TELEMETRY_REPLY, MESH_TELEMETRY_TIMEOUT,
    MESH_TELEMETRY_FAILED, MESH_TELEMETRY_INTERRUPTED } mesh_telemetry_state_t;
typedef struct {
    uint32_t node, origin, request_id, error_reason;
    mesh_telemetry_state_t state;
    unsigned elapsed_seconds;
    uint8_t kind;
} mesh_telemetry_request_t;
typedef enum { MESH_TELEMETRY_START_OK, MESH_TELEMETRY_START_INVALID,
    MESH_TELEMETRY_START_OFFLINE, MESH_TELEMETRY_START_BUSY,
    MESH_TELEMETRY_START_COOLDOWN, MESH_TELEMETRY_START_WRITE_FAILED } mesh_telemetry_start_t;
mesh_telemetry_start_t mesh_radio_request_telemetry(uint32_t node);
mesh_telemetry_start_t mesh_radio_request_telemetry_kind(uint32_t node,uint8_t kind);
bool mesh_radio_get_telemetry_request(uint32_t node, mesh_telemetry_request_t *out);
unsigned mesh_radio_telemetry_cooldown_seconds(void);

typedef enum {
    MESH_DELIVERY_NONE,
    MESH_DELIVERY_PENDING,
    MESH_DELIVERY_ACKNOWLEDGED,
    MESH_DELIVERY_FAILED,
} mesh_delivery_state_t;

typedef struct {
    char from[12];
    // Meshtastic text payloads can approach the 233-byte decoded data limit.
    char text[234];
    // Unix time in seconds; zero means the badge clock was not available.
    uint32_t timestamp;
    uint32_t packet_id;
    mesh_delivery_state_t delivery;
    uint32_t peer_node; // Zero for channel messages, otherwise the DM peer.
    bool outgoing;
    uint8_t channel; // Local channel slot for broadcast messages.
    uint32_t reply_id; // Data.reply_id, scoped to this conversation/channel.
    uint32_t error_reason; // Meshtastic Routing.Error, only when failure == RADIO.
    uint8_t failure; // mesh_message_failure_t; never infer a radio NAK from a local timeout.
} mesh_message_t;
typedef enum { MESH_FAILURE_UNKNOWN, MESH_FAILURE_RADIO, MESH_FAILURE_TIMEOUT,
    MESH_FAILURE_RESTARTED } mesh_message_failure_t;
// A reply never adds quoted text to the radio payload or changes its 200-byte limit.
bool mesh_radio_send_reply(uint32_t peer, uint8_t channel, const char *text, uint32_t reply_id);
bool mesh_radio_get_reply_parent(const mesh_message_t *reply, mesh_message_t *out);
const char *mesh_radio_delivery_text(const mesh_message_t *message);
bool mesh_radio_node_muted(uint32_t node);
bool mesh_radio_set_node_muted(uint32_t node, bool muted);
bool mesh_radio_alert_unread(bool direct, uint8_t channel);


// Starts the built-in C6 Meshtastic serial link and requests its node database.
// This only changes the C6's run mode; it does not write firmware or settings.
bool mesh_radio_start(void);
void mesh_radio_request_config(void);

// Call regularly from the app task to consume and decode radio frames.
void mesh_radio_poll(void);
mesh_radio_status_t mesh_radio_status(void);
uint32_t mesh_radio_history_revision(void);
typedef struct {
    bool loaded, pending;
    char firmware_version[64];
    uint32_t hardware_model, role;
} mesh_radio_metadata_t;
mesh_radio_metadata_t mesh_radio_metadata(void);
bool mesh_radio_request_metadata(void);
unsigned mesh_radio_rx_age_seconds(void);
typedef struct { bool loaded; uint32_t role, revision, rebroadcast_mode, node_info_broadcast_secs; } mesh_device_config_t;
typedef enum { MESH_ROLE_IDLE, MESH_ROLE_SAVING, MESH_ROLE_SAVED, MESH_ROLE_FAILED, MESH_ROLE_TIMEOUT } mesh_role_state_t;
typedef struct { mesh_role_state_t state; uint32_t desired, error_reason; } mesh_role_save_t;
bool mesh_radio_get_device(mesh_device_config_t *out);
bool mesh_radio_request_device(void);
bool mesh_radio_role_supported(uint32_t role);
bool mesh_radio_set_role(uint32_t role, uint32_t expected_revision);
bool mesh_radio_behavior_supported(uint32_t role, uint32_t mode);
bool mesh_radio_set_behavior(uint32_t mode, uint32_t interval, uint32_t expected_revision);
mesh_role_save_t mesh_radio_role_save(void);

typedef struct {
    bool loaded, smart, fixed;
    uint32_t broadcast_secs, revision;
} mesh_position_config_t;
typedef enum { MESH_LOCATION_IDLE, MESH_LOCATION_SAVING, MESH_LOCATION_SAVED,
    MESH_LOCATION_FAILED, MESH_LOCATION_TIMEOUT } mesh_location_state_t;
typedef struct { mesh_location_state_t state; uint32_t error_reason; } mesh_location_save_t;
bool mesh_radio_get_position_config(mesh_position_config_t *out);
bool mesh_radio_request_position_config(void);
bool mesh_radio_set_position_config(const mesh_position_config_t *desired);
bool mesh_radio_set_fixed_position(int32_t latitude_i, int32_t longitude_i, uint32_t expected_revision);
mesh_location_save_t mesh_radio_location_save(void);

// Local radio device-metrics broadcasts only. Sensor configuration is retained.
typedef struct {
    bool loaded, enabled, enable_supported;
    uint32_t interval_secs, revision;
} mesh_telemetry_config_t;
typedef enum { MESH_TELEMETRY_CONFIG_IDLE, MESH_TELEMETRY_CONFIG_SAVING,
    MESH_TELEMETRY_CONFIG_SAVED, MESH_TELEMETRY_CONFIG_FAILED,
    MESH_TELEMETRY_CONFIG_TIMEOUT, MESH_TELEMETRY_CONFIG_INTERRUPTED } mesh_telemetry_config_state_t;
typedef struct { mesh_telemetry_config_state_t state; uint32_t error_reason; } mesh_telemetry_save_t;
bool mesh_radio_get_telemetry_config(mesh_telemetry_config_t *out);
bool mesh_radio_request_telemetry_config(void);
bool mesh_radio_set_telemetry_config(const mesh_telemetry_config_t *desired);
mesh_telemetry_save_t mesh_radio_telemetry_save(void);

// Drain live incoming text events: bits 0..7 are channel slots, bit 8 is DM.
// Independent of unread state; restored history and reactions never set bits.
uint16_t mesh_radio_take_message_alerts(void);

#define MESH_TRACE_HOPS 8
typedef enum { MESH_TRACE_WAITING = 1, MESH_TRACE_COMPLETE, MESH_TRACE_TIMEOUT,
    MESH_TRACE_ERROR, MESH_TRACE_INTERRUPTED } mesh_trace_state_t;
typedef struct {
    uint32_t node, origin, request_id, requested_at, completed_at, error_reason;
    mesh_trace_state_t state;
    uint8_t route_count, return_count, snr_count, return_snr_count;
    uint32_t route[MESH_TRACE_HOPS], return_route[MESH_TRACE_HOPS];
    int32_t snr[MESH_TRACE_HOPS + 1], return_snr[MESH_TRACE_HOPS + 1];
} mesh_trace_result_t;
typedef enum { MESH_TRACE_START_OK, MESH_TRACE_START_OFFLINE, MESH_TRACE_START_INVALID,
    MESH_TRACE_START_BUSY, MESH_TRACE_START_COOLDOWN, MESH_TRACE_START_WRITE_FAILED } mesh_trace_start_t;
mesh_trace_start_t mesh_radio_start_traceroute(uint32_t node);
bool mesh_radio_get_traceroute(uint32_t node, mesh_trace_result_t *out);
unsigned mesh_radio_traceroute_elapsed(uint32_t node);
size_t mesh_radio_node_count(void);
bool mesh_radio_get_node(size_t index, mesh_node_t *out_node);
bool mesh_radio_get_node_by_number(uint32_t number, mesh_node_t *out_node);
void mesh_radio_flush_node_cache(void);
size_t mesh_radio_history_limit(void);
bool mesh_radio_set_history_limit(size_t limit);
bool mesh_radio_clear_thread(uint32_t peer, uint8_t channel, size_t *removed);
bool mesh_radio_flush_history(void);
void mesh_radio_poll_history(bool busy);
bool mesh_radio_retry_message(const mesh_message_t *target);
typedef enum { MESH_FAVORITE_IDLE, MESH_FAVORITE_SAVING, MESH_FAVORITE_SAVED,
    MESH_FAVORITE_FAILED, MESH_FAVORITE_TIMEOUT, MESH_FAVORITE_INTERRUPTED } mesh_favorite_state_t;
typedef struct {
    uint32_t node, error_reason;
    bool desired;
    mesh_favorite_state_t state;
} mesh_favorite_status_t;
typedef enum { MESH_FAVORITE_START_OK, MESH_FAVORITE_START_INVALID, MESH_FAVORITE_START_BUSY,
    MESH_FAVORITE_START_OFFLINE, MESH_FAVORITE_START_NOT_READY, MESH_FAVORITE_START_WRITE_FAILED } mesh_favorite_start_t;
mesh_favorite_start_t mesh_radio_set_favorite(uint32_t number, bool favorite);
typedef enum { MESH_NODE_IGNORE, MESH_NODE_UNIGNORE, MESH_NODE_REMOVE } mesh_node_control_kind_t;
typedef struct {
    uint32_t node, error_reason;
    mesh_node_control_kind_t kind;
    mesh_favorite_state_t state;
} mesh_node_control_status_t;
mesh_node_control_status_t mesh_radio_node_control_status(void);
mesh_favorite_start_t mesh_radio_control_node(uint32_t number, mesh_node_control_kind_t kind);
mesh_favorite_status_t mesh_radio_favorite_status(void);
size_t mesh_radio_message_count(void);
// Local history only. Rejects missing/ambiguous snapshots and pending sends.
bool mesh_radio_delete_message(const mesh_message_t *target);
bool mesh_radio_get_message(size_t index, mesh_message_t *out_message);
// Resolve a display label from current NodeDB without changing stored sender IDs.
void mesh_radio_message_sender_name(const mesh_message_t *message, char *out, size_t size);
bool mesh_radio_send_text(const char *text);
bool mesh_radio_request_owner(void);
bool mesh_radio_admin_ready(void);
bool mesh_radio_set_owner(const char *long_name, const char *short_name);
bool mesh_radio_get_owner(char *long_name, size_t long_name_size,
                          char *short_name, size_t short_name_size);

// Direct messages use the radio firmware's normal routing/encryption behavior.
bool mesh_radio_send_direct(uint32_t destination, const char *text);
uint32_t mesh_radio_local_node(void);
size_t mesh_radio_conversation_count(uint32_t peer_node);
bool mesh_radio_get_conversation_message(uint32_t peer_node, size_t index, mesh_message_t *out);
size_t mesh_radio_direct_peer_count(void);
bool mesh_radio_get_direct_peer(size_t index, uint32_t *peer_node);

#define MESH_CHANNEL_LIMIT 8
typedef struct {
    bool loaded;
    uint8_t index;
    uint8_t role; // 0 disabled, 1 primary, 2 secondary.
    char name[12];
    bool uplink_enabled;
    bool downlink_enabled;
    bool has_key;
    bool default_key;
    bool has_position_precision;
    uint32_t position_precision;
    uint32_t revision;
} mesh_channel_t;
bool mesh_radio_get_channel(uint8_t index, mesh_channel_t *out);
bool mesh_radio_request_channel(uint8_t index);
bool mesh_radio_set_channel(uint8_t index, const char *name, bool uplink, bool downlink);
// precision=-1 preserves the existing module settings, including absent defaults.
bool mesh_radio_set_channel_location(uint8_t index, const char *name, bool uplink,
    bool downlink, int precision, uint32_t expected_revision);

// Standard channel URLs carry one or more ChannelSettings records.
#define MESH_CHANNEL_URL_MAX 2400
typedef struct {
    uint8_t settings[8][160]; uint16_t sizes[8]; uint8_t count;
    bool has_lora; // Import deliberately leaves the radio's LoRa settings unchanged.
} mesh_channel_share_t;
bool mesh_radio_channel_url(uint8_t index, char *out, size_t capacity);
bool mesh_radio_parse_channel_url(const char *url, mesh_channel_share_t *out);
bool mesh_radio_share_channel_info(const mesh_channel_share_t *share, uint8_t entry, mesh_channel_t *out);
bool mesh_radio_export_channel_settings(uint8_t index, uint8_t *out, size_t *size);
bool mesh_radio_manage_channel(uint8_t index, uint8_t role, const uint8_t *settings, size_t size, uint32_t revision);
bool mesh_radio_replace_channel_key(uint8_t index, const char *key_text, uint32_t revision);
bool mesh_radio_set_channel_role(uint8_t index, uint8_t role, bool erase, uint32_t revision);
bool mesh_radio_generate_channel_key(char *out, size_t capacity);
typedef struct { mesh_favorite_state_t state; uint8_t index; uint32_t error_reason; } mesh_channel_operation_t;
mesh_channel_operation_t mesh_radio_channel_operation(void);

// A staged share contains only ChannelSettings, never LoRa radio configuration.
bool mesh_radio_get_shared_channel(mesh_channel_t *out);
bool mesh_radio_add_shared_channel(uint8_t index);
bool mesh_radio_shared_channel_matches(uint8_t index);

bool mesh_radio_send_channel(uint8_t channel, const char *text);
size_t mesh_radio_channel_message_count(uint8_t channel);
bool mesh_radio_get_channel_message(uint8_t channel, size_t index, mesh_message_t *out);

// Counts cover incoming messages retained in the shared history.
size_t mesh_radio_unread_channel(uint8_t channel);
size_t mesh_radio_unread_direct(uint32_t peer_node);
size_t mesh_radio_unread_total(bool direct);
void mesh_radio_mark_read(uint32_t peer_node, uint8_t channel);
// Per-thread chronological indexes, matching the existing message getters.
bool mesh_radio_first_unread(uint32_t peer,uint8_t channel,size_t *index);
void mesh_radio_mark_read_range(uint32_t peer,uint8_t channel,size_t first,size_t end);

typedef struct {
    uint32_t sender, packet_id, reply_id, peer_node, timestamp;
    uint8_t channel;
    char emoji[64];
} mesh_reaction_t;
// Automatic history eviction only; intentional delete/clear stays separate.
// Borrowed data is valid during this app-task callback only.
typedef void (*mesh_history_archive_fn)(const mesh_message_t *, const mesh_reaction_t *, size_t, const char *);
void mesh_radio_set_history_archive(mesh_history_archive_fn callback);
size_t mesh_radio_reaction_count(const mesh_message_t *message);
bool mesh_radio_get_reaction(const mesh_message_t *message, size_t index, mesh_reaction_t *out);
// Queues a reaction to a retained message, preserving its channel/DM scope.
// Success means accepted by the local UART, not delivery confirmation.
bool mesh_radio_send_reaction(const mesh_message_t *message, const char *emoji);

typedef struct {
    bool loaded;
    bool use_preset;
    uint32_t region;
    uint32_t modem_preset;
    uint8_t hop_limit;
    uint32_t revision;
} mesh_lora_config_t;
bool mesh_radio_get_lora(mesh_lora_config_t *out);
bool mesh_radio_request_lora(void);
bool mesh_radio_set_lora(uint32_t region, uint32_t preset, uint8_t hops, bool use_preset);

// Accepts standard/URL-safe Base64 or 16/32-byte hex (optional 0x prefix).
bool mesh_radio_channel_key_valid(const char *key_text);
bool mesh_radio_add_manual_channel(uint8_t index, const char *name, const char *key_text);
bool mesh_radio_manual_channel_matches(uint8_t index, const char *name, const char *key_text);

// Shared map markers use standard WAYPOINT_APP (port8); never enter text/unread history.
#define MESH_WAYPOINT_LIMIT 64
typedef struct {
    uint32_t id, creator, updated_by, locked_to, expire, timestamp, packet_id, peer_node, icon;
    int32_t latitude_i, longitude_i;
    uint8_t channel;
    char name[30], description[100];
    uint8_t extra[233]; // Unknown future waypoint fields, preserved on edit.
    uint16_t extra_size;
    bool deleted;
    mesh_delivery_state_t delivery;
    uint32_t error_reason;
    uint8_t failure;
} mesh_waypoint_t;
size_t mesh_radio_waypoint_count(void);
bool mesh_radio_get_waypoint(size_t index, mesh_waypoint_t *out);
bool mesh_radio_find_waypoint(uint32_t id, uint32_t peer, uint8_t channel, mesh_waypoint_t *out);
bool mesh_radio_send_waypoint(const mesh_waypoint_t *draft, bool remove);
bool mesh_radio_waypoint_editable(const mesh_waypoint_t *waypoint);
uint32_t mesh_radio_new_waypoint_id(void);
bool mesh_radio_flush_waypoints(void);

uint32_t mesh_radio_time(void);

#include "key_backup.h"
typedef enum { MESH_KEYS_IDLE,MESH_KEYS_READING,MESH_KEYS_READY,MESH_KEYS_SAVING,MESH_KEYS_SAVED,MESH_KEYS_FAILED,MESH_KEYS_TIMEOUT,MESH_KEYS_INTERRUPTED } mesh_radio_keys_state_t;
bool mesh_radio_keys_request(void);
bool mesh_radio_keys_snapshot(mesh_key_backup_t *out,bool identity,bool channels);
uint32_t mesh_radio_keys_revision(void);
mesh_radio_keys_state_t mesh_radio_keys_state(void);
void mesh_radio_keys_clear(void);
bool mesh_radio_keys_restore_identity(const uint8_t public_key[32],const uint8_t private_key[32],uint32_t revision);
bool mesh_radio_keys_restore_channel(uint8_t index,const mesh_backup_channel_t *saved,uint32_t revision);

// Explicit NODEINFO_APP request; one per minute globally, no automatic resend.
bool mesh_radio_request_node_info(uint32_t number);

// App-only labels; empty string clears a nickname. Never transmitted to the radio.
bool mesh_radio_set_nickname(uint32_t node,const char *name);
const char *mesh_radio_nickname(uint32_t node);
size_t mesh_radio_nicknames_export(void *out,size_t capacity);
bool mesh_radio_nicknames_import(const void *data,size_t size);
size_t mesh_radio_mutes_export(void *out,size_t capacity);
bool mesh_radio_mutes_import(const void *data,size_t size);
