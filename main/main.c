#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "bsp/device.h"
#include "bsp/display.h"
#include "bsp/input.h"
#include "bsp/led.h"
#include "bsp/power.h"
#include "bsp/rtc.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_types.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "hal/lcd_types.h"
#include "nvs_flash.h"
#include "pax_fonts.h"
#include "pax_gfx.h"
#include "pax_text.h"
#include "pax_types.h"
#include "portmacro.h"
#include "radio_link.h"
#include "config_backup.h"
#include "emoji_text.h"
#include "emoji_picker.h"
#include "composer_view.h"
#include "display_settings.h"
#include "power_settings.h"
#include "backlight.h"
#include "node_details.h"
#include "node_browser.h"
#include "brand_logo.h"
#include "text_settings.h"
#include "notification_time.h"
#include "role_options.h"
#include "app_version.h"
#include "map_view.h"
#include "map_trails.h"
#include "map_tiles.h"
#include "location_utils.h"
#include "units.h"
#include "telemetry_store.h"
#include "telemetry_export.h"
#include "message_export.h"
#include "range_export.h"
#include "message_archive.h"
#include "chat_actions.h"
#include "draft_store.h"
#include "message_navigation.h"
#include "settings_menu.h"
static uint8_t distance_units=MESH_UNITS_METRIC,temperature_units=MESH_TEMP_CELSIUS;
#include "telemetry_format.h"
#include "packet_runner.h"
#include "packet_scores.h"
#include "message_search.h"
#include "sound_settings.h"
#include "sound_player.h"
#include "third_party/qrcodegen.h"

// Constants
static char const TAG[] = "main";

#if defined(CONFIG_BSP_TARGET_KAMI)
#define BLACK 0
#define WHITE 1
#define RED   2
#else
#define BLACK 0xFF101513
#define WHITE 0xFFFFFFFF
#define RED   0xFFFF0000
#define KEY_X_COLOR 0xFFFF4965
#define KEY_TRIANGLE_COLOR 0xFFFFB72B
#define KEY_SQUARE_COLOR 0xFFD7EF00
#define KEY_CIRCLE_COLOR 0xFF42D392
#define KEY_DIAMOND_COLOR 0xFFFF49D1
#endif

// Shared dark green palette for all screens.
#define UI_GREEN 0xFF67EA94
#define UI_MUTED 0xFFA7B9AF
#define UI_PANEL 0xFF1B2821
#define UI_BORDER 0xFF30483A
#define UI_SELECTED 0xFF253E30
#define UI_FOOTER_HEIGHT 42
#define UI_HINT_OFFSET (UI_FOOTER_HEIGHT + 18)

// Global variables
static size_t                     display_h_res        = 0;
static size_t                     display_v_res        = 0;
static size_t                     logical_h_res        = 0;
static size_t                     logical_v_res        = 0;
static bsp_display_color_format_t display_color_format = 0;
static bsp_display_endianness_t   display_data_endian  = 0;
static pax_buf_t                  fb                   = {0};
static QueueHandle_t              input_event_queue    = NULL;
typedef enum {
    SCREEN_HOME,
    SCREEN_MESSAGES,
    SCREEN_NODES,
    SCREEN_SETTINGS,
    SCREEN_DIRECT_MESSAGES,
    SCREEN_SETTINGS_MENU,
    SCREEN_CHANNELS,
    SCREEN_CHANNEL_EDIT,
    SCREEN_CHANNEL_IMPORT,
    SCREEN_CHANNEL_LIST,
    SCREEN_RADIO_SETTINGS,
    SCREEN_MANUAL_CHANNEL,
    SCREEN_DISPLAY_ALERTS,
    SCREEN_POWER_SETTINGS,
    SCREEN_TEXT_SETTINGS,
    SCREEN_ABOUT,
    SCREEN_CHANNEL_ALERTS,
    SCREEN_TIME_SETTINGS,
    SCREEN_ROLE_SETTINGS,
    SCREEN_RADIO_BEHAVIOR,
    SCREEN_HISTORY_SETTINGS,
    SCREEN_MAP,
    SCREEN_NODE_SETTINGS_MENU,
    SCREEN_KEY_BACKUP,
    SCREEN_CONFIG_BACKUP,
    SCREEN_UNITS,
    SCREEN_DISPLAY_SETTINGS_MENU,
    SCREEN_POSITION_SETTINGS,
    SCREEN_FIXED_POSITION,
    SCREEN_RADIO_ACTIVITY,
    SCREEN_PACKET_RUNNER,
    SCREEN_MESSAGE_SEARCH,
    SCREEN_SOUND_SETTINGS,
    SCREEN_TELEMETRY_SETTINGS,
    SCREEN_CHANNEL_TOOLS,
    SCREEN_WAYPOINTS,
    SCREEN_SECURITY,
    SCREEN_REMOTE,
    SCREEN_RANGE_TEST,
    SCREEN_ARCHIVE_SETTINGS, SCREEN_ARCHIVE_BROWSER,
    SCREEN_NEIGHBOR_INFO, SCREEN_NEIGHBOR_SETTINGS,
    SCREEN_FIRST_USE,
} meshdeck_screen_t;
static meshdeck_screen_t current_screen = SCREEN_HOME;
static uint32_t message_peer;
static uint8_t message_channel;
static uint8_t channel_chat_selection;
static char message_peer_name[32];
static size_t home_selection;
static size_t direct_selection;
static uint32_t direct_selected_peer;
static size_t node_selection;
// Node browser state.
static mesh_node_browser_t node_browser = {.favorites_first = true};
static bool node_search_editing;
static char node_search_before[sizeof(node_browser.query)];
static uint32_t node_search_selected;
// End node browser state.
// Node actions keep their target stable even while the node list updates.
enum { NODE_ACTIONS = 1, NODE_TRACE, NODE_TELEMETRY, NODE_CONTROL_CONFIRM, NODE_NICKNAME };
static unsigned node_modal;
static bool map_action_origin;
static void go_back(void);
static void go_home(void);
static void open_security_centre(uint32_t);
static void open_remote_manager(uint32_t);
static void open_range_testing(uint32_t);
static void open_neighbor_info(uint32_t);
static void open_archive_settings(void);
static void open_archive_browser(bool,uint32_t,uint8_t);
static void telemetry_view_open(uint32_t node,bool local);
static mesh_node_t action_node;
static size_t node_action_selection, trace_scroll;
static bool trace_return_selected;
static bool node_control_confirm;
static mesh_node_control_kind_t node_control_kind;
static char node_action_feedback[80];
static char nickname_draft[40];
static bool nickname_replace;
static char telemetry_feedback[96];
// End node action state.
// A draft remains with its recipient when switching screens/conversations.
static mesh_draft_t drafts[MESH_DRAFT_LIMIT];
static size_t draft_count = 1;
static char *message_draft = drafts[0].text;
static char message_feedback[64];
static size_t message_scroll_offset;
static mesh_message_anchor_t message_anchor;
static bool message_visible_valid;
static size_t message_visible_first,message_visible_end;
static uint32_t message_visible_peer;
static uint8_t message_visible_channel;
static void open_message_search(void);
// Chat action state (kept outside the screen history).
static EXT_RAM_BSS_ATTR mesh_chat_actions_t chat_actions;
static uint32_t draft_reply_ids[MESH_NODE_LIMIT + 60];
static size_t draft_slot;
static int64_t chat_action_enter_us;
// End chat action state.
#include "draft_ui.inc"
static char settings_long_name[40];
static char settings_short_name[5];
static bool settings_names_loaded;
static bool settings_edits_started;
static bool settings_long_edited;
static bool settings_short_edited;
static bool settings_short_name_selected;
static bool settings_save_pending;
static char settings_feedback[64];
static size_t settings_menu_selection;
static size_t node_settings_selection, display_settings_selection;
// History settings state.
static size_t history_limit_draft, history_field;
static bool history_limit_confirm, home_exit_failed,home_exit_waiting,home_exit_discard;
static bool archive_exit_discard;
static char history_feedback[80];
static void open_history_settings(void) {
    current_screen = SCREEN_HISTORY_SETTINGS; history_limit_draft = mesh_radio_history_limit();
    history_field = 0; history_limit_confirm = false; history_feedback[0] = '\0';
}
static bool history_settings_navigation(int key) {
    if (current_screen != SCREEN_HISTORY_SETTINGS) return false;
    if (key == BSP_INPUT_NAVIGATION_KEY_UP) history_field = 0;
    else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) history_field = 1;
    else if ((key == BSP_INPUT_NAVIGATION_KEY_LEFT || key == BSP_INPUT_NAVIGATION_KEY_RIGHT) && !history_field) {
        const size_t options[] = {100, 250, 500, 1000};
        for (size_t i = 0; i < 4; i++) if (history_limit_draft == options[i]) {
            history_limit_draft = options[(i + (key == BSP_INPUT_NAVIGATION_KEY_RIGHT ? 1 : 3)) % 4]; break;
        }
        history_limit_confirm = false; snprintf(history_feedback, sizeof(history_feedback), "Unsaved limit.");
    } else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN && history_field == 1) {
        chat_action_enter_us = esp_timer_get_time();
        if (mesh_radio_message_count() > history_limit_draft && !history_limit_confirm) {
            history_limit_confirm = true;
            snprintf(history_feedback, sizeof(history_feedback), "Removes older recent history. Enter again: confirm.");
        } else if (mesh_radio_set_history_limit(history_limit_draft)) {
            history_limit_confirm = false; snprintf(history_feedback, sizeof(history_feedback), "History limit saved.");
        } else snprintf(history_feedback, sizeof(history_feedback), "Not saved. Check storage / pending messages.");
    } else return false;
    return true;
}
// End history settings state.
// Message text preferences.
static unsigned about_action_selection;
static uint8_t message_text_size = MESH_TEXT_MEDIUM, text_size_draft = MESH_TEXT_MEDIUM;
static char text_size_feedback[64];
static void open_text_settings(void) {
    current_screen = SCREEN_TEXT_SETTINGS; text_size_draft = message_text_size; text_size_feedback[0] = '\0';
}
static bool extra_settings_navigation(int key) {
    if (current_screen == SCREEN_TEXT_SETTINGS) {
        if (key == BSP_INPUT_NAVIGATION_KEY_LEFT && text_size_draft) text_size_draft--;
        else if (key == BSP_INPUT_NAVIGATION_KEY_RIGHT && text_size_draft < MESH_TEXT_LARGE) text_size_draft++;
        else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) {
            if (mesh_text_size_save(text_size_draft) == ESP_OK) {
                message_text_size = text_size_draft;
                message_scroll_offset = 0;
                snprintf(text_size_feedback, sizeof(text_size_feedback), "Text size saved.");
            } else snprintf(text_size_feedback, sizeof(text_size_feedback), "Could not save. Please retry.");
            return true;
        } else return false;
        snprintf(text_size_feedback, sizeof(text_size_feedback), "Preview: Enter to save. Esc to cancel.");
        return true;
    }
    if (current_screen == SCREEN_ABOUT) {
        if (key == BSP_INPUT_NAVIGATION_KEY_LEFT || key == BSP_INPUT_NAVIGATION_KEY_UP) about_action_selection=0;
        else if (key == BSP_INPUT_NAVIGATION_KEY_RIGHT || key == BSP_INPUT_NAVIGATION_KEY_DOWN) about_action_selection=1;
        else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) {
            if (about_action_selection) current_screen=SCREEN_RADIO_ACTIVITY;
            else mesh_radio_request_metadata();
        } else return false;
        return true;
    }
    if (current_screen == SCREEN_RADIO_ACTIVITY && key == BSP_INPUT_NAVIGATION_KEY_RETURN) return true;
    return false;
}
// End message text preferences.

static size_t channel_selection;
static size_t channel_field;
static char channel_name[12];
static bool channel_name_edited;
static bool channel_uplink, channel_downlink;
static int channel_precision;
static mesh_channel_t editing_channel;
static bool channel_save_pending;
static int64_t channel_save_started;
static char channel_feedback[80];
static size_t import_slot = 1;
static bool import_pending;
static uint32_t import_revision;
static int64_t import_started;
static char import_feedback[80];
// Keep the real navigation path, including a direct chat opened from Nodes.
static struct { meshdeck_screen_t screen; uint32_t peer; uint8_t channel; } screen_history[24];
static size_t screen_history_depth;
static meshdeck_screen_t tracked_screen = SCREEN_HOME;
static uint32_t tracked_peer;
static uint8_t tracked_channel;
static bool skip_history_capture;
static size_t radio_field;
static bool radio_values_loaded, radio_edits_started, radio_save_pending;
static mesh_lora_config_t radio_baseline, radio_draft;
static int64_t radio_save_started;
static char radio_feedback[80];
typedef struct { uint32_t value; const char *name; } radio_option_t;
static const radio_option_t region_options[] = {
    {1,"US"}, {3,"EU_868"}, {5,"JP"}, {6,"ANZ"}, {7,"KR"}, {8,"TW"}, {9,"RU"},
    {10,"IN"}, {11,"NZ_865"}, {12,"TH"}, {15,"UA_868"}, {17,"MY_919"}, {18,"SG_923"},
    {20,"PH_868"}, {21,"PH_915"}, {24,"KZ_863"}, {25,"NP_865"}, {26,"BR_902"}
};
static const radio_option_t preset_options[] = {
    {0,"LONG_FAST"}, {3,"MEDIUM_SLOW"}, {4,"MEDIUM_FAST"}, {5,"SHORT_SLOW"},
    {6,"SHORT_FAST"}, {7,"LONG_MODERATE"}, {8,"SHORT_TURBO"}, {9,"LONG_TURBO"}
};
static size_t manual_field, manual_slot = 1;
static char manual_name[12], manual_key[67];
static bool manual_key_visible;
static unsigned manual_phase; // 0 editing, 1 fresh slot check, 2 save readback.
static uint32_t manual_revision;
static int64_t manual_started;
static char manual_feedback[80];
static bool display_and_lights_off;
static uint8_t saved_display_brightness = 100;
static uint8_t saved_led_brightness = 100;
static uint8_t saved_keyboard_brightness = 100;
static mesh_display_settings_t display_settings, display_draft;
// Additional channel alerts, local time and radio-role settings.
static uint8_t channel_alerts_mask = 0xff, channel_alerts_draft = 0xff;
static size_t channel_alert_field, time_field, role_field;
static mesh_time_settings_t time_settings = {0,0,-420}, time_draft;
static mesh_device_config_t role_baseline;
static uint32_t role_draft;
static bool role_loaded, role_edited;
static char channel_alert_feedback[80], time_feedback[80], role_feedback[80];
// End additional settings state.

static uint8_t active_display_brightness = 100, active_keyboard_brightness = 100;
static bool display_preview_active;
static size_t display_field;
static char display_feedback[80];
static int64_t last_activity_us, screen_retry_at;
static mesh_power_settings_t power_settings, power_draft;
static size_t power_field;
static char power_feedback[80];
static int applied_screen = -1, applied_keyboard = -1;
static bool ui_dirty = true;
static char battery_text[8] = "--%";
static bool compose_redraw_pending;
static int64_t compose_redraw_due_us, last_text_edit_us, last_screen_draw_us;
static int displayed_compose_height;
static bool displayed_chat;
static uint32_t displayed_peer;
static uint8_t displayed_channel;

static void update_message_light(void) {
#if defined(CONFIG_BSP_TARGET_TANMATSU)
    static int applied = -1, observed = -1;
    static int64_t retry_at;
    bool channel_unread = false;
    for (uint8_t i = 0; i < MESH_CHANNEL_LIMIT; i++) {
        if (mesh_channel_alert_enabled(channel_alerts_mask, i) && mesh_radio_alert_unread(0, i)) { channel_unread = true; break; }
    }
    bool unread = mesh_display_alert(&display_settings, channel_unread, mesh_radio_alert_unread(1, 0));
    uint8_t brightness = display_and_lights_off ? (unread ? display_settings.night_brightness : 0) : saved_led_brightness;
    bool decorative = power_settings.decorative_lights && !display_and_lights_off;
    if (!decorative && !unread) brightness = 0;
    int desired = (int)unread | ((int)display_and_lights_off << 1) | ((int)brightness << 2) | ((int)decorative << 10);
    if (observed != desired) { observed = desired; retry_at = 0; }
    if (applied == desired || esp_timer_get_time() < retry_at) return;
    // Official CH32 firmware maps message to pixel 2. Its message register
    // updates the automatic buffer, while MeshDeck uses the manual buffer.
    static const uint32_t awake_colors[] = {0xff0000, 0x00ff00, 0, 0xffff00, 0x00ffff, 0xff00ff};
    esp_err_t result = bsp_led_set_mode(false);
    for (size_t i = 0; result == ESP_OK && i < 6; i++) {
        uint32_t color = i == 2 ? (unread ? 0x0000ff : 0) : decorative ? awake_colors[i] : 0;
        result = bsp_led_set_pixel(i, color);
    }
    // Send the isolated pixel frame before enabling brightness, so turning
    // the message light on cannot illuminate the other five LEDs at night.
    if (result == ESP_OK) result = bsp_led_send();
    if (result == ESP_OK) result = bsp_led_set_brightness(brightness);
    if (result == ESP_OK) applied = desired;
    else {
        retry_at = esp_timer_get_time() + 5000000;
        ESP_LOGW(TAG, "Could not update message LED: %s", esp_err_to_name(result));
    }
#endif
}

#if defined(CONFIG_BSP_TARGET_KAMI)
// Temporary addition for supporting epaper devices (irrelevant for Tanmatsu)
static pax_col_t palette[] = {0xffffffff, 0xff000000, 0xffff0000};  // white, black, red
#endif

#if defined(CONFIG_BSP_TARGET_KAMI)
#define KEY_X_COLOR 2
#define KEY_TRIANGLE_COLOR 2
#define KEY_SQUARE_COLOR 2
#define KEY_CIRCLE_COLOR 2
#define KEY_DIAMOND_COLOR 2
#endif

static bool blit(void) {
    esp_err_t res = bsp_display_blit(0, 0, display_h_res, display_v_res, pax_buf_get_pixels(&fb));
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to blit to display: %d", res);
    }
    return res == ESP_OK;
}

