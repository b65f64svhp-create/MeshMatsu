from ui_source import read_main_ui
"""Exercise production search UI with retained history and fake transport."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = read_main_ui(root)

def section(start, end):
    begin = source.index(start)
    return source[begin:source.index(end, begin)]

screen_enum = section('typedef enum {', '} meshdeck_screen_t;') + '} meshdeck_screen_t;'
thread_helpers = section('static void peer_name(', 'static size_t current_message_count(')
keyboard_guard = section('                char key = event.args_keyboard.ascii;',
                         '                if (current_screen == SCREEN_MANUAL_CHANNEL)')
keys = ['UP', 'DOWN', 'LEFT', 'RIGHT', 'RETURN', 'ESC', 'F1', 'F2', 'F3', 'F4', 'F5', 'F6']

harness = '#define CHANNEL_TEST_MAIN channel_tests\n#include "' + str(root / 'tests/test_channel_messaging.c') + '"\n'
harness += '#include "' + str(root / 'main/message_search.c') + '"\n'
harness += '#include "' + str(root / 'main/emoji_text.c') + '"\n'
harness += '#include "' + str(root / 'main/composer_view.c') + '"\n'
harness += '#include "'+str(root/'main/message_navigation.c')+'"\n'
harness += screen_enum + '\n'
harness += ''.join(f'#define BSP_INPUT_NAVIGATION_KEY_{key} {i + 1}\n' for i, key in enumerate(keys))
harness += r'''
static meshdeck_screen_t current_screen = SCREEN_MESSAGES;
static uint32_t message_peer;
static uint8_t message_channel;
static uint32_t draft_reply_ids[188];
static size_t draft_slot;
static char message_peer_name[32], message_feedback[64];
static struct { uint32_t peer; uint8_t channel; char text[MESH_TEXT_MAX_LENGTH + 1]; } drafts[MESH_NODE_LIMIT + 60];
static size_t draft_count = 1, message_scroll_offset;
static char *message_draft = drafts[0].text;
static struct {unsigned modal;} chat_actions;
static unsigned node_modal;
static mesh_message_anchor_t message_anchor;
static bool message_visible_valid,draft_recovered;
static uint32_t draft_hashes[188];
static uint32_t draft_fingerprint(size_t slot){return 0;}
static void draft_ui_touch(void){}
static int64_t chat_action_enter_us;
''' + thread_helpers + '\n#include "' + str(root / 'main/message_search_ui.inc') + '"\n' + r'''
static void open_archive_browser(bool all,uint32_t p,uint8_t c){current_screen=SCREEN_ARCHIVE_BROWSER;}
static bool first_use_key(char key){return false;}
static bool neighbor_key(char key){return false;}
static bool archive_key(char key){return false;}
static bool range_key(char key){return false;}
static bool management_key(char key){return false;}
static bool position_key(char key) { (void)key; return false; }
static bool channel_tools_key(char key){return false;}
static bool waypoint_key(char key){return false;}
static bool map_key(char key) { (void)key; return false; }
static bool packet_key(char key) { (void)key; return false; }
static bool behavior_key(char key){return false;}
static bool sound_key(char key) { (void)key; return false; }
static bool telemetry_settings_key(char key) { (void)key; return false; }
static bool telemetry_view_key(char key){(void)key;return false;}
static bool key_backup_key(char key){return false;}
static bool config_backup_key(char key){return false;}
static bool node_nickname_key(char key){return false;}
static bool home_key(char key) { (void)key; return false; }
static bool node_browser_key(char key) { (void)key; return false; }
static bool node_actions_navigation(int key) { (void)key; return false; }
static bool chat_actions_navigation(int key) { (void)key; return false; }
static void open_chat_actions(void) { chat_actions.modal = 1; }
static unsigned draw_count;
static void draw_screen(void) { draw_count++; }
static bool keyboard_reaches_editor(char ascii) {
    struct { struct { char ascii; } args_keyboard; } event = {{ascii}};
    for (int once = 0; once < 1; once++) {
''' + keyboard_guard + r'''
        return true;
    }
    return false;
}

static const uint32_t north = 0xaabbccdd, south = 0x11223344;
static void add(uint32_t peer, uint8_t channel, uint32_t sender, uint32_t id, const char *text) {
    char from[12]; snprintf(from, sizeof(from), "!%08lx", (unsigned long)sender);
    assert(store_message(from, (const uint8_t *)text, strlen(text), 1800000000 + id, peer, false, channel));
    messages[message_count - 1].packet_id = id;
}
static void setup(void) {
    reset(); fake_time = 1000000;
    memset(drafts, 0, sizeof(drafts)); draft_count = 1; message_draft = drafts[0].text;
    chat_actions.modal = node_modal = 0; chat_action_enter_us = 0; search_count = search_selection = 0;
    search_query[0] = search_feedback[0] = 0; search_all = false;
    message_peer = 0; message_channel = 0; current_screen = SCREEN_MESSAGES;
    nodes[0] = (mesh_node_t){.number = north, .name = "North Radio"};
    nodes[1] = (mesh_node_t){.number = south, .name = "South Radio"}; node_count = 2;
    add(0, 0, north, 10, "Primary meeting");
    add(0, 1, north, 11, "Relay rendezvous");
    add(north, 0, north, 12, "Private alpha");
    add(0, 1, south, 13, "Relay schedule");
    add(south, 0, south, 14, "Private beta");
    add(0, 1, local_node_number, 15, "Own status");
    messages[message_count - 1].outgoing = true; message_unread[message_count - 1] = false;
    open_thread(0, 1); strcpy(message_draft, "Channel draft stays");
}
static void type(const char *text) {
    while (*text) assert(message_search_key(*text++));
}
static void query(const char *text) {
    search_query[0] = 0; refresh_message_search(); type(text);
}
static void select_packet(uint32_t id) {
    for (search_selection = 0; search_selection < search_count; search_selection++)
        if (search_results[search_selection].packet_id == id) return;
    assert(false);
}
static void enter(void) { assert(message_search_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN)); }

int main(void) {
    setup(); size_t writes = write_count, retained = message_count;
    bool unread_before[MESH_MESSAGE_HISTORY_MAX]; memcpy(unread_before, message_unread, sizeof(unread_before));
    mesh_message_t saved[MESH_MESSAGE_HISTORY_MAX]; memcpy(saved, messages, sizeof(saved));
    uint32_t revision = history_revision; bool dirty = history_dirty;
    char *channel_draft = message_draft; chat_actions.modal = 1;
    open_message_search();
    assert(current_screen == SCREEN_MESSAGE_SEARCH && !chat_actions.modal && !search_all);
    assert(!search_peer && search_channel == 1 && search_count == 3 && !search_selection);
    assert(search_results[0].packet_id == 15 && search_results[1].packet_id == 13 && search_results[2].packet_id == 11);
    assert(message_search_navigation(BSP_INPUT_NAVIGATION_KEY_UP) && search_selection == 0);
    for (unsigned i = 0; i < 10; i++) assert(message_search_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN));
    assert(search_selection == 2);
    assert(message_search_navigation(BSP_INPUT_NAVIGATION_KEY_UP) && search_selection == 1);

    // Current radio display names, sender IDs and outgoing names use real resolution.
    query("nOrTh"); assert(search_count == 1 && search_results[0].packet_id == 11);
    assert(message_search_key('\t') && search_all && search_count == 3);
    for (size_t i = 0; i < search_count; i++) assert(search_results[i].packet_id >= 10 && search_results[i].packet_id <= 12);
    assert(message_search_key('\t') && !search_all && search_count == 1);
    query("!AABBCCDD"); assert(search_count == 1 && search_results[0].packet_id == 11);
    query("YOU"); assert(search_count == 1 && search_results[0].outgoing);
    strcpy(nodes[0].name, "Peak Base");
    query("North"); assert(!search_count);
    query("PEAK"); assert(search_count == 1 && search_results[0].packet_id == 11);
    query("relay"); assert(search_count == 2);
    query("does not exist"); assert(!search_count); enter(); assert(current_screen == SCREEN_MESSAGE_SEARCH);
    assert(!strcmp(message_draft, "Channel draft stays") && message_draft == channel_draft);
    assert(write_count == writes && message_count == retained && history_revision == revision && history_dirty == dirty);
    assert(!memcmp(saved, messages, sizeof(saved)) && !memcmp(unread_before, message_unread, sizeof(unread_before)));

    // Results remain frozen when new arrivals would otherwise move every row.
    query(""); select_packet(13); size_t selected = search_selection;
    add(0, 1, north, 16, "New arrival"); add(south, 0, south, 17, "Unrelated arrival");
    assert(search_count == 3 && search_selection == selected && search_results[selected].packet_id == 13);
    messages[3].delivery = MESH_DELIVERY_ACKNOWLEDGED; // Delivery updates do not change identity.
    memcpy(unread_before, message_unread, sizeof(unread_before));
    enter();
    assert(current_screen == SCREEN_MESSAGES && !message_peer && message_channel == 1 && message_scroll_offset == 2);
    assert(message_draft == channel_draft && !strcmp(message_draft, "Channel draft stays"));
    assert(!memcmp(unread_before, message_unread, sizeof(unread_before)) && write_count == writes);

    // All-saved scope opens the selected destination and retains each thread draft.
    open_thread(north, 0); strcpy(message_draft, "Private draft stays"); char *private_draft = message_draft;
    open_thread(0, 1); open_message_search(); message_search_key('\t');
    select_packet(12); enter();
    assert(current_screen == SCREEN_MESSAGES && message_peer == north && !message_channel && !message_scroll_offset);
    assert(message_draft == private_draft && !strcmp(message_draft, "Private draft stays"));
    open_message_search(); assert(!search_all && search_peer == north && search_count == 1);
    message_search_key('\t'); select_packet(10); assert(message_search_key('\r'));
    assert(current_screen == SCREEN_MESSAGES && !message_peer && !message_channel && !message_scroll_offset);
    open_thread(0, 1); assert(!strcmp(message_draft, "Channel draft stays"));

    // Evicting a snapshot never opens whichever message inherited its old index.
    open_message_search(); select_packet(13);
    for (size_t i = 0; i < message_count; i++) if (messages[i].packet_id == 13) { remove_history_message(i); break; }
    add(0, 1, south, 18, "Replacement row"); retained = message_count;
    memcpy(unread_before, message_unread, sizeof(unread_before));
    enter();
    assert(current_screen == SCREEN_MESSAGE_SEARCH && strstr(search_feedback, "no longer retained"));
    assert(search_results[search_selection].packet_id == 13 && message_count == retained && write_count == writes);
    assert(message_draft == channel_draft && !strcmp(message_draft, "Channel draft stays"));
    assert(!memcmp(unread_before, message_unread, sizeof(unread_before)));

    // UTF-8 deletion calls the production composer helper and removes one token.
    query("A📡é"); assert(!strcmp(search_query, "A📡é"));
    message_search_key('\b'); assert(!strcmp(search_query, "A📡"));
    message_search_key(0x7f); assert(!strcmp(search_query, "A"));
    message_search_key('\b'); message_search_key('\b'); assert(!search_query[0]);
    for (size_t i = 0; i < MESH_MESSAGE_SEARCH_QUERY_MAX; i++) message_search_key('x');
    assert(strlen(search_query) == MESH_MESSAGE_SEARCH_QUERY_MAX);
    message_search_key('z'); assert(strlen(search_query) == MESH_MESSAGE_SEARCH_QUERY_MAX && strstr(search_feedback, "48"));
    message_search_key('\b'); message_search_key('z'); assert(strlen(search_query) == MESH_MESSAGE_SEARCH_QUERY_MAX && search_query[MESH_MESSAGE_SEARCH_QUERY_MAX - 1] == 'z');

    // Escape and shape shortcuts fall through to the global screen navigation.
    assert(!message_search_navigation(BSP_INPUT_NAVIGATION_KEY_ESC));
    for (int key = BSP_INPUT_NAVIGATION_KEY_F1; key <= BSP_INPUT_NAVIGATION_KEY_F6; key++)
        assert(!message_search_navigation(key));
    assert(!message_search_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT));
    assert(!message_search_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT));
    current_screen = SCREEN_HOME;
    assert(!message_search_navigation(BSP_INPUT_NAVIGATION_KEY_UP) && !message_search_key('x'));

    // A physical Return plus the duplicate ASCII event cannot reach the composer.
    open_thread(0, 1); open_message_search();
    assert(message_search_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN)); chat_action_enter_us = fake_time;
    assert(current_screen == SCREEN_MESSAGES && !keyboard_reaches_editor('\r') && !keyboard_reaches_editor('\n'));
    fake_time += 200000; open_message_search();
    assert(!keyboard_reaches_editor('\r') && current_screen == SCREEN_MESSAGES);
    assert(!keyboard_reaches_editor('\n'));
    fake_time += 200000; assert(keyboard_reaches_editor('a'));
    assert(write_count == writes && !strcmp(channel_draft, "Channel draft stays") && !strcmp(private_draft, "Private draft stays"));

    puts("PASS: actual search UI snapshots, thread/all scope, live sender matching, destination/offset, eviction safety, draft/unread/radio isolation, UTF-8 deletion, query bounds, global shortcuts and duplicate Return guard");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='meshmatsu-message-search-ui-') as folder:
    path = Path(folder)
    (path / 'test.c').write_text(harness)
    env = dict(os.environ, DEVELOPER_DIR='/Library/Developer/CommandLineTools')
    subprocess.run(['clang', '-std=c11', '-fsanitize=address,undefined',
                    '-I', str(root / 'tests/stubs'), '-I', str(root / 'main'),
                    str(path / 'test.c'), '-lm', '-o', str(path / 'test')], check=True, env=env)
    subprocess.run([str(path / 'test')], cwd=folder, check=True, env=env)