static int draw_wrapped_text(const char *text, const char *first_prefix, const char *next_prefix,
                             int x, int y, int font_size, int max_chars, int line_height, int max_lines) {
    size_t text_length = strlen(text);
    size_t offset = 0;
    int lines = 0;

    do {
        const char *prefix = lines == 0 ? first_prefix : next_prefix;
        size_t prefix_length = strlen(prefix);
        size_t capacity = max_chars > (int)prefix_length ? (size_t)(max_chars - prefix_length) : 1;
        size_t remaining = text_length - offset;
        size_t take = remaining < capacity ? remaining : capacity;

        if (take < remaining) {
            size_t break_at = take;
            while (break_at > 0 && text[offset + break_at] != ' ' && text[offset + break_at] != '\t') break_at--;
            if (break_at > 0) take = break_at;
        }

        char line[96];
        size_t used = prefix_length < sizeof(line) - 1 ? prefix_length : sizeof(line) - 1;
        memcpy(line, prefix, used);
        size_t copy = take < sizeof(line) - used - 1 ? take : sizeof(line) - used - 1;
        if (copy) memcpy(line + used, text + offset, copy);
        line[used + copy] = '\0';
        pax_draw_text(&fb, WHITE, pax_font_sky_mono, font_size, x, y + lines * line_height, line);

        offset += take;
        while (offset < text_length && (text[offset] == ' ' || text[offset] == '\t')) offset++;
        lines++;
    } while (offset < text_length && lines < max_lines);

    return y + lines * line_height;
}

static void draw_footer_icon(int key, float x, float y, pax_col_t color) {
    if (key == 0) {
        pax_draw_line(&fb, color, x - 4, y - 4, x + 4, y + 4);
        pax_draw_line(&fb, color, x - 4, y + 4, x + 4, y - 4);
    } else if (key == 1) {
        pax_outline_tri(&fb, color, x, y - 5, x - 5, y + 4, x + 5, y + 4);
    } else if (key == 2) {
        pax_outline_rect(&fb, color, x - 4, y - 4, 8, 8);
    } else if (key == 3) {
        pax_outline_circle(&fb, color, x, y, 4.5f);
    } else if (key == 4) {
        // Match the three-leaf clover printed on F5.
        float previous_x = 0, previous_y = 0;
        for (int i = 0; i <= 48; i++) {
            float angle = -1.5707963f + i * 6.2831853f / 48;
            float radius = 4.2f + 2.0f * cosf(3 * (angle + 1.5707963f));
            float next_x = x + radius * cosf(angle);
            float next_y = y + radius * sinf(angle);
            if (i) pax_draw_line(&fb, color, previous_x, previous_y, next_x, next_y);
            previous_x = next_x; previous_y = next_y;
        }
    } else if (key == 5) {
        pax_draw_line(&fb, color, x, y - 5, x + 5, y);
        pax_draw_line(&fb, color, x + 5, y, x, y + 5);
        pax_draw_line(&fb, color, x, y + 5, x - 5, y);
        pax_draw_line(&fb, color, x - 5, y, x, y - 5);
    }
}

static void draw_footer(void) {
    int top = (int)logical_v_res - UI_FOOTER_HEIGHT;
    pax_draw_rect(&fb, BLACK, 0, top, logical_h_res, UI_FOOTER_HEIGHT);
    pax_draw_line(&fb, UI_BORDER, 0, top, logical_h_res, top);
    int active = (current_screen == SCREEN_MAP || current_screen == SCREEN_WAYPOINTS) ? 4 : current_screen == SCREEN_HOME ? 0 :
                 (current_screen == SCREEN_CHANNEL_LIST || (current_screen == SCREEN_MESSAGES && !message_peer)) ? 1 :
                 (current_screen == SCREEN_DIRECT_MESSAGES || (current_screen == SCREEN_MESSAGES && message_peer)) ? 2 :
                 current_screen == SCREEN_NODES ? 3 : -1;
    const char *labels[] = {"HOME", "CHANNELS", "DIRECT", "NODES", "MAPS", "SCREEN"};
    for (int key = 0; key < 6; key++) {
        if (!labels[key][0]) continue;
        int left = (int)(logical_h_res * (size_t)key / 6);
        int right = (int)(logical_h_res * (size_t)(key + 1) / 6);
        bool selected = key == active;
        pax_col_t color = selected ? UI_GREEN : UI_MUTED;
        pax_draw_rect(&fb, selected ? UI_SELECTED : UI_PANEL, left + 4, top + 5, right - left - 8, UI_FOOTER_HEIGHT - 10);
        if (selected) pax_draw_rect(&fb, UI_GREEN, left + 4, top + 12, 2, UI_FOOTER_HEIGHT - 24);
        int font_size = right - left < 100 ? 9 : 11;
        int label_offset = right - left < 100 ? 14 : 16;
        pax_vec2f text = pax_text_size(pax_font_sky_mono, font_size, labels[key]);
        float group_width = label_offset + text.x;
        float start = (left + right - group_width) * 0.5f;
        float center_y = top + UI_FOOTER_HEIGHT * 0.5f;
        draw_footer_icon(key, start + 5, center_y, color);
        pax_draw_text(&fb, color, pax_font_sky_mono, font_size, start + label_offset, center_y - text.y * 0.5f, labels[key]);
    }
    // Clover actions are contextual to an open channel or direct chat.
}

static esp_err_t apply_backlight_levels(uint8_t screen, uint8_t keyboard) {
    if (applied_screen != screen) {
        esp_err_t result = mesh_backlight_set(false, screen);
        if (result != ESP_OK) { applied_screen = -1; return result; }
        applied_screen = screen;
    }
    if (applied_keyboard != keyboard) {
        esp_err_t result = mesh_backlight_set(true, keyboard);
        if (result != ESP_OK) { applied_keyboard = -1; return result; }
        applied_keyboard = keyboard;
    }
    return ESP_OK;
}

static void update_idle_backlights(int64_t now, bool input_waiting) {
    static int observed = -1;
    static int64_t retry_at;
    mesh_power_backlights_t desired = mesh_power_backlights(&power_settings, display_and_lights_off,
        now, input_waiting ? now : last_activity_us, active_display_brightness, active_keyboard_brightness);
    int target = desired.screen | ((int)desired.keyboard << 8);
    if (target != observed) { observed = target; retry_at = 0; }
    if (now < retry_at) return;
    esp_err_t result = apply_backlight_levels(desired.screen, desired.keyboard);
    if (result != ESP_OK) {
        retry_at = now + 5000000;
        ESP_LOGW(TAG, "Could not update idle backlights: %s", esp_err_to_name(result));
    }
}

static void toggle_display_and_lights(void) {
    bool off = !display_and_lights_off;
    int64_t now = esp_timer_get_time();
    mesh_power_backlights_t previous = mesh_power_backlights(&power_settings, display_and_lights_off,
        now, last_activity_us, active_display_brightness, active_keyboard_brightness);
    esp_err_t result = apply_backlight_levels(off ? 0 : active_display_brightness, off ? 0 : active_keyboard_brightness);
    if (result == ESP_OK) {
        display_and_lights_off = off;
        last_activity_us = now;
    } else {
        apply_backlight_levels(previous.screen, previous.keyboard);
        screen_retry_at = now + 5000000;
        ESP_LOGW(TAG, "Could not change screen state: %s", esp_err_to_name(result));
    }
    update_message_light();
}

static void check_screen_timeout(int64_t now, bool input_waiting) {
    if (!display_and_lights_off && !input_waiting && now >= screen_retry_at &&
        mesh_display_idle_due(&display_settings, now, last_activity_us)) toggle_display_and_lights();
}

static int draw_message_timestamp(uint32_t timestamp, bool outgoing, int y) {
    char text[24];
    mesh_time_stamp(&time_settings, timestamp, text, sizeof(text));
    char label[40];
    snprintf(label, sizeof(label), "%s %s", outgoing ? "Sent" : "Received", text);
    int width = (int)ceilf(pax_text_size(pax_font_sky_mono, 11, label).x);
    int left = (int)logical_h_res - 32 - width;
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, left, y, label);
    return left;
}

typedef struct { int x, y, font_size; pax_col_t color; } rich_text_context_t;

static int rich_text_advance(int font_size) {
    // Sky Mono's advance is wider than the earlier character-count estimate.
    return (int)ceilf(pax_text_size(pax_font_sky_mono, font_size, "M").x);
}

static void draw_rich_token(const mesh_text_token_t *token, int x, int y, void *context) {
    rich_text_context_t *draw = context;
    x += draw->x; y += draw->y;
    if (token->glyph < 0) {
        pax_draw_text(&fb, draw->color, pax_font_sky_mono, draw->font_size, x, y + 2, token->text);
        return;
    }
    for (unsigned row = 0; row < MESH_EMOJI_SIZE; row++) {
        for (unsigned col = 0; col < MESH_EMOJI_SIZE;) {
            uint8_t alpha = mesh_emoji_alpha(token->glyph, col, row);
            unsigned end = col + 1;
            while (end < MESH_EMOJI_SIZE && mesh_emoji_alpha(token->glyph, end, row) == alpha) end++;
            if (alpha) pax_draw_rect(&fb, ((uint32_t)alpha << 24) | (draw->color & 0xffffff), x + col, y + row, end - col, 1);
            col = end;
        }
    }
}

static int draw_rich_text(const char *text, int x, int y, int font_size, int width, int line_height, int max_lines, pax_col_t color) {
    rich_text_context_t context = {x, y, font_size, color};
    int advance = rich_text_advance(font_size);
    int lines = mesh_text_layout(text, width, advance, line_height, max_lines, draw_rich_token, &context);
    return y + lines * line_height;
}

static void refresh_battery_status(void) {
    bsp_power_battery_information_t information = {0};
    if (bsp_power_get_battery_information(&information) == ESP_OK && information.battery_available) {
        unsigned percentage = (unsigned)(information.remaining_percentage + 0.5);
        if (percentage > 100) percentage = 100;
        snprintf(battery_text, sizeof(battery_text), "%u%%", percentage);
    } else {
        snprintf(battery_text, sizeof(battery_text), "--%%");
    }
}

// Node browser helpers.
static size_t filtered_node_count(time_t now) {
    node_selection = mesh_node_browser_refresh(&node_browser, now, node_selection);
    return node_browser.count;
}
static bool get_filtered_node(size_t index, time_t now, mesh_node_t *out) {
    (void)now; // The list is snapshotted once before rendering or opening Actions.
    if (index >= node_browser.count) return false;
    if (out) *out = node_browser.nodes[index];
    return true;
}
static void begin_node_search(void) {
    snprintf(node_search_before, sizeof(node_search_before), "%s", node_browser.query);
    node_search_selected = node_browser.selected;
    node_search_editing = true;
}
static bool node_browser_navigation(int key) {
    if (current_screen != SCREEN_NODES || node_modal) return false;
    if (node_search_editing) {
        if (key == BSP_INPUT_NAVIGATION_KEY_ESC) {
            snprintf(node_browser.query, sizeof(node_browser.query), "%s", node_search_before);
            node_browser.selected = node_search_selected;
            node_search_editing = false;
            return true;
        }
        if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) { node_search_editing = false; return true; }
        if (key == BSP_INPUT_NAVIGATION_KEY_LEFT || key == BSP_INPUT_NAVIGATION_KEY_RIGHT ||
            key == BSP_INPUT_NAVIGATION_KEY_UP || key == BSP_INPUT_NAVIGATION_KEY_DOWN) return true;
    } else if (key == BSP_INPUT_NAVIGATION_KEY_LEFT || key == BSP_INPUT_NAVIGATION_KEY_RIGHT) {
        node_browser.filter = (node_browser.filter + (key == BSP_INPUT_NAVIGATION_KEY_RIGHT ? 1 : MESH_NODE_FILTER_COUNT-1)) % MESH_NODE_FILTER_COUNT;
        return true;
    }
    if (key == BSP_INPUT_NAVIGATION_KEY_F1 || key == BSP_INPUT_NAVIGATION_KEY_F2 ||
        key == BSP_INPUT_NAVIGATION_KEY_F3 || key == BSP_INPUT_NAVIGATION_KEY_F4) node_search_editing = false;
    return false;
}
static bool node_browser_key(char key) {
    if (node_search_editing) {
        size_t length = strlen(node_browser.query);
        if (key == '\r' || key == '\n' || key == '\t') node_search_editing = false;
        else if ((key == '\b' || key == 0x7f) && length) node_browser.query[length - 1] = '\0';
        else if (key >= 32 && key <= 126 && length + 1 < sizeof(node_browser.query)) {
            node_browser.query[length] = key; node_browser.query[length + 1] = '\0';
        }
        return true;
    }
    if (key == '/') begin_node_search();
    else if (key == '\t') node_browser.sort = (node_browser.sort + 1) % MESH_NODE_SORT_COUNT;
    else if (node_browser.filter==MESH_NODES_NEARBY&&(key=='+'||key=='='||key=='-')) mesh_node_browser_step_radius(&node_browser,key!='-');
    else if (key == 'f' || key == 'F') node_browser.favorites_first = !node_browser.favorites_first;
    else return false;
    return true;
}
// End node browser helpers.

static void format_node_last_seen(uint32_t timestamp, time_t now, char *out, size_t out_size) {
    if (!timestamp || timestamp < 1700000000U || now < 1700000000) {
        snprintf(out, out_size, "unknown");
        return;
    }
    uint64_t age = (uint64_t)now > timestamp ? (uint64_t)now - timestamp : 0;
    if (age < 60) snprintf(out, out_size, "just now");
    else if (age < 3600) snprintf(out, out_size, "%lum ago", (unsigned long)(age / 60));
    else if (age < 86400) snprintf(out, out_size, "%luh %lum ago", (unsigned long)(age / 3600),
                                   (unsigned long)((age % 3600) / 60));
    else snprintf(out, out_size, "%lud ago", (unsigned long)(age / 86400));
}

static void peer_name(uint32_t number, char *out, size_t size) {
    const char *alias=mesh_radio_nickname(number);if(alias[0]){snprintf(out,size,"%s",alias);return;}
    for (size_t i = 0; i < mesh_radio_node_count(); i++) {
        mesh_node_t node;
        if (mesh_radio_get_node(i, &node) && node.number == number) {
            snprintf(out, size, "%s", node.name);
            return;
        }
    }
    snprintf(out, size, "!%08lx", (unsigned long)number);
}

static void open_thread(uint32_t peer, uint8_t channel) {
    draft_ui_touch();
    if(peer)channel=0;
    size_t slot;
    for (slot = 0; slot < draft_count; slot++) if (drafts[slot].peer == peer && drafts[slot].channel == channel) break;
    if (slot == draft_count) {
        if (draft_count == sizeof(drafts) / sizeof(drafts[0])) return;
        drafts[slot].peer = peer;
        drafts[slot].channel = channel;
        draft_hashes[slot]=draft_fingerprint(slot);
        draft_count++;
    }
    if (peer) peer_name(peer, message_peer_name, sizeof(message_peer_name));
    message_peer = peer;
    message_channel = channel;
    message_draft = drafts[slot].text;
    draft_slot = slot;
    snprintf(message_feedback,sizeof(message_feedback),"%s",draft_recovered?"Recovered older drafts. Review before sending.":"");
    message_scroll_offset = mesh_message_open_offset(peer,channel,&message_anchor);
    message_visible_valid=false;
    chat_actions.modal = 0;
    current_screen = SCREEN_MESSAGES;
}

static void open_conversation(uint32_t peer) { open_thread(peer, 0); }

static size_t current_message_count(void) {
    return message_peer ? mesh_radio_conversation_count(message_peer) : mesh_radio_channel_message_count(message_channel);
}

static bool get_current_message(size_t index, mesh_message_t *out) {
    return message_peer ? mesh_radio_get_conversation_message(message_peer, index, out) : mesh_radio_get_channel_message(message_channel, index, out);
}

static mesh_chat_context_t chat_context(void) {
    return (mesh_chat_context_t){
        .peer=message_peer,.channel=message_channel,.draft=message_draft,
        .draft_capacity=MESH_TEXT_MAX_LENGTH+1,.reply_id=&draft_reply_ids[draft_slot],
        .feedback=message_feedback,.feedback_size=sizeof(message_feedback),.scroll=&message_scroll_offset
    };
}
static void open_chat_actions(void) { mesh_chat_actions_open(&chat_actions); }
static bool chat_actions_navigation(int key) {
    if(current_screen!=SCREEN_MESSAGES){chat_actions.modal=CHAT_CLOSED;return false;}
    if(key==BSP_INPUT_NAVIGATION_KEY_F5 || key==BSP_INPUT_NAVIGATION_KEY_F6)return false;
    if(!chat_actions.modal)return false;
    if(key==BSP_INPUT_NAVIGATION_KEY_F1 || key==BSP_INPUT_NAVIGATION_KEY_F2 ||
       key==BSP_INPUT_NAVIGATION_KEY_F3 || key==BSP_INPUT_NAVIGATION_KEY_F4){chat_actions.modal=CHAT_CLOSED;return false;}
    mesh_chat_key_t input=MESH_CHAT_KEY_OTHER;
    switch(key){
        case BSP_INPUT_NAVIGATION_KEY_UP:input=MESH_CHAT_KEY_UP;break;
        case BSP_INPUT_NAVIGATION_KEY_DOWN:input=MESH_CHAT_KEY_DOWN;break;
        case BSP_INPUT_NAVIGATION_KEY_LEFT:input=MESH_CHAT_KEY_LEFT;break;
        case BSP_INPUT_NAVIGATION_KEY_RIGHT:input=MESH_CHAT_KEY_RIGHT;break;
        case BSP_INPUT_NAVIGATION_KEY_RETURN:input=MESH_CHAT_KEY_RETURN;chat_action_enter_us=esp_timer_get_time();break;
        case BSP_INPUT_NAVIGATION_KEY_ESC:input=MESH_CHAT_KEY_ESC;break;
    }
    mesh_chat_context_t context=chat_context();
    mesh_chat_input_result_t result=mesh_chat_actions_input(&chat_actions,&context,input);
    draft_ui_touch();
    if(result==MESH_CHAT_SEARCH)open_message_search();
    if(result==MESH_CHAT_ARCHIVE)open_archive_browser(false,message_peer,message_channel);
    return result!=MESH_CHAT_IGNORED;
}

static bool enabled_channel_at(size_t position, mesh_channel_t *out) {
    for (uint8_t i = 0; i < MESH_CHANNEL_LIMIT; i++) {
        mesh_channel_t channel;
        if (mesh_radio_get_channel(i, &channel) && channel.role && position-- == 0) {
            if (out) *out = channel;
            return true;
        }
    }
    return false;
}

#include "message_search_ui.inc"

static void move_channel_chat(bool down) {
    int direction = down ? 1 : -1;
    for (int i = (int)channel_chat_selection + direction; i >= 0 && i < MESH_CHANNEL_LIMIT; i += direction) {
        mesh_channel_t channel;
        if (mesh_radio_get_channel(i, &channel) && channel.role) { channel_chat_selection = i; break; }
    }
}

static void open_selected_channel(void) {
    mesh_channel_t channel;
    if (mesh_radio_get_channel(channel_chat_selection, &channel) && channel.role) open_thread(0, channel.index);
}

static void open_settings(void) {
    current_screen = SCREEN_SETTINGS_MENU;
    settings_menu_selection = 0;
    settings_names_loaded = false;
    settings_edits_started = false;
    settings_long_edited = false;
    settings_short_edited = false;
    settings_short_name_selected = false;
    settings_save_pending = false;
    settings_feedback[0] = '\0';
    mesh_radio_request_owner();
}

#include "node_nickname_ui.inc"
#include "map_ui.inc"
#include "location_ui.inc"
#include "waypoint_ui.inc"
#include "telemetry_view.inc"

static void activate_home_card(void) {
    if (home_selection == 0) current_screen = SCREEN_CHANNEL_LIST;
    else if (home_selection == 1) { current_screen = SCREEN_DIRECT_MESSAGES; direct_selection = 0; direct_selected_peer = 0; }
    else if (home_selection == 2) { current_screen = SCREEN_NODES; node_modal=0;node_selection = 0; node_browser.selected = 0; node_search_editing = false; }
    else if (home_selection == 3) open_map();
    else if (home_selection == 4) open_settings();
    else if (home_selection == 5) {
        draft_ui_touch();
        int draft_result=draft_ui_flush();
        mesh_radio_range_stop();
        if(!draft_result){home_exit_waiting=true;return;}
        if(draft_result<0&&!draft_exit_discard){draft_exit_discard=true;home_exit_waiting=false;return;}
        if(mesh_radio_setup_busy()||mesh_radio_neighbor_config_busy()||mesh_range_export_busy()||mesh_chat_export_busy()||mesh_tlm_export_busy()||mesh_config_store_busy()||(mesh_radio_config_state()==CFG_SAVING||mesh_radio_role_save().state==MESH_ROLE_SAVING)){home_exit_waiting=true;return;}
        int archive_saved=mesh_archive_flush(esp_timer_get_time());
        if(!archive_saved){home_exit_waiting=true;return;}
        if(archive_saved<0&&!archive_exit_discard){archive_exit_discard=true;home_exit_waiting=false;return;}
        home_exit_failed=!mesh_radio_flush_history()||!mesh_radio_flush_waypoints();if(home_exit_failed)return;
        int telemetry=mesh_tlm_store_flush(esp_timer_get_time());
        if(!telemetry){home_exit_waiting=true;return;}
        if(telemetry<0&&!home_exit_discard){home_exit_discard=true;home_exit_waiting=false;return;}
        home_exit_waiting=false;mesh_radio_flush_node_cache();bsp_device_restart_to_launcher();
    }
}
static bool home_navigation(int key) {
    if(current_screen!=SCREEN_HOME)return false;
    if(key==BSP_INPUT_NAVIGATION_KEY_LEFT){if(home_selection%3)home_selection--;}
    else if(key==BSP_INPUT_NAVIGATION_KEY_RIGHT){if(home_selection%3<2 && home_selection<5)home_selection++;}
    else if(key==BSP_INPUT_NAVIGATION_KEY_UP){if(home_selection>=3)home_selection-=3;}
    else if(key==BSP_INPUT_NAVIGATION_KEY_DOWN){if(home_selection<3)home_selection+=3;}
    else if(key==BSP_INPUT_NAVIGATION_KEY_RETURN)activate_home_card();
    else return false;
    if(home_selection!=5){home_exit_waiting=home_exit_discard=draft_exit_discard=archive_exit_discard=false;}
    return true;
}
static bool home_key(char key) {
    if(current_screen!=SCREEN_HOME)return false;
    if(key=='\t'){home_selection=(home_selection+1)%6;home_exit_waiting=home_exit_discard=draft_exit_discard=archive_exit_discard=false;}
    else if(key=='\r'||key=='\n')activate_home_card();
    else return false;
    return true;
}

static void open_node_actions(void) {
    if (!node_modal) map_action_origin = false;
    if (!node_modal) filtered_node_count(time(NULL));
    if (!node_modal && !get_filtered_node(node_selection, time(NULL), &action_node)) {
        snprintf(node_action_feedback, sizeof(node_action_feedback), "Select a node first.");
        return;
    }
    node_modal = NODE_ACTIONS;
    node_action_selection = 0;
    node_action_feedback[0] = '\0';telemetry_feedback[0]='\0';
}

static void start_node_trace(void) {
    mesh_trace_start_t result = mesh_radio_start_traceroute(action_node.number);
    if (result == MESH_TRACE_START_OK) {
        node_modal = NODE_TRACE;
        trace_return_selected = false;
        trace_scroll = 0;
        node_action_feedback[0] = '\0';
        return;
    }
    const char *message = result == MESH_TRACE_START_BUSY ? "A traceroute is already waiting for a reply." :
        result == MESH_TRACE_START_COOLDOWN ? "Wait 30 seconds between traceroute requests." :
        result == MESH_TRACE_START_INVALID ? "Choose a remote node to trace." :
        result == MESH_TRACE_START_OFFLINE ? "Radio is offline or reconnecting. Retry shortly." :
        "Request not queued. Check radio and retry.";
    snprintf(node_action_feedback, sizeof(node_action_feedback), "%s", message);
}

static void toggle_node_favorite(void) {
    mesh_node_t node;
    if (!mesh_radio_get_node_by_number(action_node.number, &node) || !node.favorite_known) {
        mesh_radio_status_t radio = mesh_radio_status();
        if (radio.synchronized && !radio.reconnecting && mesh_radio_favorite_status().state != MESH_FAVORITE_SAVING) {
            mesh_radio_request_config();
            snprintf(node_action_feedback, sizeof(node_action_feedback), "Refreshing nodes. Retry after radio sync.");
        } else snprintf(node_action_feedback, sizeof(node_action_feedback), "Favorite status unavailable. Wait for radio sync.");
        return;
    }
    mesh_favorite_start_t result = mesh_radio_set_favorite(node.number, !node.is_favorite);
    const char *message = result == MESH_FAVORITE_START_OK ? "" :
        result == MESH_FAVORITE_START_BUSY ? "A favorite change is already being saved." :
        result == MESH_FAVORITE_START_OFFLINE ? "Wait for the radio to finish connecting." :
        result == MESH_FAVORITE_START_NOT_READY ? "Refreshing radio access. Retry shortly." :
        result == MESH_FAVORITE_START_INVALID ? "Choose a known remote node." : "Not saved. Check radio and retry.";
    snprintf(node_action_feedback, sizeof(node_action_feedback), "%s", message);
}

static void request_node_telemetry(void) {
    telemetry_export_visible=false;
    if(action_node.number==mesh_radio_local_node()) {
        snprintf(telemetry_feedback,sizeof(telemetry_feedback),"This is your own radio; reports update automatically.");return;
    }
    mesh_telemetry_request_t current;
    if(mesh_radio_get_telemetry_request(action_node.number,&current) && current.state==MESH_TELEMETRY_WAITING && (current.kind?current.kind:2)==telemetry_kind) {
        telemetry_feedback[0]='\0';return;
    }
    mesh_telemetry_start_t result=mesh_radio_request_telemetry_kind(action_node.number,telemetry_kind);
    const char *feedback=result==MESH_TELEMETRY_START_OK?"":
        result==MESH_TELEMETRY_START_INVALID?"Choose a known remote node to request telemetry.":
        result==MESH_TELEMETRY_START_OFFLINE?"Wait for the radio to finish connecting.":
        result==MESH_TELEMETRY_START_BUSY?"Another telemetry request is still waiting for a reply.":
        result==MESH_TELEMETRY_START_COOLDOWN?"Wait 60 seconds between telemetry requests.":
        "Request was not queued. Check the radio and retry.";
    snprintf(telemetry_feedback,sizeof(telemetry_feedback),"%s",feedback);
}

static bool node_actions_navigation(int key) {
    if (current_screen != SCREEN_NODES) { node_modal = 0; return false; }
    if (!node_modal) {
        if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) { open_node_actions(); return true; }
        return false;
    }
    if (key == BSP_INPUT_NAVIGATION_KEY_F1 || key == BSP_INPUT_NAVIGATION_KEY_F2 ||
        key == BSP_INPUT_NAVIGATION_KEY_F3 || key == BSP_INPUT_NAVIGATION_KEY_F4) { node_modal = 0; return false; }
    if (key == BSP_INPUT_NAVIGATION_KEY_F6) return false;
    if (key == BSP_INPUT_NAVIGATION_KEY_ESC) {
        if (node_modal == NODE_ACTIONS && map_action_origin) {
            node_modal = 0; map_action_origin = false; go_back(); return true;
        }
        node_modal = node_modal != NODE_ACTIONS ? NODE_ACTIONS : 0;
        node_action_feedback[0] = '\0';
        return true;
    }
    if (node_modal == NODE_ACTIONS) {
        if (key == BSP_INPUT_NAVIGATION_KEY_UP && node_action_selection) node_action_selection--;
        else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN && node_action_selection < 13) node_action_selection++;
        else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) {
            if (!node_action_selection) { uint32_t peer = action_node.number; node_modal = 0; open_conversation(peer); }
            else if (node_action_selection == 1) start_node_trace();
            else if (node_action_selection == 2) { node_modal = NODE_TRACE; trace_return_selected = false; trace_scroll = 0; node_action_feedback[0] = '\0'; }
            else if (node_action_selection == 3) toggle_node_favorite();
            else if (node_action_selection == 4) {node_modal=NODE_TELEMETRY;telemetry_view_open(action_node.number,false);}
            else if(node_action_selection==10){open_security_centre(action_node.number);}
            else if(node_action_selection==11){open_remote_manager(action_node.number);}
            else if(node_action_selection==13){open_neighbor_info(action_node.number);}
            else if(node_action_selection==12){open_range_testing(action_node.number==mesh_radio_local_node()?0:action_node.number);}
            else if (node_action_selection == 5) {
                bool mute = !mesh_radio_node_muted(action_node.number);
                bool ok = mesh_radio_set_node_muted(action_node.number, mute);
                snprintf(node_action_feedback, sizeof(node_action_feedback), "%s", !ok ? "Could not save. Alerts unchanged." :
                    mute ? "Muted: counts kept; sound and blue light silenced." : "Unmuted: normal alert settings apply.");
            } else if(node_action_selection==9){
                snprintf(nickname_draft,sizeof(nickname_draft),"%s",mesh_radio_nickname(action_node.number));
                nickname_replace=true;node_modal=NODE_NICKNAME;node_action_feedback[0]=0;
            } else if(node_action_selection==8){
                bool ok=mesh_radio_request_node_info(action_node.number);
                snprintf(node_action_feedback,sizeof(node_action_feedback),"%s",ok?"Name requested; updates when node replies. Replies may be limited.":"Not sent. Wait 60s; check radio/owner info and retry.");
            } else {
                mesh_node_t live;
                bool ignored = mesh_radio_get_node_by_number(action_node.number, &live) && live.is_ignored;
                node_control_kind = node_action_selection == 7 ? MESH_NODE_REMOVE : ignored ? MESH_NODE_UNIGNORE : MESH_NODE_IGNORE;
                node_control_confirm = false; node_modal = NODE_CONTROL_CONFIRM;
            }
        }
    } else if (node_modal == NODE_NICKNAME) {
        if(key==BSP_INPUT_NAVIGATION_KEY_RETURN){
            if(mesh_radio_set_nickname(action_node.number,nickname_draft)){node_modal=NODE_ACTIONS;snprintf(node_action_feedback,sizeof(node_action_feedback),"%s",nickname_draft[0]?"Local nickname saved.":"Nickname cleared; radio name restored.");}
            else snprintf(node_action_feedback,sizeof(node_action_feedback),"Could not save nickname. Check space or text.");
        }
    } else if (node_modal == NODE_CONTROL_CONFIRM) {
        if (key == BSP_INPUT_NAVIGATION_KEY_LEFT || key == BSP_INPUT_NAVIGATION_KEY_RIGHT) node_control_confirm = key == BSP_INPUT_NAVIGATION_KEY_RIGHT;
        else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) {
            if (!node_control_confirm) node_modal = NODE_ACTIONS;
            else {
                mesh_favorite_start_t result = mesh_radio_control_node(action_node.number, node_control_kind);
                if (result == MESH_FAVORITE_START_OK) { node_modal = NODE_ACTIONS; node_action_feedback[0] = 0; }
                else snprintf(node_action_feedback, sizeof(node_action_feedback), "%s", result == MESH_FAVORITE_START_BUSY ? "Another radio change is already pending." :
                    result == MESH_FAVORITE_START_NOT_READY ? "Refreshing radio session. Try again shortly." :
                    result == MESH_FAVORITE_START_OFFLINE ? "Radio not ready. Wait for synchronization." : "Not sent. Node may no longer be available.");
            }
        }
    } else if (node_modal == NODE_TELEMETRY) {
        if(key==BSP_INPUT_NAVIGATION_KEY_RETURN)request_node_telemetry();
        else return telemetry_view_navigation(key);
    } else if (node_modal == NODE_TRACE) {
        if (key == BSP_INPUT_NAVIGATION_KEY_LEFT || key == BSP_INPUT_NAVIGATION_KEY_RIGHT) {
            trace_return_selected = key == BSP_INPUT_NAVIGATION_KEY_RIGHT;
            trace_scroll = 0;
        } else if (key == BSP_INPUT_NAVIGATION_KEY_UP && trace_scroll) trace_scroll--;
        else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) {
            mesh_trace_result_t trace;
            if (mesh_radio_get_traceroute(action_node.number, &trace) && trace.state == MESH_TRACE_COMPLETE) {
                size_t count = (trace_return_selected ? trace.return_count : trace.route_count) + 2;
                size_t visible = ((int)logical_v_res - UI_HINT_OFFSET - 214) / 32;
                if (!visible) visible = 1;
                if (trace_scroll + visible < count) trace_scroll++;
            }
        }
    }
    return true;
}

static void resolve_direct_selection(void) {
    size_t count = direct_ui_count();
    for (size_t i = 0; direct_selected_peer && i < count; i++) {
        uint32_t peer;
        if (direct_ui_peer(i, &peer) && peer == direct_selected_peer) { direct_selection = i; return; }
    }
    if (direct_selection >= count) direct_selection = count ? count - 1 : 0;
    direct_selected_peer = 0;
    direct_ui_peer(direct_selection, &direct_selected_peer);
}

static void open_selected_direct(void) {
    resolve_direct_selection();
    if (direct_selected_peer) open_conversation(direct_selected_peer);
}

static void move_node_selection(bool down) {
    size_t count = filtered_node_count(time(NULL));
    if (!count) { node_selection = 0; node_browser.selected = 0; return; }
    if (node_selection >= count) node_selection = count - 1;
    if (down && node_selection + 1 < count) node_selection++;
    else if (!down && node_selection > 0) node_selection--;
    node_browser.selected = node_browser.nodes[node_selection].number;
}

static const char *channel_role(uint8_t role) {
    return role == 1 ? "Primary" : role == 2 ? "Secondary" : "Disabled";
}

static void open_channel_editor(void) {
    if (!mesh_radio_get_channel(channel_selection, &editing_channel)) {
        mesh_radio_request_channel(channel_selection);
        return;
    }
    snprintf(channel_name, sizeof(channel_name), "%s", editing_channel.name);
    channel_uplink = editing_channel.uplink_enabled;
    channel_downlink = editing_channel.downlink_enabled;
    channel_precision = editing_channel.has_position_precision ? (int)editing_channel.position_precision : -1;
    channel_name_edited = channel_save_pending = false;
    channel_field = 0;
    channel_feedback[0] = '\0';
    current_screen = SCREEN_CHANNEL_EDIT;
    mesh_radio_request_owner(); // Obtain the local admin session key.
}

#include "channel_tools_ui.inc"

static void save_channel(void) {
    if (channel_save_pending) return;
    if (!editing_channel.role) {
        snprintf(channel_feedback, sizeof(channel_feedback), "Use More: Import Channel Link or Change Channel Role, then reopen.");
    } else if (!mesh_radio_admin_ready()) {
        mesh_radio_request_owner();
        snprintf(channel_feedback, sizeof(channel_feedback), "Loading radio permissions; retry save.");
    } else if (mesh_radio_set_channel_location(channel_selection, channel_name, channel_uplink, channel_downlink,
                   channel_precision == (editing_channel.has_position_precision ? (int)editing_channel.position_precision : -1) ? -1 : channel_precision, editing_channel.revision)) {
        channel_save_pending = true;
        channel_save_started = esp_timer_get_time();
        snprintf(channel_feedback, sizeof(channel_feedback), "Saving; waiting for radio confirmation...");
    } else snprintf(channel_feedback, sizeof(channel_feedback), "Not saved; reload channel and retry.");
}

static uint32_t step_radio_option(const radio_option_t *options, size_t count, uint32_t value, bool forward) {
    for (size_t i = 0; i < count; i++) if (options[i].value == value) return options[(i + (forward ? 1 : count - 1)) % count].value;
    return options[forward ? 0 : count - 1].value;
}

static void radio_option_name(const radio_option_t *options, size_t count, uint32_t value, char *out, size_t size) {
    for (size_t i = 0; i < count; i++) if (options[i].value == value) { snprintf(out, size, "%s", options[i].name); return; }
    snprintf(out, size, "Current value: %lu", (unsigned long)value);
}

static void open_radio_settings(void) {
    current_screen = SCREEN_RADIO_SETTINGS;
    radio_field = 0;
    radio_values_loaded = radio_edits_started = radio_save_pending = false;
    radio_feedback[0] = '\0';
    mesh_radio_request_lora();
    mesh_radio_request_owner();
}

static void save_radio_settings(void) {
    if (!radio_values_loaded || radio_save_pending) return;
    if (!mesh_radio_admin_ready()) {
        mesh_radio_request_owner();
        snprintf(radio_feedback, sizeof(radio_feedback), "Loading radio permissions; retry Save.");
    } else if (mesh_radio_set_lora(radio_draft.region, radio_draft.modem_preset, radio_draft.hop_limit, radio_draft.use_preset)) {
        radio_save_pending = true;
        radio_save_started = esp_timer_get_time();
        snprintf(radio_feedback, sizeof(radio_feedback), "Saving; waiting for radio readback...");
    } else snprintf(radio_feedback, sizeof(radio_feedback), "Not saved. Reload radio settings and retry.");
}

static void open_power_settings(void) {
    current_screen = SCREEN_POWER_SETTINGS;
    power_draft = power_settings;
    power_field = 0;
    power_feedback[0] = '\0';
}

static void save_power_settings(void) {
    esp_err_t result = mesh_power_save(&power_draft);
    if (result != ESP_OK) {
        snprintf(power_feedback, sizeof(power_feedback), "Not saved. Please retry.");
        ESP_LOGW(TAG, "Power settings save failed: %s", esp_err_to_name(result));
        return;
    }
    power_settings = power_draft;
    last_activity_us = esp_timer_get_time();
    update_idle_backlights(last_activity_us, false);
    update_message_light();
    snprintf(power_feedback, sizeof(power_feedback), "Saved. Each option works independently.");
}

static esp_err_t apply_display_brightness(uint8_t screen, uint8_t keyboard) {
    return apply_backlight_levels(screen, keyboard);
}

static void discard_display_preview(void) {
    if (!display_preview_active) return;
    if (apply_display_brightness(saved_display_brightness, saved_keyboard_brightness) != ESP_OK) {
        ESP_LOGW(TAG, "Could not restore saved brightness after preview");
    }
    active_display_brightness = saved_display_brightness;
    active_keyboard_brightness = saved_keyboard_brightness;
    display_preview_active = false;
}

static void open_display_settings(void) {
    discard_display_preview();
    current_screen = SCREEN_DISPLAY_ALERTS;
    display_draft = display_settings;
    display_field = 0;
    display_feedback[0] = '\0';
}

static void save_display_settings(void) {
    esp_err_t result = apply_display_brightness(display_draft.screen_brightness, display_draft.keyboard_brightness);
    if (result == ESP_OK) result = mesh_display_save(&display_draft);
    if (result != ESP_OK) {
        apply_display_brightness(saved_display_brightness, saved_keyboard_brightness);
        active_display_brightness = saved_display_brightness;
        active_keyboard_brightness = saved_keyboard_brightness;
        display_preview_active = false;
        snprintf(display_feedback, sizeof(display_feedback), "Not saved. Please retry.");
        ESP_LOGW(TAG, "Display settings save failed: %s", esp_err_to_name(result));
        return;
    }
    display_settings = display_draft;
    saved_display_brightness = active_display_brightness = display_settings.screen_brightness;
    saved_keyboard_brightness = active_keyboard_brightness = display_settings.keyboard_brightness;
    display_preview_active = false;
    last_activity_us = esp_timer_get_time();
    update_message_light();
    snprintf(display_feedback, sizeof(display_feedback), "Saved. Diamond: screen off / wake.");
}

static void step_display_setting(bool forward) {
    uint8_t *value = display_field == 0 ? &display_draft.screen_brightness :
                     display_field == 1 ? &display_draft.keyboard_brightness : &display_draft.night_brightness;
    if (display_field == 0 || display_field == 1 || display_field == 4) {
        int minimum = display_field == 0 ? 10 : display_field == 1 ? 0 : 5;
        int next = *value + (forward ? 5 : -5);
        *value = next < minimum ? minimum : next > 100 ? 100 : next;
    } else if (display_field == 2) {
        const uint16_t options[] = {0, 15, 30, 45, 60, 120, 300, 900};
        const size_t count = sizeof(options) / sizeof(options[0]);
        for (size_t i = 0; i < count; i++) if (display_draft.timeout_seconds == options[i]) {
            display_draft.timeout_seconds = options[(i + (forward ? 1 : count - 1)) % count]; break;
        }
    } else if (display_field == 3) display_draft.unread_light = !display_draft.unread_light;
    else if (display_field == 5) display_draft.alert_sources = 1 + (display_draft.alert_sources - 1 + (forward ? 1 : 2)) % 3;
    snprintf(display_feedback, sizeof(display_feedback), "Unsaved changes. Enter: save.");
    if (display_field < 2) {
        esp_err_t result = apply_display_brightness(display_draft.screen_brightness, display_draft.keyboard_brightness);
        if (result == ESP_OK) {
            active_display_brightness = display_draft.screen_brightness;
            active_keyboard_brightness = display_draft.keyboard_brightness;
            display_preview_active = true;
            snprintf(display_feedback, sizeof(display_feedback), "Preview active. Enter: save. Esc: cancel.");
        } else {
            apply_display_brightness(active_display_brightness, active_keyboard_brightness);
            snprintf(display_feedback, sizeof(display_feedback), "Brightness not confirmed. Please retry.");
            ESP_LOGW(TAG, "Brightness preview failed: %s", esp_err_to_name(result));
        }
    }
}

static void open_manual_channel(void) {
    current_screen = SCREEN_MANUAL_CHANNEL;
    manual_field = 0;
    manual_slot = 1;
    for (uint8_t i = 1; i < MESH_CHANNEL_LIMIT; i++) {
        mesh_channel_t channel;
        if (mesh_radio_get_channel(i, &channel) && !channel.role) { manual_slot = i; break; }
    }
    manual_name[0] = '\0';
    memset(manual_key, 0, sizeof(manual_key));
    manual_key_visible = false;
    manual_phase = 0;
    manual_feedback[0] = '\0';
    mesh_radio_request_channel(manual_slot);
    mesh_radio_request_owner();
}

static void save_manual_channel(void) {
    if (manual_phase) return;
    if (!manual_name[0]) {
        snprintf(manual_feedback, sizeof(manual_feedback), "Enter the existing channel's exact name.");
        manual_field = 1;
    } else if (!mesh_radio_channel_key_valid(manual_key)) {
        snprintf(manual_feedback, sizeof(manual_feedback), "Invalid key. Use Base64 or 32/64 hex digits.");
        manual_field = 2;
    } else {
        mesh_channel_t channel;
        manual_revision = mesh_radio_get_channel(manual_slot, &channel) ? channel.revision : 0;
        manual_phase = 1;
        manual_started = esp_timer_get_time();
        manual_key_visible = false;
        mesh_radio_request_channel(manual_slot);
        mesh_radio_request_owner();
        snprintf(manual_feedback, sizeof(manual_feedback), "Checking that the destination slot is unused...");
    }
}

static void apply_manual_key(char key) {
    if (manual_phase) return;
    if (key == '\t') { manual_field = (manual_field + 1) % 4; manual_key_visible = false; }
    else if (key == '\r' || key == '\n') {
        if (manual_field == 3) save_manual_channel();
        else manual_field++;
        manual_key_visible = false;
    } else if (key == ' ' && manual_field == 2) manual_key_visible = !manual_key_visible;
    else if (manual_field == 1 || manual_field == 2) {
        char *value = manual_field == 1 ? manual_name : manual_key;
        size_t capacity = manual_field == 1 ? sizeof(manual_name) : sizeof(manual_key);
        size_t n = strlen(value);
        if ((key == '\b' || key == 0x7f) && n) value[n - 1] = '\0';
        else if (key >= (manual_field == 2 ? 33 : 32) && key <= 126 && n + 1 < capacity) { value[n] = key; value[n + 1] = '\0'; }
        manual_feedback[0] = '\0';
    }
}

// Additional settings navigation.
static void open_channel_alerts(void) {
    current_screen = SCREEN_CHANNEL_ALERTS; channel_alerts_draft = channel_alerts_mask;
    channel_alert_field = 0; channel_alert_feedback[0] = '\0';
}
static void open_time_settings(void) {
    current_screen = SCREEN_TIME_SETTINGS; time_draft = time_settings;
    time_field = 0; time_feedback[0] = '\0';
}
static void open_role_settings(void) {
    current_screen = SCREEN_ROLE_SETTINGS; role_field = 0; role_loaded = role_edited = false;
    role_feedback[0] = '\0'; mesh_radio_request_device(); mesh_radio_request_metadata(); mesh_radio_request_owner();
}
static void save_role_setting(void) {
    mesh_device_config_t current;
    if (mesh_radio_role_save().state == MESH_ROLE_SAVING) return;
    if (!role_loaded || !mesh_radio_get_device(&current)) {
        mesh_radio_request_device(); snprintf(role_feedback,sizeof(role_feedback),"Loading radio settings. Retry Save shortly.");
    } else if (current.role == role_draft) {
        snprintf(role_feedback,sizeof(role_feedback),"Radio already uses this role.");
    } else if (!mesh_radio_admin_ready()) {
        mesh_radio_request_owner(); snprintf(role_feedback,sizeof(role_feedback),"Loading radio access. Retry Save shortly.");
    } else if (!mesh_radio_role_supported(role_draft)) {
        snprintf(role_feedback,sizeof(role_feedback),"Role support is not confirmed by this firmware.");
    } else if (mesh_radio_set_role(role_draft,role_baseline.revision)) role_feedback[0]='\0';
    else snprintf(role_feedback,sizeof(role_feedback),"Not saved. Reopen this page and retry.");
}
static bool additional_settings_navigation(int key) {
    bool left=key==BSP_INPUT_NAVIGATION_KEY_LEFT, right=key==BSP_INPUT_NAVIGATION_KEY_RIGHT;
    if (current_screen==SCREEN_CHANNEL_ALERTS) {
        if(key==BSP_INPUT_NAVIGATION_KEY_UP && channel_alert_field)channel_alert_field--;
        else if(key==BSP_INPUT_NAVIGATION_KEY_DOWN && channel_alert_field<8)channel_alert_field++;
        else if((left||right) && channel_alert_field<8) {
            channel_alerts_draft^=1u<<channel_alert_field;
            snprintf(channel_alert_feedback,sizeof(channel_alert_feedback),"Unsaved changes. Enter: save.");
        } else if(key==BSP_INPUT_NAVIGATION_KEY_RETURN) {
            if(mesh_channel_alerts_save(channel_alerts_draft)==ESP_OK) {
                channel_alerts_mask=channel_alerts_draft;update_message_light();
                snprintf(channel_alert_feedback,sizeof(channel_alert_feedback),"Channel alerts saved.");
            } else snprintf(channel_alert_feedback,sizeof(channel_alert_feedback),"Could not save. Please retry.");
        } else return false;
        return true;
    }
    if(current_screen==SCREEN_TIME_SETTINGS) {
        if(key==BSP_INPUT_NAVIGATION_KEY_UP && time_field)time_field--;
        else if(key==BSP_INPUT_NAVIGATION_KEY_DOWN && time_field<3)time_field++;
        else if(left||right) {
            if(time_field==0)time_draft.zone=(time_draft.zone+mesh_time_zone_count()+(right?1:-1))%mesh_time_zone_count();
            else if(time_field==1)time_draft.use_24h=!time_draft.use_24h;
            else if(time_field==2) {
                int offset=time_draft.offset_minutes+(right?15:-15);
                if(offset>=-720 && offset<=840)time_draft.offset_minutes=offset;
            } else return false;
            snprintf(time_feedback,sizeof(time_feedback),"Unsaved changes. Enter: save.");
        } else if(key==BSP_INPUT_NAVIGATION_KEY_RETURN) {
            if(mesh_time_save(&time_draft)==ESP_OK) {
                time_settings=time_draft;mesh_time_apply(&time_settings);
                snprintf(time_feedback,sizeof(time_feedback),"Time display settings saved.");
            } else snprintf(time_feedback,sizeof(time_feedback),"Could not save. Please retry.");
        } else return false;
        return true;
    }
    if(current_screen==SCREEN_ROLE_SETTINGS) {
        if(key==BSP_INPUT_NAVIGATION_KEY_RETURN && role_field==1){save_role_setting();return true;}
        if(mesh_radio_role_save().state==MESH_ROLE_SAVING)return key!=BSP_INPUT_NAVIGATION_KEY_ESC && key!=BSP_INPUT_NAVIGATION_KEY_F1 && key!=BSP_INPUT_NAVIGATION_KEY_F2 && key!=BSP_INPUT_NAVIGATION_KEY_F3 && key!=BSP_INPUT_NAVIGATION_KEY_F4 && key!=BSP_INPUT_NAVIGATION_KEY_F6;
        if(key==BSP_INPUT_NAVIGATION_KEY_UP)role_field=0;
        else if(key==BSP_INPUT_NAVIGATION_KEY_DOWN)role_field=1;
        else if((left||right) && role_field==0 && role_loaded) {
            size_t selected=0;
            for(size_t i=0;i<mesh_role_count();i++)if(mesh_role_at(i)==role_draft)selected=i;
            for(size_t i=0;i<mesh_role_count();i++) {
                selected=(selected+mesh_role_count()+(right?1:-1))%mesh_role_count();
                if(mesh_radio_role_supported(mesh_role_at(selected))){role_draft=mesh_role_at(selected);break;}
            }
            role_edited=true;snprintf(role_feedback,sizeof(role_feedback),"Unsaved role selection.");
        } else return false;
        return true;
    }
    return false;
}
// End additional settings navigation.

#include "sound_ui.inc"
#include "telemetry_settings_ui.inc"
#include "key_backup_ui.inc"
#include "config_backup_ui.inc"
#include "units_ui.inc"

#include "radio_behavior_ui.inc"

#include "management_ui.inc"

#include "range_ui.inc"
#include "archive_ui.inc"
#include "neighbor_ui.inc"
#include "first_use_ui.inc"

#include "settings_menu_ui.inc"
#include "settings_navigation.inc"

static void go_home(void) {
    node_modal = 0; map_action_origin = false;
    chat_actions.modal = 0;
    current_screen = SCREEN_HOME;
    screen_history_depth = 0;
    home_exit_waiting=home_exit_discard=draft_exit_discard=archive_exit_discard=false;
    skip_history_capture = true;
}

static void go_back(void) {
    if (!screen_history_depth) { go_home(); return; }
    screen_history_depth--;
    meshdeck_screen_t screen = screen_history[screen_history_depth].screen;
    if (screen == SCREEN_MESSAGES) open_thread(screen_history[screen_history_depth].peer, screen_history[screen_history_depth].channel);
    else current_screen = screen;
    skip_history_capture = true;
}

static const char *conversation_delivery_feedback(pax_col_t *color) {
    *color = UI_MUTED;
    if(draft_revision!=draft_saved_revision&&(draft_load_failed||draft_save_failed)){*color=KEY_TRIANGLE_COLOR;return draft_load_failed?"Saved drafts unavailable. Reopen app to retry recovery.":"Draft not saved. Retrying when storage is available.";}
    if (message_feedback[0]) return message_feedback;
    if(message_scroll_offset)return "Viewing earlier messages. Down: newer messages.";
    if (!message_draft[0]) {
        size_t count = current_message_count();
        for (size_t i = count; i > 0; i--) {
            mesh_message_t message;
            if (!get_current_message(i - 1, &message) || !message.outgoing) continue;
            switch (message.delivery) {
                case MESH_DELIVERY_ACKNOWLEDGED:
                    *color = UI_GREEN;
                    return message_peer ? "Delivered to node." : "Delivered to mesh.";
                case MESH_DELIVERY_PENDING:
                    *color = KEY_TRIANGLE_COLOR;
                    return message_peer ? "Waiting for recipient acknowledgement..." : "Waiting for mesh confirmation...";
                case MESH_DELIVERY_FAILED:
                    *color = RED;
                    return mesh_radio_delivery_text(&message);
                default:
                    break;
            }
            break;
        }
    }
    return "Enter sends; Backspace edits; Up/Down history.";
}

static void mark_visible_thread_read(void) {
    if (current_screen==SCREEN_MESSAGES && !display_and_lights_off && !chat_actions.modal &&
        message_visible_valid && message_visible_peer==message_peer && message_visible_channel==message_channel) {
        mesh_radio_mark_read_range(message_peer,message_channel,message_visible_first,message_visible_end);
    }
}

static void draw_unread_badge(size_t count, int y) {
    if (!count) return;
    char label[24];
    snprintf(label, sizeof(label), "%u unread", (unsigned)count);
    int width = (int)ceilf(pax_text_size(pax_font_sky_mono,12,label).x) + 16;
    int x = (int)logical_h_res - 34 - width;
    pax_draw_rect(&fb, UI_GREEN, x, y - 2, width, 20);
    pax_draw_text(&fb, UI_PANEL, pax_font_sky_mono, 12, x + 8, y, label);
}

static void draw_draft_badge(int y) {
    int x=(int)logical_h_res-104;
    pax_draw_rect(&fb,UI_SELECTED,x,y-2,70,18);
    pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,10,x+10,y+1,"DRAFT");
}

static void draw_reactions(const mesh_message_t *message, int y) {
    size_t count = mesh_radio_reaction_count(message);
    size_t capacity = ((int)logical_h_res - 68) / 96;
    if (!capacity) return;
    size_t visible = count > capacity ? capacity - 1 : count;
    for (size_t i = 0; i < visible; i++) {
        mesh_reaction_t reaction;
        if (!mesh_radio_get_reaction(message, i, &reaction)) continue;
        int x = 32 + i * 96;
        pax_draw_rect(&fb, UI_SELECTED, x, y, 90, 24);
        draw_rich_text(reaction.emoji, x + 4, y + 2, 11, 44, 22, 1, WHITE);
        char sender[8];
        snprintf(sender, sizeof(sender), "%04lx", (unsigned long)(reaction.sender & 0xffff));
        for (size_t n = 0; n < mesh_radio_node_count() && reaction.sender != mesh_radio_local_node(); n++) {
            mesh_node_t node;
            if (mesh_radio_get_node(n, &node) && node.number == reaction.sender && node.short_name[0]) {
                snprintf(sender, sizeof(sender), "%s", node.short_name); break;
            }
        }
        if (reaction.sender == mesh_radio_local_node()) snprintf(sender, sizeof(sender), "You");
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 10, x + 51, y + 8, sender);
    }
    if (visible < count) {
        char more[24]; snprintf(more, sizeof(more), "+%u reactions", (unsigned)(count - visible));
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 32 + visible * 96, y + 8, more);
    }
}

typedef struct {
    rich_text_context_t base;
    char text[256];
    size_t used;
    int run_x, run_y, next_x, caret_x, caret_y;
} compose_paint_t;

static void flush_compose_run(compose_paint_t *paint) {
    if (!paint->used) return;
    paint->text[paint->used] = 0;
    pax_draw_text(&fb, paint->base.color, pax_font_sky_mono, 16,
                  paint->base.x + paint->run_x, paint->base.y + paint->run_y + 2, paint->text);
    paint->caret_x = paint->run_x + (int)ceilf(pax_text_size(pax_font_sky_mono, 16, paint->text).x);
    paint->caret_y = paint->run_y / 22;
    paint->used = 0;
}

static void draw_compose_token(const mesh_text_token_t *token, int x, int y, void *context) {
    compose_paint_t *paint = context;
    if (token->glyph < 0) {
        size_t length = strlen(token->text);
        if (paint->used && (paint->run_y != y || paint->next_x != x || paint->used + length >= sizeof(paint->text))) flush_compose_run(paint);
        if (!paint->used) { paint->run_x = x; paint->run_y = y; }
        memcpy(paint->text + paint->used, token->text, length);
        paint->used += length;
        paint->next_x = x + token->width;
    } else {
        flush_compose_run(paint);
        draw_rich_token(token, x, y, &paint->base);
        paint->caret_x = x + token->width;
        paint->caret_y = y / 22;
    }
}

static int message_compose_height(void) {
    int lines = mesh_text_line_count(message_draft, (int)logical_h_res - 76, rich_text_advance(16));
    return (lines < 3 ? lines : 3) * 22 + 12 + (draft_reply_ids[draft_slot] ? 28 : 0);
}

static void draw_message_composer(int compose_height) {
    int compose_width = (int)logical_h_res - 76;
    int compose_top = (int)logical_v_res - UI_HINT_OFFSET - 8 - compose_height;
    mesh_radio_status_t radio = mesh_radio_status();
    pax_draw_rect(&fb, UI_SELECTED, 20, compose_top, logical_h_res - 40, compose_height);
    pax_draw_rect(&fb, UI_GREEN, 20, compose_top, 3, compose_height);
    int reply_height = draft_reply_ids[draft_slot] ? 28 : 0;
    if (reply_height) {
        mesh_message_t reply = {.peer_node = message_peer, .channel = message_channel, .reply_id = draft_reply_ids[draft_slot]}, parent;
        char quote[320], name[40];
        if (mesh_radio_get_reply_parent(&reply, &parent)) {
            mesh_radio_message_sender_name(&parent, name, sizeof(name));
            snprintf(quote, sizeof(quote), "Reply to %s: %s", name, parent.text);
        } else snprintf(quote, sizeof(quote), "Reply !%08lx (original not retained)", (unsigned long)reply.reply_id);
        draw_rich_text(quote, 34, compose_top + 5, 10, compose_width, 22, 1, UI_MUTED);
        pax_draw_line(&fb, UI_BORDER, 32, compose_top + 25, logical_h_res - 32, compose_top + 25);
    }
    compose_paint_t paint = {.base = {34, compose_top + reply_height + 6, 16, WHITE}};
    mesh_compose_view_t compose = mesh_compose_layout(message_draft, compose_width, rich_text_advance(16), 3, draw_compose_token, &paint);
    flush_compose_run(&paint);
    pax_draw_rect(&fb, UI_GREEN, 34 + paint.caret_x, compose_top + reply_height + 7 + paint.caret_y * 22, 2, 18);
    if (compose.first_line) pax_outline_tri(&fb, UI_MUTED, 27, compose_top + reply_height + 6, 24, compose_top + reply_height + 11, 30, compose_top + reply_height + 11);
    pax_col_t feedback_color;
    const char *feedback = conversation_delivery_feedback(&feedback_color);
    if (radio.reconnecting) {
        feedback = "Radio reconnecting automatically. Draft kept.";
        feedback_color = KEY_TRIANGLE_COLOR;
    }
    if (!radio.frames_received && !message_feedback[0] && !message_draft[0] &&
        current_message_count() == 0) {
        feedback = "Waiting for C6 radio data.";
        feedback_color = KEY_TRIANGLE_COLOR;
    }
    size_t used_bytes = strlen(message_draft);
    size_t remaining = used_bytes < MESH_TEXT_MAX_LENGTH ? MESH_TEXT_MAX_LENGTH - used_bytes : 0;
    char counter[32]; snprintf(counter, sizeof(counter), "%u bytes left", (unsigned)remaining);
    int counter_x = (int)logical_h_res - 20 - (int)ceilf(pax_text_size(pax_font_sky_mono, 10, counter).x);
    draw_rich_text(feedback, 20, (int)logical_v_res - UI_HINT_OFFSET, 10, counter_x - 32, 22, 1, feedback_color);
    pax_draw_text(&fb, remaining ? UI_MUTED : KEY_TRIANGLE_COLOR, pax_font_sky_mono, 10, counter_x,
              (int)logical_v_res - UI_HINT_OFFSET + 2, counter);
}

static void draw_message_header(const mesh_message_t *message, int y, size_t index) {
    char name[40], label[64];
    mesh_radio_message_sender_name(message, name, sizeof(name));
    if (index) snprintf(label, sizeof(label), "%u. %s", (unsigned)index, name);
    else snprintf(label, sizeof(label), "%s:", name);
    int timestamp_left = draw_message_timestamp(message->timestamp, message->outgoing, y);
    int width = timestamp_left - 12 - 32;
    if (width > 0) draw_rich_text(label, 32, y, index ? 11 : 12, width, 16, 1, UI_GREEN);
}

#include "message_search_draw.inc"

#include "chat_actions_draw.inc"

static const char *trace_error_text(uint32_t reason) {
    switch (reason) {
        case 1: return "No route to destination";
        case 2: return "Negative acknowledgement";
        case 3: return "Radio routing timeout";
        case 4: return "No suitable radio interface";
        case 5: return "Radio retries exhausted";
        case 6: return "Channel unavailable";
        case 7: return "Request too large";
        case 8: return "Destination did not respond";
        case 9: return "Radio duty-cycle limit";
        case 32: return "Destination rejected request";
        case 33: return "Request not authorised";
        default: return "Radio reported a routing error";
    }
}

#include "node_actions_draw.inc"

#include "settings_draw.inc"

#include "location_draw.inc"

#include "management_draw.inc"
#include "range_draw.inc"
#include "archive_draw.inc"
#include "neighbor_draw.inc"
#include "first_use_draw.inc"

#include "settings_menu_draw.inc"

#include "radio_behavior_draw.inc"
#include "key_backup_draw.inc"
#include "config_backup_draw.inc"
#include "channel_tools_draw.inc"
#include "waypoint_draw.inc"

#include "home_draw.inc"
#include "home_clock.inc"
#include "packet_runner_ui.inc"

static void draw_screen(void) {
    message_visible_valid=false;
    if (current_screen != SCREEN_ABOUT) packet_secret_progress = 0;
    if(tracked_screen==SCREEN_CONFIG_BACKUP&&current_screen!=SCREEN_CONFIG_BACKUP)cf_close();
    if(tracked_screen==SCREEN_KEY_BACKUP&&current_screen!=SCREEN_KEY_BACKUP)bk_close();
    if (tracked_screen == SCREEN_DISPLAY_ALERTS && current_screen != SCREEN_DISPLAY_ALERTS) discard_display_preview();
    bool changed = current_screen != tracked_screen || (current_screen == SCREEN_MESSAGES && (message_peer != tracked_peer || message_channel != tracked_channel));
    if (changed && !skip_history_capture) {
        if (screen_history_depth == sizeof(screen_history) / sizeof(screen_history[0])) {
            memmove(screen_history, screen_history + 1, (screen_history_depth - 1) * sizeof(screen_history[0]));
            screen_history_depth--;
        }
        screen_history[screen_history_depth].screen = tracked_screen;
        screen_history[screen_history_depth].peer = tracked_peer;
        screen_history[screen_history_depth++].channel = tracked_channel;
    }
    skip_history_capture = false;
    tracked_screen = current_screen;
    tracked_peer = message_peer;
    tracked_channel = message_channel;
    const char* title = "MESHMATSU";
    mesh_radio_status_t radio = mesh_radio_status();
    pax_background(&fb, BLACK);

    if (current_screen == SCREEN_CHANNEL_TOOLS) { title="CHANNEL TOOLS";
    } else if (current_screen == SCREEN_WAYPOINTS) { title="WAYPOINTS";
    } else if (current_screen == SCREEN_MESSAGE_SEARCH) {
        title = "SEARCH MESSAGES";
    } else if (current_screen == SCREEN_SOUND_SETTINGS) {
        title = "SOUNDS & QUIET HOURS";
    } else if (current_screen == SCREEN_TELEMETRY_SETTINGS) {
        title = "TELEMETRY BROADCASTS";
    } else if (current_screen == SCREEN_PACKET_RUNNER) {
        title = "PACKET RUNNER";
    } else if (current_screen == SCREEN_MAP) {
        title = "MAPS";
    } else if (current_screen == SCREEN_MESSAGES) {
        title = chat_actions.modal == CHAT_ACTIONS ? "CHAT ACTIONS" : chat_actions.modal == CHAT_REPLY_TARGETS ? "REPLY TO MESSAGE" : chat_actions.modal == CHAT_TARGETS ? "REACT TO MESSAGE" :
                chat_actions.modal == CHAT_EMOJI ? "CHOOSE EMOJI" :
                chat_actions.modal == CHAT_RETRY_TARGETS ? "RETRY FAILED MESSAGE" : chat_actions.modal == CHAT_RETRY_REVIEW ? "REVIEW RETRY" :
                chat_actions.modal == CHAT_COPY_TARGETS ? "REUSE MESSAGE TEXT" : chat_actions.modal == CHAT_COPY_REVIEW ? "REPLACE DRAFT" :
                chat_actions.modal == CHAT_DELETE_TARGETS ? "DELETE ONE MESSAGE" : chat_actions.modal == CHAT_DELETE_REVIEW ? "CONFIRM DELETE" :
                chat_actions.modal == CHAT_EXPORT_REVIEW || chat_actions.modal == CHAT_EXPORT_STATUS ? "EXPORT CONVERSATION" :
                chat_actions.modal == CHAT_CLEAR_CONFIRM ? "CLEAR CONVERSATION" : message_peer ? "DIRECT CHAT" : "CHANNEL CHAT";
    } else if (current_screen == SCREEN_MANUAL_CHANNEL) {
        title = "ADD CHANNEL";
    } else if (current_screen == SCREEN_DISPLAY_ALERTS) {
        title = "SCREEN & LIGHTS";
    } else if (current_screen == SCREEN_TEXT_SETTINGS) {
        title = "MESSAGE TEXT SIZE";
    } else if (current_screen == SCREEN_CHANNEL_ALERTS) {
        title = "CHANNEL ALERTS";
    } else if (current_screen == SCREEN_TIME_SETTINGS) {
        title = "TIME SETTINGS";
    } else if (current_screen == SCREEN_RADIO_BEHAVIOR) {
        title = "RADIO BEHAVIOR";
    } else if (current_screen == SCREEN_ROLE_SETTINGS) {
        title = "DEVICE ROLE";
    } else if (current_screen == SCREEN_RADIO_ACTIVITY) {
        title = "LOCAL RADIO ACTIVITY";
    } else if (current_screen == SCREEN_ABOUT) {
        title = "ABOUT & DIAGNOSTICS";
    } else if (current_screen == SCREEN_HISTORY_SETTINGS) {
        title = "MESSAGE HISTORY";
    } else if (current_screen == SCREEN_POWER_SETTINGS) {
        title = "BATTERY SAVER";
    } else if (current_screen == SCREEN_RADIO_SETTINGS) {
        title = "RADIO SETTINGS";
    } else if (current_screen == SCREEN_CHANNEL_LIST) {
        title = "CHANNELS";
    } else if (current_screen == SCREEN_DIRECT_MESSAGES) {
        title = "DIRECT MESSAGES";
    } else if (current_screen == SCREEN_POSITION_SETTINGS || current_screen == SCREEN_FIXED_POSITION) {
        title = current_screen == SCREEN_FIXED_POSITION ? "FIXED LOCATION" : "POSITION & LOCATION";
    } else if (current_screen == SCREEN_NODES) {
        title = node_modal == NODE_NICKNAME ? "LOCAL NICKNAME" : node_modal == NODE_CONTROL_CONFIRM ? "CONFIRM NODE CHANGE" : node_modal == NODE_TELEMETRY ? "NODE TELEMETRY" : node_modal == NODE_ACTIONS ? "NODE ACTIONS" : node_modal == NODE_TRACE ? "TRACEROUTE" : "MESH NODES";
    } else if (current_screen == SCREEN_SETTINGS) {
        title = "NODE IDENTITY";
    } else if(current_screen==SCREEN_UNITS){
        title="UNITS";
    } else if(current_screen==SCREEN_CONFIG_BACKUP){
        title="CONFIGURATION BACKUP";
    } else if(current_screen==SCREEN_KEY_BACKUP){
        title="KEY BACKUP & RESTORE";
    } else if (current_screen == SCREEN_NODE_SETTINGS_MENU) {
        title = "NODE & RADIO";
    } else if (current_screen == SCREEN_DISPLAY_SETTINGS_MENU) {
        title = "DISPLAY & ALERTS";
    } else if (current_screen == SCREEN_SETTINGS_MENU) {
        title = "SETTINGS";
    } else if (current_screen == SCREEN_CHANNELS || current_screen == SCREEN_CHANNEL_EDIT || current_screen == SCREEN_CHANNEL_IMPORT) {
        title = "CHANNEL SETTINGS";
    }

    if(current_screen==SCREEN_FIRST_USE)title="FIRST-TIME SETUP";
    if(current_screen==SCREEN_SECURITY)title="SECURITY & CONTACTS";
    if(current_screen==SCREEN_REMOTE)title="REMOTE MANAGEMENT";
    if(current_screen==SCREEN_RANGE_TEST)title="RANGE TEST";
    if(current_screen==SCREEN_NEIGHBOR_INFO)title="NEIGHBOR INFO";
    if(current_screen==SCREEN_NEIGHBOR_SETTINGS)title="NEIGHBOR SETTINGS";
    if(current_screen==SCREEN_ARCHIVE_SETTINGS)title="SD MESSAGE ARCHIVE";
    if(current_screen==SCREEN_ARCHIVE_BROWSER)title="ARCHIVED MESSAGES";
    if (current_screen == SCREEN_HOME) mesh_brand_draw(&fb, 20, 0);
    else pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 24, 20, 18, title);
    const char *radio_text = radio.reconnecting ? "RADIO: RECONNECTING" : radio.synchronized ? "RADIO: ONLINE" :
                             radio.frames_received ? "RADIO: SYNCING" :
                             radio.started ? "RADIO: WAITING" : "RADIO: OFFLINE";
    int radio_status_width = (int)strlen(radio_text) * 8;
    int battery_text_width = (int)strlen(battery_text) * 9;
    int battery_icon_x = (int)logical_h_res - 22 - battery_text_width - 19;
    pax_draw_text(&fb, radio.synchronized ? UI_GREEN : KEY_TRIANGLE_COLOR, pax_font_sky_mono, 12,
                  (int)logical_h_res - 20 - radio_status_width, 42, radio_text);
    pax_outline_rect(&fb, WHITE, battery_icon_x, 22, 14, 9);
    pax_draw_line(&fb, WHITE, battery_icon_x + 15, 24, battery_icon_x + 15, 28);
    pax_draw_text(&fb, WHITE, pax_font_sky_mono, 16, battery_icon_x + 19, 18, battery_text);
    if (current_screen == SCREEN_HOME) {
        draw_home_clock();
        char version[32]; snprintf(version, sizeof(version), "v0.%u.beta", MESH_APP_VERSION);
        float version_width = pax_text_size(pax_font_sky_mono, 11, version).x;
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, logical_h_res - 20 - version_width, 74, version);
    }

    if(current_screen==SCREEN_FIRST_USE){
        draw_first_use();
    } else if(current_screen==SCREEN_NEIGHBOR_INFO){
        draw_neighbor_info();
    } else if(current_screen==SCREEN_NEIGHBOR_SETTINGS){
        draw_neighbor_settings();
    } else if(current_screen==SCREEN_ARCHIVE_SETTINGS){
        draw_archive_settings();
    } else if(current_screen==SCREEN_ARCHIVE_BROWSER){
        draw_archive_browser();
    } else if(current_screen==SCREEN_RANGE_TEST){
        draw_range_test();
    } else if(current_screen==SCREEN_SECURITY){
        draw_security_centre();
    } else if(current_screen==SCREEN_REMOTE){
        draw_remote_manager();
    } else if(current_screen==SCREEN_UNITS){
        draw_units_settings();
    } else if(current_screen==SCREEN_CONFIG_BACKUP){
        draw_config_backup();
    } else if(current_screen==SCREEN_KEY_BACKUP){
        draw_key_backup();
    } else if (current_screen == SCREEN_MESSAGE_SEARCH) {
        draw_message_search();
    } else if (current_screen == SCREEN_SOUND_SETTINGS) {
        draw_sound_settings();
    } else if (current_screen == SCREEN_TELEMETRY_SETTINGS) {
        draw_telemetry_settings();
    } else if (current_screen == SCREEN_PACKET_RUNNER) {
        draw_packet_runner();
    } else if (current_screen == SCREEN_POSITION_SETTINGS || current_screen == SCREEN_FIXED_POSITION) {
        draw_position_settings();
    } else if (current_screen == SCREEN_MAP) {
        draw_map_screen();
        draw_waypoint_markers();
    } else if (current_screen == SCREEN_CHANNEL_TOOLS) {
        draw_channel_tools();
    } else if (current_screen == SCREEN_WAYPOINTS) {
        draw_waypoints();
    } else if (current_screen == SCREEN_HOME) {
        draw_home_dashboard();
    } else if (current_screen == SCREEN_MESSAGES) {
        if (chat_actions.modal) draw_chat_actions();
        else {
        message_scroll_offset=mesh_message_resolve_offset(message_peer,message_channel,message_scroll_offset,&message_anchor);
        size_t count = current_message_count();
        char recipient[80];
        if (message_peer) {
            peer_name(message_peer, message_peer_name, sizeof(message_peer_name));
            snprintf(recipient, sizeof(recipient), "To: %.24s  !%08lx", message_peer_name, (unsigned long)message_peer);
        } else {
            mesh_channel_t channel;
            bool loaded = mesh_radio_get_channel(message_channel, &channel);
            snprintf(recipient, sizeof(recipient), "To: %s / slot %u", loaded && channel.name[0] ? channel.name : "Primary channel", (unsigned)message_channel);
        }
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 12, 20, 64, recipient);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, logical_h_res - 125, 64, "Tab: Actions");
        size_t end = count > message_scroll_offset ? count - message_scroll_offset : 0;
        int message_area_top = 86;
        int compose_height = message_compose_height();
        int compose_top = (int)logical_v_res - UI_HINT_OFFSET - 8 - compose_height;
        int message_area_bottom = compose_top - 6;
        const int body_font_size = mesh_message_font(message_text_size);
        const int body_line_height = mesh_message_line_height(message_text_size);
        const int body_width = (int)logical_h_res - 68;
        const int body_advance = rich_text_advance(body_font_size);
        int available_height = message_area_bottom - message_area_top;
        int used_height = 0;
        size_t first = end;
        while (first > 0) {
            mesh_message_t message;
            if (!get_current_message(first - 1, &message)) break;
            int lines = mesh_text_line_count(message.text, body_width, body_advance);
            int status_height = message.outgoing ? 18 : 0;
            int reaction_height = mesh_radio_reaction_count(&message) ? 26 : 0;
            int row_height = 24 + lines * body_line_height + status_height + reaction_height + (message.reply_id ? 26 : 0) + 10;
            if (used_height + row_height > available_height) {
                if (first == end) first--;
                break;
            }
            used_height += row_height;
            first--;
        }
        if (!count) {
            pax_draw_rect(&fb, UI_PANEL, 20, message_area_top, logical_h_res - 40, 64);
            pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 16, 34, message_area_top + 10, message_peer ? "Start a direct conversation" : "Your channel messages");
            pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 34, message_area_top + 36, "Type below to send your first message.");
        }
        int message_y = message_area_top;
        message_visible_first=first;message_visible_end=first;
        message_visible_peer=message_peer;message_visible_channel=message_channel;message_visible_valid=true;
        for (size_t i = first; i < end; i++) {
            mesh_message_t message;
            if (!get_current_message(i, &message)) continue;
            if (message_y >= message_area_bottom) break;
            int needed_lines = mesh_text_line_count(message.text, body_width, body_advance);
            int status_height = message.outgoing ? 18 : 0;
            int reaction_height = mesh_radio_reaction_count(&message) ? 26 : 0;
            int quote_height = message.reply_id ? 26 : 0;
            int body_lines = (message_area_bottom - message_y - 28 - quote_height - status_height - reaction_height) / body_line_height;
            if (body_lines > needed_lines) body_lines = needed_lines;
            if (body_lines < 1) break;
            int card_height = 24 + quote_height + body_lines * body_line_height + status_height + reaction_height + 4;
            pax_draw_rect(&fb, UI_PANEL, 20, message_y, logical_h_res - 40, card_height);
            pax_col_t delivery_color = message.delivery == MESH_DELIVERY_FAILED ? RED :
                                       message.delivery == MESH_DELIVERY_PENDING ? KEY_TRIANGLE_COLOR : UI_GREEN;
            pax_draw_rect(&fb, delivery_color, 20, message_y, 3, card_height);
            draw_message_header(&message, message_y + 6, 0);
            if (quote_height) {
                mesh_message_t parent; char quote[320], name[40];
                if (mesh_radio_get_reply_parent(&message, &parent)) {
                    mesh_radio_message_sender_name(&parent, name, sizeof(name));
                    snprintf(quote, sizeof(quote), "Reply to %s: %s", name, parent.text);
                } else snprintf(quote, sizeof(quote), "Reply !%08lx (original not retained)", (unsigned long)message.reply_id);
                pax_draw_rect(&fb, UI_MUTED, 32, message_y + 25, 2, 20);
                draw_rich_text(quote, 40, message_y + 27, 10, body_width - 8, 20, 1, UI_MUTED);
            }
            int body_bottom = draw_rich_text(message.text, 32, message_y + 24 + quote_height,
                                             body_font_size, body_width, body_line_height, body_lines, WHITE);
            if (status_height) {
                const char *ack = mesh_radio_delivery_text(&message);
                draw_rich_text(ack, 32, body_bottom + 3, 11, body_width, 18, 1, delivery_color);
            }
            if (reaction_height) draw_reactions(&message, body_bottom + status_height + 2);
            message_visible_end=i+1;
            message_y += card_height + 6;
        }
        draw_message_composer(compose_height);
        displayed_compose_height = compose_height;
        }
    } else if (current_screen == SCREEN_CHANNEL_LIST) {
        // Recover missing channel records without restarting or replacing the config.
        for (uint8_t i = 0; i < MESH_CHANNEL_LIMIT; i++) {
            mesh_channel_t channel;
            if (!mesh_radio_get_channel(i, &channel)) { mesh_radio_request_channel(i); break; }
        }
        size_t count = 0, selected = 0;
        mesh_channel_t channel;
        while (enabled_channel_at(count, &channel)) {
            if (channel.index == channel_chat_selection) selected = count;
            count++;
        }
        mesh_channel_t current;
        if ((!mesh_radio_get_channel(channel_chat_selection, &current) || !current.role) && enabled_channel_at(0, &current)) channel_chat_selection = current.index;
        size_t visible = ((int)logical_v_res - (UI_FOOTER_HEIGHT + 116)) / 56;
        if (!visible) visible = 1;
        size_t first = selected >= visible ? selected - visible + 1 : 0;
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 20, 64, "Choose a channel. Enter opens its messages.");
        if (!count) pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 14, 20, 112, "Waiting for enabled channels from radio...");
        for (size_t i = first; i < count && i < first + visible; i++) {
            if (!enabled_channel_at(i, &channel)) continue;
            int y = 88 + (int)(i - first) * 56;
            pax_draw_rect(&fb, channel.index == channel_chat_selection ? UI_SELECTED : UI_PANEL, 20, y - 6, logical_h_res - 40, 50);
            pax_draw_rect(&fb, channel.index == channel_chat_selection ? UI_GREEN : UI_BORDER, 20, y - 6, 3, 50);
            char label[48];
            snprintf(label, sizeof(label), "%u: %s", (unsigned)channel.index, channel.name[0] ? channel.name : channel.role == 1 ? "Primary channel" : "(preset name)");
            pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 14, 34, y, label);
            draw_unread_badge(mesh_radio_unread_channel(channel.index), y);
            bool has_draft=draft_ui_present(0,channel.index);
            if(has_draft)draw_draft_badge(y+22);
            char details[64];
            snprintf(details, sizeof(details), "%s / %u saved messages", channel_role(channel.role), (unsigned)mesh_radio_channel_message_count(channel.index));
            draw_rich_text(details,34,y+20,11,logical_h_res-(has_draft?160:68),20,1,UI_MUTED);
        }
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, logical_v_res - UI_HINT_OFFSET, "Up/Down: select. Enter: chat. Esc: previous screen.");
    } else if (current_screen == SCREEN_DIRECT_MESSAGES) {
        size_t count = direct_ui_count();
        resolve_direct_selection();
        size_t visible = ((int)logical_v_res - (UI_FOOTER_HEIGHT + 116)) / 56;
        if (!visible) visible = 1;
        size_t first = direct_selection >= visible ? direct_selection - visible + 1 : 0;
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 20, 64, "Choose a conversation; Enter opens it.");
        if (!count) {
            pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 16, 20, 112, "No direct messages yet");
            pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 20, 144, "Circle: Nodes. Select a node and press Enter.");
        }
        for (size_t i = first; i < count && i < first + visible; i++) {
            uint32_t peer;
            if (!direct_ui_peer(i, &peer)) continue;
            int y = 88 + (int)(i - first) * 56;
            pax_draw_rect(&fb, i == direct_selection ? UI_SELECTED : UI_PANEL, 20, y - 6, logical_h_res - 40, 50);
            pax_draw_rect(&fb, i == direct_selection ? UI_GREEN : UI_BORDER, 20, y - 6, 3, 50);
            char name[32];
            peer_name(peer, name, sizeof(name));
            pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 14, 32, y, name);
            draw_unread_badge(mesh_radio_unread_direct(peer), y);
            bool has_draft=draft_ui_present(peer,0);
            if(has_draft)draw_draft_badge(y+24);
            mesh_message_t latest;
            size_t n = mesh_radio_conversation_count(peer);
            if (n && mesh_radio_get_conversation_message(peer, n - 1, &latest)) {
                if (latest.outgoing) pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 32, y + 24, "You:");
                int x = latest.outgoing ? 68 : 32;
                draw_rich_text(latest.text, x, y + 21, 11, (int)logical_h_res - x - (has_draft?126:34), 22, 1, UI_MUTED);
            }else if(has_draft)draw_rich_text("Unsent message",32,y+21,11,logical_h_res-158,22,1,UI_MUTED);
        }
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, logical_v_res - UI_HINT_OFFSET, "Up/Down: select. Enter: chat. Circle: Nodes.");
    } else if (current_screen == SCREEN_NODES) {
        if (node_modal) draw_node_actions();
        else draw_node_browser();
    } else if (current_screen == SCREEN_HISTORY_SETTINGS) {
        draw_history_settings();
    } else if (current_screen == SCREEN_TEXT_SETTINGS) {
        draw_text_settings();
    } else if (current_screen == SCREEN_CHANNEL_ALERTS) {
        draw_channel_alerts();
    } else if (current_screen == SCREEN_TIME_SETTINGS) {
        draw_time_settings();
    } else if(current_screen==SCREEN_RADIO_BEHAVIOR){
        draw_behavior_settings();
    } else if (current_screen == SCREEN_ROLE_SETTINGS) {
        draw_role_settings();
    } else if (current_screen == SCREEN_RADIO_ACTIVITY) {
        draw_telemetry_page(mesh_radio_local_node(),true);
    } else if (current_screen == SCREEN_ABOUT) {
        draw_about();
    } else if (current_screen == SCREEN_POWER_SETTINGS) {
        draw_power_settings();
    } else if (current_screen == SCREEN_DISPLAY_ALERTS) {
        draw_display_settings();
    } else if (current_screen == SCREEN_SETTINGS_MENU || current_screen == SCREEN_NODE_SETTINGS_MENU ||
               current_screen == SCREEN_DISPLAY_SETTINGS_MENU) {
        draw_settings_menu();
    } else if (current_screen == SCREEN_MANUAL_CHANNEL) {
        mesh_channel_t slot;
        bool loaded = mesh_radio_get_channel(manual_slot, &slot);
        mesh_radio_request_channel(manual_slot);
        if (manual_phase == 1 && loaded && slot.revision > manual_revision) {
            if (slot.role) {
                manual_phase = 0;
                snprintf(manual_feedback, sizeof(manual_feedback), "Slot occupied; choose an unused secondary slot.");
            } else if (!mesh_radio_admin_ready()) mesh_radio_request_owner();
            else if (mesh_radio_add_manual_channel(manual_slot, manual_name, manual_key)) {
                manual_revision = slot.revision;
                manual_phase = 2;
                snprintf(manual_feedback, sizeof(manual_feedback), "Saving; verifying channel name and exact key...");
            } else {
                manual_phase = 0;
                snprintf(manual_feedback, sizeof(manual_feedback), "Not saved. Check name, key and radio connection.");
            }
        }
        if (manual_phase == 2 && loaded && slot.revision > manual_revision && mesh_radio_manual_channel_matches(manual_slot, manual_name, manual_key)) {
            manual_phase = 0;
            memset(manual_key, 0, sizeof(manual_key));
            manual_key_visible = false;
            snprintf(manual_feedback, sizeof(manual_feedback), "Channel added; name and key confirmed by radio.");
        }
        if (manual_phase && esp_timer_get_time() - manual_started > 20000000) {
            manual_phase = 0;
            snprintf(manual_feedback, sizeof(manual_feedback), "Not confirmed. Check Channels before retrying.");
        }
        char target[64];
        snprintf(target, sizeof(target), "Slot %u / %s", (unsigned)manual_slot, !loaded ? "reading..." : slot.role ? "OCCUPIED" : "unused secondary");
        const int y[] = {86,142,204,290};
        const int height[] = {48,54,78,40};
        const char *labels[] = {"DESTINATION (LEFT / RIGHT)", "CHANNEL NAME (11 characters)", "CHANNEL KEY", "SAVE TO RADIO"};
        for (int i = 0; i < 4; i++) {
            pax_draw_rect(&fb, manual_field == (size_t)i ? UI_SELECTED : UI_PANEL, 20, y[i], logical_h_res - 40, height[i]);
            pax_draw_rect(&fb, manual_field == (size_t)i ? UI_GREEN : UI_BORDER, 20, y[i], 3, height[i]);
            pax_draw_text(&fb, i == 3 ? UI_GREEN : UI_MUTED, pax_font_sky_mono, i == 3 ? 15 : 11, 34, y[i] + 6, manual_phase && i == 3 ? "WORKING..." : labels[i]);
        }
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 14, 34, 108, target);
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 14, 34, 166, manual_name[0] ? manual_name : "Enter exact channel name");
        if (manual_key_visible && manual_key[0]) draw_wrapped_text(manual_key, "", "", 34, 228, 13, ((int)logical_h_res - 68)/8, 16, 2);
        else {
            char masked[48];
            if (manual_key[0]) snprintf(masked, sizeof(masked), "%u characters (hidden)", (unsigned)strlen(manual_key));
            else snprintf(masked, sizeof(masked), "Enter Base64 or hex key");
            pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 13, 34, 234, masked);
        }
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, 342, "Key: Base64 (AQ== etc.), or 32/64 hex digits.");
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 11, 20, 364, manual_feedback);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, logical_v_res - UI_HINT_OFFSET, "Tab: field. Space: show key. Enter on Save: add.");
    } else if (current_screen == SCREEN_RADIO_SETTINGS) {
        mesh_lora_config_t actual;
        bool loaded = mesh_radio_get_lora(&actual);
        if (!radio_values_loaded && loaded) {
            radio_baseline = radio_draft = actual;
            radio_values_loaded = true;
        }
        if (!radio_values_loaded) mesh_radio_request_lora();
        if (radio_save_pending) {
            if (loaded && actual.revision > radio_baseline.revision && actual.region == radio_draft.region &&
                actual.modem_preset == radio_draft.modem_preset && actual.hop_limit == radio_draft.hop_limit && actual.use_preset == radio_draft.use_preset) {
                radio_baseline = radio_draft = actual;
                radio_save_pending = false;
                radio_edits_started = false;
                snprintf(radio_feedback, sizeof(radio_feedback), "Radio settings confirmed.");
            } else if (esp_timer_get_time() - radio_save_started > 20000000) {
                radio_save_pending = false;
                snprintf(radio_feedback, sizeof(radio_feedback), "Not confirmed. Reopen to check radio values.");
            } else mesh_radio_request_lora();
        } else if (loaded && !radio_edits_started) radio_baseline = radio_draft = actual;
        char values[3][40];
        radio_option_name(region_options, sizeof(region_options)/sizeof(region_options[0]), radio_draft.region, values[0], sizeof(values[0]));
        if (radio_draft.use_preset) radio_option_name(preset_options, sizeof(preset_options)/sizeof(preset_options[0]), radio_draft.modem_preset, values[1], sizeof(values[1]));
        else snprintf(values[1], sizeof(values[1]), "CUSTOM (preserved)");
        snprintf(values[2], sizeof(values[2]), "%u", (unsigned)radio_draft.hop_limit);
        const char *labels[] = {"REGION", "MODEM PRESET", "HOP LIMIT (0-7)"};
        for (int i = 0; i < 3; i++) {
            int y = 86 + i * 60;
            pax_draw_rect(&fb, radio_field == (size_t)i ? UI_SELECTED : UI_PANEL, 20, y, logical_h_res - 40, 52);
            pax_draw_rect(&fb, radio_field == (size_t)i ? UI_GREEN : UI_BORDER, 20, y, 3, 52);
            pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 34, y + 6, labels[i]);
            pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 15, 34, y + 25, radio_values_loaded ? values[i] : "Loading from radio...");
        }
        pax_draw_rect(&fb, radio_field == 3 ? UI_SELECTED : UI_PANEL, 20, 274, logical_h_res - 40, 44);
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 16, 34, 286, radio_save_pending ? "SAVING..." : "SAVE TO RADIO");
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, 330, "Use local region; peers need a matching preset.");
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, 348, "Radio may reconnect while changes apply.");
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 11, 20, 370, radio_feedback);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, logical_v_res - UI_HINT_OFFSET, "Up/Down: field. L/R: value. Enter on Save: apply.");
    } else if (current_screen == SCREEN_CHANNELS) {
        mesh_radio_request_channel(channel_selection);
        size_t visible = ((int)logical_v_res - (UI_FOOTER_HEIGHT + 116)) / 56;
        if (!visible) visible = 1;
        size_t first = channel_selection >= visible ? channel_selection - visible + 1 : 0;
        for (size_t i = first; i < MESH_CHANNEL_LIMIT && i < first + visible; i++) {
            mesh_channel_t channel;
            bool loaded = mesh_radio_get_channel(i, &channel);
            int y = 88 + (int)(i - first) * 56;
            pax_draw_rect(&fb, i == channel_selection ? UI_SELECTED : UI_PANEL, 20, y - 6, logical_h_res - 40, 50);
            pax_draw_rect(&fb, i == channel_selection ? UI_GREEN : UI_BORDER, 20, y - 6, 3, 50);
            char label[64];
            snprintf(label, sizeof(label), "%u: %s", (unsigned)i,
                     !loaded ? "Loading..." : channel.name[0] ? channel.name : channel.role ? "(preset name)" : "Disabled");
            pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 14, 34, y, label);
            pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 34, y + 22,
                          !loaded ? "Select to read from radio" : channel_role(channel.role));
        }
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, logical_v_res - UI_HINT_OFFSET, "Up/Down: select  Enter: configure  I: import link  Esc: Settings");
    } else if (current_screen == SCREEN_CHANNEL_IMPORT) {
        mesh_channel_t shared, slot;
        bool ready = mesh_radio_get_shared_channel(&shared);
        bool loaded = mesh_radio_get_channel(import_slot, &slot);
        mesh_radio_request_channel(import_slot);
        if (import_pending) {
            if (loaded && slot.revision > import_revision && ready && mesh_radio_shared_channel_matches(import_slot)) {
                import_pending = false;
                snprintf(import_feedback, sizeof(import_feedback), "Channel added and confirmed by radio.");
            } else if (esp_timer_get_time() - import_started > 15000000) {
                import_pending = false;
                snprintf(import_feedback, sizeof(import_feedback), "Not confirmed; check Channels before retrying.");
            }
        }
        pax_draw_rect(&fb, UI_PANEL, 20, 94, logical_h_res - 40, 86);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 34, 106, "SHARED CHANNEL");
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 20, 34, 134, ready ? shared.name[0] ? shared.name : "(preset name)" : "No import file loaded");
        char target[64];
        snprintf(target, sizeof(target), "Destination: slot %u / %s", (unsigned)import_slot,
                 !loaded ? "reading..." : slot.role ? "OCCUPIED" : "unused secondary");
        pax_draw_rect(&fb, UI_SELECTED, 20, 198, logical_h_res - 40, 64);
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 14, 34, 220, target);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 20, 282, "Primary channel and radio settings stay unchanged.");
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 11, 20, 318, import_feedback);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, logical_v_res - UI_HINT_OFFSET, "Up/Down: unused slot. Enter: add. Esc: back.");
    } else if (current_screen == SCREEN_CHANNEL_EDIT) {
        if (channel_save_pending) {
            mesh_channel_t actual;
            if (mesh_radio_get_channel(channel_selection, &actual) && actual.revision > editing_channel.revision &&
                strcmp(actual.name, channel_name) == 0 && actual.uplink_enabled == channel_uplink && actual.downlink_enabled == channel_downlink &&
                mesh_radio_location_save().state == MESH_LOCATION_SAVED &&
                (channel_precision < 0 || (actual.has_position_precision && actual.position_precision == (unsigned)channel_precision))) {
                editing_channel = actual;
                channel_save_pending = false;
                snprintf(channel_feedback, sizeof(channel_feedback), "Channel settings confirmed by radio.");
            } else if (esp_timer_get_time() - channel_save_started > 15000000) {
                channel_save_pending = false;
                snprintf(channel_feedback, sizeof(channel_feedback), "Save not confirmed. Reopen to check radio.");
            } else mesh_radio_request_channel(channel_selection);
        }
        char summary[80];
        snprintf(summary, sizeof(summary), "Channel %u / %s / %s", (unsigned)channel_selection, channel_role(editing_channel.role),
                 editing_channel.default_key ? "Default key" : editing_channel.has_key ? "Custom key" : "No channel key");
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 20, 66, summary);
        const char *labels[] = {"CHANNEL NAME (11 characters)", "MQTT UPLINK", "MQTT DOWNLINK", "LOCATION SHARING PRECISION"};
        const char *values[] = {channel_name[0] ? channel_name : "(preset name)", channel_uplink ? "ON" : "OFF", channel_downlink ? "ON" : "OFF", mesh_units_precision(distance_units,channel_precision)};
        for (int i = 0; i < 4; i++) {
            int y = 92 + i * 54;
            pax_draw_rect(&fb, channel_field == (size_t)i ? UI_SELECTED : UI_PANEL, 20, y, logical_h_res - 40, 48);
            pax_draw_rect(&fb, channel_field == (size_t)i ? UI_GREEN : UI_BORDER, 20, y, 3, 48);
            pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 34, y + 7, labels[i]);
            pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 15, 34, y + 28, values[i]);
        }
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, 304, "Precision limits shared detail; it does not improve GPS accuracy.");
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, 322, (!editing_channel.has_key || editing_channel.default_key) ? "Public-key channel: choices limited to approx. 729 m or coarser." : "Location sharing applies only to this channel.");
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 11, 20, 348, channel_feedback);
        pax_draw_rect(&fb,channel_field==4?UI_SELECTED:UI_PANEL,20,376,logical_h_res-40,30);
        pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,12,34,385,"MORE: QR / LINK / KEYS / CHANNEL ROLE");
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, logical_v_res - UI_HINT_OFFSET, "Tab: field  L/R: value  Enter: save/open  Esc: back");
    } else {
        if (!settings_names_loaded) {
            char current_long[40] = {0};
            char current_short[5] = {0};
            if (mesh_radio_get_owner(current_long, sizeof(current_long), current_short, sizeof(current_short))) {
                if (!settings_long_edited) snprintf(settings_long_name, sizeof(settings_long_name), "%s", current_long);
                if (!settings_short_edited) snprintf(settings_short_name, sizeof(settings_short_name), "%s", current_short);
                settings_names_loaded = true;
            } else {
                mesh_radio_request_owner();
            }
        }
        if (settings_save_pending) {
            char current_long[40] = {0};
            char current_short[5] = {0};
            if (mesh_radio_get_owner(current_long, sizeof(current_long), current_short, sizeof(current_short)) &&
                strcmp(current_long, settings_long_name) == 0 &&
                strcmp(current_short, settings_short_name) == 0) {
                snprintf(settings_feedback, sizeof(settings_feedback), "Name saved to radio.");
                settings_save_pending = false;
            }
        }
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 20, 68, "NODE IDENTITY");
        for (int i = 0; i < 2; i++) {
            bool selected = settings_short_name_selected == (i == 1);
            int y = 94 + i * 76;
            pax_draw_rect(&fb, selected ? UI_SELECTED : UI_PANEL, 20, y, logical_h_res - 40, 64);
            pax_draw_rect(&fb, selected ? UI_GREEN : UI_BORDER, 20, y, 3, 64);
            pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 34, y + 9,
                          i == 0 ? "Long name (up to 39 characters)" : "Short name (up to 4 characters)");
            const char *value = i == 0 ? settings_long_name : settings_short_name;
            if (!value[0]) value = settings_names_loaded ? "(empty)" : "Waiting for radio...";
            draw_wrapped_text(value, selected ? "> " : "", "", 34, y + 31, 14,
                              ((int)logical_h_res - 68) / 9, 16, 2);
        }
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, 254, "Up/Down or Tab: select. Type replaces name.");
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, 272, "Enter: save. Backspace: erase.");
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 11, 20, 296,
                      settings_feedback[0] ? settings_feedback :
                      settings_names_loaded ? "Current radio names loaded." : "Waiting for radio admin reply.");
    }

    draw_footer();
    bool painted = blit();
    if (painted) mark_visible_thread_read();
    ui_dirty = !painted;
    update_message_light();
    compose_redraw_pending = false;
    displayed_chat = current_screen == SCREEN_MESSAGES;
    displayed_peer = message_peer;
    displayed_channel = message_channel;
    last_screen_draw_us = esp_timer_get_time();
}

static void queue_compose_redraw(void) {
    int64_t now = esp_timer_get_time();
    if (!compose_redraw_pending) compose_redraw_due_us = now + 20000;
    compose_redraw_pending = true;
    last_text_edit_us = now;
}

static bool compose_refresh_due(int64_t now, bool input_waiting) {
    return compose_redraw_pending && now >= compose_redraw_due_us &&
           (!input_waiting || now - compose_redraw_due_us >= 60000);
}

static void redraw_composer(void) {
    if (display_and_lights_off || current_screen != SCREEN_MESSAGES || chat_actions.modal) { compose_redraw_pending = false; return; }
    int height = message_compose_height();
    if (!displayed_chat || displayed_peer != message_peer || displayed_channel != message_channel || height != displayed_compose_height) {
        draw_screen(); // Growing/shrinking needs the history area laid out again.
        return;
    }
    int top = (int)logical_v_res - UI_HINT_OFFSET - 8 - height;
    pax_draw_rect(&fb, BLACK, 0, top, logical_h_res, (int)logical_v_res - UI_FOOTER_HEIGHT - top);
    draw_message_composer(height);
    blit(); // Keep the proven rotated full-buffer transfer, avoid rebuilding it.
    compose_redraw_pending = false;
}

static void submit_message_draft(void) {
    if (!message_draft[0]) {
        snprintf(message_feedback, sizeof(message_feedback), "Type a message first.");
    } else if (draft_reply_ids[draft_slot] ? mesh_radio_send_reply(message_peer, message_channel, message_draft, draft_reply_ids[draft_slot]) :
               message_peer ? mesh_radio_send_direct(message_peer, message_draft) : mesh_radio_send_channel(message_channel, message_draft)) {
        draft_reply_ids[draft_slot] = 0;
        message_draft[0] = '\0';
        draft_ui_touch();
        message_feedback[0] = '\0'; // Render live delivery state instead of a fixed send notification.
    } else {
        snprintf(message_feedback, sizeof(message_feedback), "Not sent: check radio and channel settings.");
    }
}

static void apply_message_key(char key) {
    size_t length = strlen(message_draft);
    if (key == '\r' || key == '\n') {
        submit_message_draft();
        message_scroll_offset = 0;
    } else if ((key == '\b' || key == 0x7f) && length) {
        mesh_compose_backspace(message_draft);
        message_feedback[0] = '\0';
    } else if (key >= 32 && key <= 126) {
        if (length < MESH_TEXT_MAX_LENGTH) {
            message_draft[length] = key;
            message_draft[length + 1] = '\0';
            message_feedback[0] = '\0';
        } else snprintf(message_feedback, sizeof(message_feedback), "200-byte limit reached.");
    }
    draft_ui_touch();
}

static bool keyboard_owns_delete(int key) {
#if defined(CONFIG_BSP_TARGET_TANMATSU)
    // The BSP emits NAVIGATION then KEYBOARD for one physical Backspace.
    // Use the text event once for all editable fields, including repeats.
    return key == BSP_INPUT_NAVIGATION_KEY_BACKSPACE;
#else
    return false;
#endif
}

static void save_settings_names(void) {
    if (!settings_long_name[0] || !settings_short_name[0]) {
        snprintf(settings_feedback, sizeof(settings_feedback), "Both names are required.");
    } else if (!mesh_radio_admin_ready()) {
        mesh_radio_request_owner();
        snprintf(settings_feedback, sizeof(settings_feedback), "Waiting for radio admin reply; retry save.");
    } else if (mesh_radio_set_owner(settings_long_name, settings_short_name)) {
        snprintf(settings_feedback, sizeof(settings_feedback), "Saving names to radio...");
        settings_save_pending = true;
    } else {
        snprintf(settings_feedback, sizeof(settings_feedback), "Could not update radio name.");
    }
}

void app_main(void) {
    // Start the GPIO interrupt service
    gpio_install_isr_service(0);

    // Initialize the Non Volatile Storage partition
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        res = nvs_flash_erase();
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to erase NVS flash: %d", res);
            return;
        }
        res = nvs_flash_init();
    }
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %d", res);
        return;
    }

    // Initialize the Board Support Package
    const bsp_configuration_t bsp_configuration = {
        .display =
            {
                .requested_color_format = BSP_DISPLAY_COLOR_FORMAT_24_888RGB,
                .num_fbs                = 1,
            },
    };
    res = bsp_device_initialize(&bsp_configuration);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BSP: %d", res);
        return;
    }

    time_settings = mesh_time_defaults();
    mesh_time_load(&time_settings);
    mesh_time_apply(&time_settings);
    mesh_channel_alerts_load(&channel_alerts_mask);
    esp_err_t rtc_err = bsp_rtc_update_time();
    if (rtc_err != ESP_OK) {
        ESP_LOGW(TAG, "Could not load time from the Tanmatsu RTC: %s", esp_err_to_name(rtc_err));
    }

    sound_settings = mesh_sound_defaults();
    mesh_sound_load(&sound_settings);
    esp_err_t sound_result = mesh_sound_init(&sound_settings);
    if (sound_result != ESP_OK) ESP_LOGW(TAG, "Audio unavailable: %s", esp_err_to_name(sound_result));

    bool radio_started = mesh_radio_start();
    mesh_archive_init();
    mesh_radio_set_history_archive(mesh_archive_capture);
    draft_ui_load();
    if (radio_started) {
        vTaskDelay(pdMS_TO_TICKS(500));
        mesh_radio_request_config();
    }

    // Get display parameters and rotation
    res = bsp_display_get_parameters(&display_h_res, &display_v_res, &display_color_format, &display_data_endian);
    if (res != ESP_ERR_NOT_SUPPORTED) {
        if (res != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get display parameters: %d", res);
            return;
        }

        // Convert ESP-IDF color format into PAX buffer type
        pax_buf_type_t format = PAX_BUF_24_888RGB;
        switch (display_color_format) {
            case BSP_DISPLAY_COLOR_FORMAT_1_PAL:
                format = PAX_BUF_1_PAL;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_2_PAL:
                format = PAX_BUF_2_PAL;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_4_PAL:
                format = PAX_BUF_4_PAL;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_8_PAL:
                format = PAX_BUF_8_PAL;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_16_PAL:
                format = PAX_BUF_16_PAL;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_1_GREY:
                format = PAX_BUF_1_GREY;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_2_GREY:
                format = PAX_BUF_2_GREY;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_4_GREY:
                format = PAX_BUF_4_GREY;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_8_GREY:
                format = PAX_BUF_8_GREY;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_8_332RGB:
                format = PAX_BUF_8_332RGB;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_16_565RGB:
                format = PAX_BUF_16_565RGB;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_4_1111ARGB:
                format = PAX_BUF_4_1111ARGB;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_8_2222ARGB:
                format = PAX_BUF_8_2222ARGB;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_16_4444ARGB:
                format = PAX_BUF_16_4444ARGB;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_24_888RGB:
                format = PAX_BUF_24_888RGB;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_32_8888ARGB:
                format = PAX_BUF_32_8888ARGB;
                break;
            case BSP_DISPLAY_COLOR_FORMAT_18_666RGB:
            default:
                ESP_LOGW(TAG, "BSP requests color format not supported by PAX (%u)", format);
                break;
        }

        // Convert BSP display rotation format into PAX orientation type
        bsp_display_rotation_t display_rotation = bsp_display_get_default_rotation();
        pax_orientation_t      orientation      = PAX_O_UPRIGHT;
        switch (display_rotation) {
            case BSP_DISPLAY_ROTATION_90:
                orientation = PAX_O_ROT_CCW;
                break;
            case BSP_DISPLAY_ROTATION_180:
                orientation = PAX_O_ROT_HALF;
                break;
            case BSP_DISPLAY_ROTATION_270:
                orientation = PAX_O_ROT_CW;
                break;
            case BSP_DISPLAY_ROTATION_0:
            default:
                orientation = PAX_O_UPRIGHT;
                break;
        }
        if (display_rotation == BSP_DISPLAY_ROTATION_90 || display_rotation == BSP_DISPLAY_ROTATION_270) {
            logical_h_res = display_v_res;
            logical_v_res = display_h_res;
        } else {
            logical_h_res = display_h_res;
            logical_v_res = display_v_res;
        }

        // Initialize graphics stack
        printf("Initializing framebuffer with w=%d h=%d format=%d endian=%d orientation=%d\n", display_h_res,
               display_v_res, format, display_data_endian, orientation);
        pax_buf_init(&fb, NULL, display_h_res, display_v_res, format);
        pax_buf_reversed(&fb, display_data_endian == BSP_DISPLAY_ENDIAN_BIG);
#if defined(CONFIG_BSP_TARGET_KAMI)
        // Temporary addition for supporting Kami
        fb.palette      = palette;
        fb.palette_size = sizeof(palette) / sizeof(pax_col_t);
#endif
        pax_buf_set_orientation(&fb, orientation);
    } else {
        ESP_LOGI(TAG, "This board has no display support");
    }

    // Get input event queue from BSP
    ESP_ERROR_CHECK(bsp_input_get_queue(&input_event_queue));

    uint8_t brightness = 0;
    if (mesh_backlight_get(false, &brightness) == ESP_OK && brightness > 0) {
        saved_display_brightness = brightness;
    }
    if (bsp_led_get_brightness(&brightness) == ESP_OK && brightness > 0) {
        saved_led_brightness = brightness;
    }
    if (mesh_backlight_get(true, &brightness) == ESP_OK) {
        saved_keyboard_brightness = brightness;
    }

    mesh_units_load_all(&distance_units,&temperature_units);
    mesh_text_size_load(&message_text_size);
    display_settings = mesh_display_defaults(saved_display_brightness, saved_keyboard_brightness);
    esp_err_t display_load_result = mesh_display_load(&display_settings);
    if (display_load_result != ESP_OK && display_load_result != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Using default display settings: %s", esp_err_to_name(display_load_result));
    }
    saved_display_brightness = active_display_brightness = display_settings.screen_brightness;
    saved_keyboard_brightness = active_keyboard_brightness = display_settings.keyboard_brightness;
    apply_backlight_levels(saved_display_brightness, saved_keyboard_brightness);
    power_settings = mesh_power_defaults();
    esp_err_t power_load_result = mesh_power_load(&power_settings);
    if (power_load_result != ESP_OK && power_load_result != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Using default power settings: %s", esp_err_to_name(power_load_result));
    }
    last_activity_us = esp_timer_get_time();
    update_message_light();

    refresh_battery_status();

    bool setup_completed=false;
    mesh_first_use_load(&setup_completed);
    if(!setup_completed){open_first_use(true);tracked_screen=SCREEN_FIRST_USE;}
    draw_screen();

    TickType_t last_battery_refresh = xTaskGetTickCount();
    mesh_radio_status_t last_radio_state = mesh_radio_status();
    uint32_t last_history_revision = mesh_radio_history_revision();
    while (1) {
        mesh_radio_poll();
        bk_tick();
        cf_tick();
        if(draft_ui_poll(esp_timer_get_time(),uxQueueMessagesWaiting(input_event_queue)!=0))ui_dirty=true;
        if(first_use_poll())ui_dirty=true;
        if(neighbor_ui_poll())ui_dirty=true;
        if(archive_ui_poll(esp_timer_get_time()))ui_dirty=true;
        if(mesh_range_export_poll())ui_dirty=true;
        if(mesh_tlm_export_poll())ui_dirty=true;
        if(mesh_chat_export_poll())ui_dirty=true;
        mesh_tlm_store_poll(esp_timer_get_time(),uxQueueMessagesWaiting(input_event_queue)!=0||compose_redraw_pending);
        if(home_exit_waiting){
            if(current_screen!=SCREEN_HOME||home_selection!=5)home_exit_waiting=home_exit_discard=draft_exit_discard=archive_exit_discard=false;
            else if(!mesh_radio_setup_busy()&&!mesh_radio_neighbor_config_busy()&&!mesh_range_export_busy()&&!mesh_draft_store_busy()&&!mesh_chat_export_busy()&&!mesh_tlm_export_busy()&&!mesh_config_store_busy()&&mesh_radio_config_state()!=CFG_SAVING&&mesh_radio_role_save().state!=MESH_ROLE_SAVING){int saved=mesh_tlm_store_flush(esp_timer_get_time());if(saved){home_exit_waiting=false;if(saved<0){home_exit_discard=true;ui_dirty=true;}else activate_home_card();}}
        }
        play_message_alerts();
        mesh_radio_status_t radio_state = mesh_radio_status();
        uint32_t history_revision = mesh_radio_history_revision();
        if (radio_state.frames_received != last_radio_state.frames_received ||
            radio_state.started != last_radio_state.started || radio_state.synchronized != last_radio_state.synchronized ||
            radio_state.reconnecting != last_radio_state.reconnecting || radio_state.node_count != last_radio_state.node_count ||
            history_revision != last_history_revision) ui_dirty = true;
        last_radio_state = radio_state;
        last_history_revision = history_revision;
        update_message_light();
        TickType_t now = xTaskGetTickCount();
        int64_t now_us = esp_timer_get_time();
        bool input_waiting = uxQueueMessagesWaiting(input_event_queue) != 0;
        check_screen_timeout(now_us, input_waiting);
        update_idle_backlights(now_us, input_waiting);
        bool typing_recently = current_screen == SCREEN_MESSAGES && last_text_edit_us && now_us - last_text_edit_us < 250000;
        mesh_radio_poll_history(input_waiting || typing_recently);
        if (!input_waiting && !typing_recently && now - last_battery_refresh >= pdMS_TO_TICKS(power_settings.reduced_background ? 60000 : 10000)) {
            refresh_battery_status();
            ui_dirty = true;
            last_battery_refresh = now;
        }
        // Coalesce text across intervening scancodes/releases, not just adjacent
        // KEYBOARD events. A fixed deadline prevents continuous typing starvation.
        if (compose_refresh_due(now_us, input_waiting)) redraw_composer();
        bool settings_poll = current_screen == SCREEN_NEIGHBOR_INFO || current_screen == SCREEN_NEIGHBOR_SETTINGS || current_screen == SCREEN_RANGE_TEST || current_screen == SCREEN_SECURITY || current_screen == SCREEN_REMOTE || current_screen == SCREEN_CONFIG_BACKUP || current_screen == SCREEN_KEY_BACKUP || current_screen == SCREEN_POSITION_SETTINGS || current_screen == SCREEN_FIXED_POSITION || current_screen == SCREEN_SETTINGS || current_screen == SCREEN_CHANNELS ||
            current_screen == SCREEN_CHANNEL_EDIT || current_screen == SCREEN_CHANNEL_TOOLS || current_screen == SCREEN_WAYPOINTS || current_screen == SCREEN_CHANNEL_IMPORT ||
            current_screen == SCREEN_CHANNEL_LIST || current_screen == SCREEN_RADIO_SETTINGS || current_screen == SCREEN_MANUAL_CHANNEL ||
            current_screen == SCREEN_RADIO_ACTIVITY || current_screen == SCREEN_ABOUT || current_screen == SCREEN_TEXT_SETTINGS || current_screen == SCREEN_HISTORY_SETTINGS ||
            current_screen == SCREEN_RADIO_BEHAVIOR || current_screen == SCREEN_TELEMETRY_SETTINGS || current_screen == SCREEN_TIME_SETTINGS || current_screen == SCREEN_CHANNEL_ALERTS || current_screen == SCREEN_ROLE_SETTINGS ||
            (current_screen == SCREEN_NODES && node_modal);
        int64_t redraw_interval = mesh_power_redraw_interval(&power_settings, ui_dirty, settings_poll);
        bool map_ready = current_screen == SCREEN_MAP && !display_and_lights_off && mesh_map_tiles_needs_draw();
        bool clock_ready = current_screen == SCREEN_HOME && !display_and_lights_off && home_clock_needs_draw();
        if (!display_and_lights_off && !input_waiting && !typing_recently &&
            !compose_redraw_pending && (clock_ready || map_ready || now_us - last_screen_draw_us >= redraw_interval)) {
            draw_screen();
        }
        bsp_input_event_t event;
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(mesh_power_wait_ms(&power_settings, display_and_lights_off, compose_redraw_pending))) == pdTRUE) {
            // While the display and lights are off, consume every input except
            // the F6 / diamond wake key so typing cannot leave the app stuck in this mode.
            if (display_and_lights_off) {
                if (event.type == INPUT_EVENT_TYPE_NAVIGATION && event.args_navigation.state &&
                    event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F6) {
                    toggle_display_and_lights();
                    draw_screen();
                }
                continue;
            }
            if (event.type == INPUT_EVENT_TYPE_KEYBOARD ||
                (event.type == INPUT_EVENT_TYPE_NAVIGATION && event.args_navigation.state)) {
                last_activity_us = esp_timer_get_time();
            }
            if (event.type == INPUT_EVENT_TYPE_NAVIGATION && event.args_navigation.state) {
                if (keyboard_owns_delete(event.args_navigation.key)) continue;
                if(first_use_navigation(event.args_navigation.key)){
                    if(event.args_navigation.key==BSP_INPUT_NAVIGATION_KEY_RETURN)chat_action_enter_us=esp_timer_get_time();
                    draw_screen();continue;
                }
                if (message_search_navigation(event.args_navigation.key)) {
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_RETURN) chat_action_enter_us = esp_timer_get_time();
                    draw_screen(); continue;
                }
                if (packet_navigation(event.args_navigation.key)) {
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_RETURN) chat_action_enter_us = esp_timer_get_time();
                    draw_screen(); continue;
                }
                if (map_navigation(event.args_navigation.key)) {
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_RETURN) chat_action_enter_us = esp_timer_get_time();
                    draw_screen();
                    continue;
                }
                if (home_navigation(event.args_navigation.key)) {
                    if(event.args_navigation.key==BSP_INPUT_NAVIGATION_KEY_RETURN)chat_action_enter_us=esp_timer_get_time();
                    draw_screen();continue;
                }
                if (node_browser_navigation(event.args_navigation.key)) {
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_RETURN) chat_action_enter_us = esp_timer_get_time();
                    draw_screen();
                    continue;
                }
                if (node_actions_navigation(event.args_navigation.key)) {
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_RETURN) chat_action_enter_us = esp_timer_get_time();
                    draw_screen();
                    continue;
                }
                if (chat_actions_navigation(event.args_navigation.key)) {
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_RETURN) chat_action_enter_us = esp_timer_get_time();
                    draw_screen();
                    continue;
                }
                if (channel_tools_navigation(event.args_navigation.key) || waypoint_navigation(event.args_navigation.key)) {
                    if(event.args_navigation.key==BSP_INPUT_NAVIGATION_KEY_RETURN)chat_action_enter_us=esp_timer_get_time();
                    draw_screen();continue;
                }
                if (neighbor_navigation(event.args_navigation.key) || archive_navigation(event.args_navigation.key) || range_navigation(event.args_navigation.key) || management_navigation(event.args_navigation.key) || settings_navigation(event.args_navigation.key)) {
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_RETURN) chat_action_enter_us = esp_timer_get_time();
                    draw_screen();
                    continue;
                }
                if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F1) {
                    go_home();
                    draw_screen();
                } else if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F2) {
                    current_screen = SCREEN_CHANNEL_LIST;
                    draw_screen();
                } else if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F3) {
                    current_screen = SCREEN_DIRECT_MESSAGES;
                    direct_selection = 0;
                    direct_selected_peer = 0;
                    draw_screen();
                } else if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F6) {
                    toggle_display_and_lights();
                } else if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_F4) {
                    current_screen = SCREEN_NODES;
                    node_selection = 0; node_browser.selected = 0; node_search_editing = false;
                    draw_screen();
                } else if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_ESC &&
                           current_screen != SCREEN_HOME) {
                    go_back();
                    message_feedback[0] = '\0';
                    draw_screen();
                } else if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_BACKSPACE &&
                           current_screen == SCREEN_MESSAGES && message_draft[0]) {
                    mesh_compose_backspace(message_draft);draft_ui_touch();
                    message_feedback[0] = '\0';
                    draw_screen();
                } else if (current_screen == SCREEN_CHANNEL_LIST &&
                           (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_UP || event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_DOWN)) {
                    move_channel_chat(event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_DOWN);
                    draw_screen();
                } else if (current_screen == SCREEN_CHANNEL_LIST && event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_RETURN) {
                    open_selected_channel();
                    draw_screen();
                } else if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_RETURN && current_screen == SCREEN_DIRECT_MESSAGES) {
                    open_selected_direct();
                    draw_screen();
                } else if (current_screen == SCREEN_DIRECT_MESSAGES &&
                           (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_UP || event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_DOWN)) {
                    resolve_direct_selection();
                    size_t count = direct_ui_count();
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_UP && direct_selection) direct_selection--;
                    if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_DOWN && direct_selection + 1 < count) direct_selection++;
                    direct_selected_peer = 0;
                    direct_ui_peer(direct_selection, &direct_selected_peer);
                    draw_screen();
                } else if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_RETURN &&
                           current_screen == SCREEN_MESSAGES) {
                    submit_message_draft();
                    message_scroll_offset = 0;
                    draw_screen();
                } else if (event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_RETURN &&
                           current_screen == SCREEN_SETTINGS) {
                    save_settings_names();
                    draw_screen();
                } else if (current_screen == SCREEN_SETTINGS &&
                           event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_UP) {
                    settings_short_name_selected = false;
                    draw_screen();
                } else if (current_screen == SCREEN_SETTINGS &&
                           event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_DOWN) {
                    settings_short_name_selected = true;
                    draw_screen();
                } else if (current_screen == SCREEN_SETTINGS &&
                           event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_BACKSPACE) {
                    char *field = settings_short_name_selected ? settings_short_name : settings_long_name;
                    size_t length = strlen(field);
                    if (length) field[length - 1] = '\0';
                    if (settings_short_name_selected) settings_short_edited = true;
                    else settings_long_edited = true;
                    settings_edits_started = true;
                    settings_feedback[0] = '\0';
                    draw_screen();
                } else if (current_screen == SCREEN_MESSAGES &&
                           event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_UP) {
                    message_scroll_offset=mesh_message_resolve_offset(message_peer,message_channel,message_scroll_offset,&message_anchor);
                    size_t count = current_message_count();
                    size_t max_offset = count ? count - 1 : 0;
                    if (message_scroll_offset < max_offset) message_scroll_offset++;
                    mesh_message_capture_anchor(message_peer,message_channel,message_scroll_offset,&message_anchor);
                    draw_screen();
                } else if (current_screen == SCREEN_MESSAGES &&
                           event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_DOWN) {
                    message_scroll_offset=mesh_message_resolve_offset(message_peer,message_channel,message_scroll_offset,&message_anchor);
                    if (message_scroll_offset > 0) message_scroll_offset--;
                    mesh_message_capture_anchor(message_peer,message_channel,message_scroll_offset,&message_anchor);
                    draw_screen();
                } else if (current_screen == SCREEN_NODES &&
                           event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_UP) {
                    move_node_selection(false);
                    draw_screen();
                } else if (current_screen == SCREEN_NODES &&
                           event.args_navigation.key == BSP_INPUT_NAVIGATION_KEY_DOWN) {
                    move_node_selection(true);
                    draw_screen();
                }
            } else if (event.type == INPUT_EVENT_TYPE_KEYBOARD) {
                char key = event.args_keyboard.ascii;
                bool enter = key == '\r' || key == '\n';
                // Some BSPs emit both NAV Return and its ASCII event. One picker
                // choice must never become a second action or send the draft.
                if (enter && chat_action_enter_us && esp_timer_get_time() - chat_action_enter_us < 150000) continue;
                if (first_use_key(key) || neighbor_key(key) || archive_key(key) || range_key(key) || management_key(key)) {if(enter)chat_action_enter_us=esp_timer_get_time();draw_screen();continue;}
                if (position_key(key)) { if (enter) chat_action_enter_us = esp_timer_get_time(); draw_screen(); continue; }
                if (channel_tools_key(key) || waypoint_key(key)) { if(enter)chat_action_enter_us=esp_timer_get_time();draw_screen();continue; }
                if (map_key(key)) { draw_screen(); continue; }
                if (packet_key(key)) { if (enter) chat_action_enter_us = esp_timer_get_time(); draw_screen(); continue; }
                if (message_search_key(key)) { if (enter) chat_action_enter_us = esp_timer_get_time(); draw_screen(); continue; }
                if (behavior_key(key) || telemetry_view_key(key) || config_backup_key(key) || key_backup_key(key) || sound_key(key) || telemetry_settings_key(key)) { if (enter) chat_action_enter_us = esp_timer_get_time(); draw_screen(); continue; }
                if (home_key(key)) {if(enter)chat_action_enter_us=esp_timer_get_time();draw_screen();continue;}
                if (current_screen == SCREEN_MESSAGES && key == '\t') { open_chat_actions(); draw_screen(); continue; }
                if (current_screen == SCREEN_NODES && !node_modal && node_browser_key(key)) {
                    if (enter) chat_action_enter_us = esp_timer_get_time();
                    draw_screen();
                    continue;
                }
                if(node_nickname_key(key)){draw_screen();continue;}
                if (current_screen == SCREEN_NODES && (node_modal || enter)) {
                    if (enter) node_actions_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
                    draw_screen();
                    continue;
                }
                if (current_screen == SCREEN_MESSAGES && chat_actions.modal) {
                    if (enter) chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
                    draw_screen();
                    continue;
                }
                if (current_screen == SCREEN_MANUAL_CHANNEL) {
                    apply_manual_key(key);
                    // Batch queued text to keep long keys responsive.
                    bsp_input_event_t queued_event;
                    while (xQueuePeek(input_event_queue, &queued_event, 0) == pdTRUE &&
                           queued_event.type == INPUT_EVENT_TYPE_KEYBOARD &&
                           xQueueReceive(input_event_queue, &queued_event, 0) == pdTRUE) apply_manual_key(queued_event.args_keyboard.ascii);
                    draw_screen();
                    continue;
                }
                if(current_screen==SCREEN_UNITS){
                    if(enter)units_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
                    else if(key=='\t')units_field=(units_field+1)%3;
                    else if(key==' ')units_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);
                    draw_screen();continue;
                }
                if (current_screen == SCREEN_CHANNEL_ALERTS || current_screen == SCREEN_TIME_SETTINGS || current_screen == SCREEN_ROLE_SETTINGS) {
                    if(enter) additional_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
                    else if(key=='\t') {
                        if(current_screen==SCREEN_CHANNEL_ALERTS)channel_alert_field=(channel_alert_field+1)%9;
                        else if(current_screen==SCREEN_TIME_SETTINGS)time_field=(time_field+1)%4;
                        else role_field=(role_field+1)%2;
                    } else if(key==' ' && current_screen==SCREEN_CHANNEL_ALERTS)additional_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);
                    draw_screen();continue;
                }
                if (current_screen == SCREEN_HISTORY_SETTINGS) {
                    if (enter) history_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
                    else if (key == '\t') history_field = !history_field;
                    draw_screen(); continue;
                }
                if (current_screen == SCREEN_RADIO_ACTIVITY || current_screen == SCREEN_TEXT_SETTINGS || current_screen == SCREEN_ABOUT) {
                    if (key=='\t' && current_screen==SCREEN_ABOUT)about_action_selection^=1;
                    if (enter) extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
                    draw_screen(); continue;
                }
                if (current_screen == SCREEN_POWER_SETTINGS) {
                    if (key == '\t') power_field = (power_field + 1) % 5;
                    else if (key == '\r' || key == '\n') save_power_settings();
                    draw_screen();
                    continue;
                }
                if (current_screen == SCREEN_DISPLAY_ALERTS) {
                    if (key == '\t') display_field = (display_field + 1) % 7;
                    else if (key == '\r' || key == '\n') save_display_settings();
                    draw_screen();
                    continue;
                }
                if (current_screen == SCREEN_RADIO_SETTINGS) {
                    if (key == '\t') radio_field = (radio_field + 1) % 4;
                    else if ((key == '\r' || key == '\n') && radio_field == 3) save_radio_settings();
                    draw_screen();
                    continue;
                }
                if (current_screen == SCREEN_CHANNEL_EDIT) {
                    if (!channel_save_pending) {
                        if (key == '\t') channel_field = (channel_field + 1) % 5;
                        else if (key == '\r' || key == '\n') save_channel();
                        else if (channel_field == 0 && (key == '\b' || key == 0x7f)) {
                            size_t n = strlen(channel_name);
                            if (n) channel_name[n - 1] = '\0';
                            channel_name_edited = true;
                        } else if (channel_field == 0 && key >= 32 && key <= 126) {
                            if (!channel_name_edited) { channel_name[0] = '\0'; channel_name_edited = true; }
                            size_t n = strlen(channel_name);
                            if (n + 1 < sizeof(channel_name)) { channel_name[n] = key; channel_name[n + 1] = '\0'; }
                            channel_feedback[0] = '\0';
                        } else if (key == ' ' && channel_field == 1) channel_uplink = !channel_uplink;
                        else if (key == ' ' && channel_field == 2) channel_downlink = !channel_downlink;
                    }
                    draw_screen();
                    continue;
                }
                if (current_screen == SCREEN_SETTINGS_MENU || current_screen == SCREEN_NODE_SETTINGS_MENU ||
                    current_screen == SCREEN_DISPLAY_SETTINGS_MENU || current_screen == SCREEN_CHANNELS || current_screen == SCREEN_CHANNEL_IMPORT) continue;
                if (current_screen == SCREEN_MESSAGES) {
                    bool submitted = key == '\r' || key == '\n';
                    apply_message_key(key);
                    // Edit immediately, display once per short input burst.
                    bsp_input_event_t queued_event;
                    while (xQueuePeek(input_event_queue, &queued_event, 0) == pdTRUE &&
                           queued_event.type == INPUT_EVENT_TYPE_KEYBOARD &&
                           xQueueReceive(input_event_queue, &queued_event, 0) == pdTRUE) {
                        char queued_key = queued_event.args_keyboard.ascii;
                        submitted |= queued_key == '\r' || queued_key == '\n';
                        apply_message_key(queued_key);
                    }
                    if (submitted) draw_screen();
                    else queue_compose_redraw();
                    continue;
                }
                if (current_screen == SCREEN_NODES &&
                    (key == 'w' || key == 'W' || key == 's' || key == 'S')) {
                    move_node_selection(key == 's' || key == 'S');
                    draw_screen();
                    continue;
                }
                if (current_screen == SCREEN_SETTINGS) {
                    if (key == '\r' || key == '\n') {
                        save_settings_names();
                    } else if (key == '\t') {
                        settings_short_name_selected = !settings_short_name_selected;
                    } else if ((key == '\b' || key == 0x7f)) {
                        char *field = settings_short_name_selected ? settings_short_name : settings_long_name;
                        size_t length = strlen(field);
                        if (length) field[length - 1] = '\0';
                        if (settings_short_name_selected) settings_short_edited = true;
                        else settings_long_edited = true;
                        settings_edits_started = true;
                        settings_feedback[0] = '\0';
                    } else if (key >= 32 && key <= 126) {
                        char *field = settings_short_name_selected ? settings_short_name : settings_long_name;
                        size_t capacity = settings_short_name_selected ? sizeof(settings_short_name) : sizeof(settings_long_name);
                        bool *edited = settings_short_name_selected ? &settings_short_edited : &settings_long_edited;
                        if (!*edited) {
                            field[0] = '\0';
                            *edited = true;
                        }
                        size_t length = strlen(field);
                        if (length + 1 < capacity) {
                            field[length] = key;
                            field[length + 1] = '\0';
                            settings_edits_started = true;
                        }
                        settings_feedback[0] = '\0';
                    }
                    draw_screen();
                    continue;
                }
                if (key == 'h' || key == 'H') {
                    go_home();
                } else if (key == 'm' || key == 'M') {
                    current_screen = SCREEN_CHANNEL_LIST;
                } else if (key == 'n' || key == 'N') {
                    current_screen = SCREEN_NODES;
                } else {
                    continue;
                }
                draw_screen();
            }
        }
    }
}
