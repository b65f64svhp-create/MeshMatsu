from ui_source import read_main_ui
"""Host-check the actual Home/back stack functions without display hardware."""
from pathlib import Path
import subprocess
import tempfile
source = read_main_ui(Path(__file__).resolve().parents[1])
a = source.index('typedef enum {')
b = source.index('} meshdeck_screen_t;', a) + len('} meshdeck_screen_t;')
enum = source[a:b]
a = source.index('static struct { meshdeck_screen_t screen;')
b = source.index('static bool skip_history_capture;', a) + len('static bool skip_history_capture;')
globals_ = source[a:b]
a = source.index('static void go_home(void) {')
b = source.index('static const char *conversation_delivery_feedback', a)
functions = source[a:b]
a = source.index('static void draw_screen(void) {') + len('static void draw_screen(void) {')
b = source.index('    const char* title', a)
capture = source[a:b]
a = source.index('static void activate_home_card(void)')
b = source.index('static void open_node_actions', a)
activate = source[a:b]
a = source.index('static void open_thread(')
b = source.index('static size_t current_message_count', a)
threads = source[a:b]
a = source.index('static mesh_draft_t drafts')
b = source.index('static char *message_draft', a)
b = source.index(';', b) + 1
drafts = source[a:b]
a = source.index('static void mark_visible_thread_read(void)')
b = source.index('static void draw_unread_badge', a)
read_visibility = source[a:b]
harness = '#include "'+str(Path(__file__).resolve().parents[1]/'main/draft_store.h')+'"\n'+'''#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
'''+enum+'''
static meshdeck_screen_t current_screen = SCREEN_HOME;
static unsigned packet_secret_progress;
static uint32_t message_peer;
static uint8_t message_channel;
static uint32_t draft_reply_ids[188];
static size_t draft_slot;
static char message_peer_name[32], message_feedback[64];
static size_t message_scroll_offset;
#define MESH_TEXT_MAX_LENGTH 200
#define MESH_NODE_LIMIT 128
#define BSP_INPUT_NAVIGATION_KEY_UP 1
#define BSP_INPUT_NAVIGATION_KEY_DOWN 2
#define BSP_INPUT_NAVIGATION_KEY_LEFT 3
#define BSP_INPUT_NAVIGATION_KEY_RIGHT 4
#define BSP_INPUT_NAVIGATION_KEY_RETURN 5
static size_t home_selection, node_selection, direct_selection;
static struct { uint32_t selected; } node_browser;
static bool node_search_editing, home_exit_failed, home_exit_waiting, home_exit_discard;
static bool archive_exit_discard;
static bool setup_saving;
static bool mesh_radio_setup_busy(void){return setup_saving;}
static bool neighbor_saving;
static bool mesh_radio_neighbor_config_busy(void){return neighbor_saving;}
static int archive_result=1;
static int mesh_archive_flush(int64_t now){return archive_result;}
static bool export_busy,draft_exit_discard,draft_recovered;
static uint32_t draft_hashes[188];
static uint32_t draft_fingerprint(size_t slot){return 0;}
static void draft_ui_touch(void){}
static int draft_result=1;
static int draft_ui_flush(void){return draft_result;}
static struct {bool valid;} message_anchor;
static size_t mesh_message_open_offset(uint32_t p,uint8_t c,void *a){return 0;}
static bool message_visible_valid;
static uint32_t message_visible_peer;
static uint8_t message_visible_channel;
static size_t message_visible_first,message_visible_end;
#define CFG_SAVING 3
static bool config_busy;
static bool mesh_config_store_busy(void){return config_busy;}
static int mesh_radio_config_state(void){return 0;}
static bool mesh_tlm_export_busy(void){return export_busy;}
static bool chat_export_busy;
static void mesh_radio_range_stop(void){}
static bool mesh_range_export_busy(void){return false;}
static bool mesh_chat_export_busy(void){return chat_export_busy;}
enum {MESH_ROLE_SAVING=1};
static struct {int state;} role_save;
#define mesh_radio_role_save() (role_save)
static int telemetry_flush_result=1;
static int64_t esp_timer_get_time(void){return 1000000;}
static int mesh_tlm_store_flush(int64_t now){return telemetry_flush_result;}
static bool history_flush_ok=true;
static bool mesh_radio_flush_waypoints(void){return true;}
static bool mesh_radio_flush_history(void){return history_flush_ok;}
static uint32_t direct_selected_peer;
static struct {unsigned modal;} chat_actions;
static unsigned node_modal;
static bool map_action_origin;
static bool exit_called;
static void bk_close(void){}
static void cf_close(void){}
static void discard_display_preview(void) {}
static bool display_and_lights_off;
static unsigned mark_read_calls;
static uint32_t marked_peer;
static uint8_t marked_channel;
static void mesh_radio_mark_read_range(uint32_t peer, uint8_t channel,size_t first,size_t end) { mark_read_calls++; marked_peer = peer; marked_channel = channel; }
static void peer_name(uint32_t peer, char *out, size_t size) { snprintf(out, size, "%u", peer); }
static void open_map(void) { current_screen = SCREEN_MAP; }
static void open_settings(void) { current_screen = SCREEN_SETTINGS_MENU; }
static void mesh_radio_flush_node_cache(void) {}
static void bsp_device_restart_to_launcher(void) { exit_called = true; }
'''+drafts+threads+globals_+functions+activate+read_visibility+'\nstatic void capture(void) {'+capture+'''}
int main(void) {
    capture();
    current_screen = SCREEN_SETTINGS_MENU; capture();
    current_screen = SCREEN_CHANNELS; capture();
    current_screen = SCREEN_CHANNEL_EDIT; capture();
    go_back(); capture(); assert(current_screen == SCREEN_CHANNELS);
    go_back(); capture(); assert(current_screen == SCREEN_SETTINGS_MENU);
    go_back(); capture(); assert(current_screen == SCREEN_HOME);
    current_screen = SCREEN_SETTINGS_MENU; capture();
    current_screen = SCREEN_NODE_SETTINGS_MENU; capture(); current_screen = SCREEN_ROLE_SETTINGS; capture();
    go_back(); capture(); assert(current_screen == SCREEN_NODE_SETTINGS_MENU);
    go_back(); capture(); assert(current_screen == SCREEN_SETTINGS_MENU);
    current_screen = SCREEN_DISPLAY_SETTINGS_MENU; capture(); current_screen = SCREEN_DISPLAY_ALERTS; capture();
    go_back(); capture(); assert(current_screen == SCREEN_DISPLAY_SETTINGS_MENU);
    go_back(); capture(); assert(current_screen == SCREEN_SETTINGS_MENU);
    go_home(); capture();
    current_screen = SCREEN_NODES; capture();
    open_conversation(123); capture();
    go_back(); capture(); assert(current_screen == SCREEN_NODES);
    open_conversation(456); capture();
    go_home(); capture(); assert(current_screen == SCREEN_HOME && screen_history_depth == 0 && !exit_called);
    go_back(); capture(); assert(current_screen == SCREEN_HOME);
    open_thread(0, 0); strcpy(message_draft, "primary draft"); draft_reply_ids[draft_slot]=777; capture();
    open_thread(0, 1); assert(message_draft[0] == 0 && !draft_reply_ids[draft_slot]); strcpy(message_draft, "secondary draft"); capture();
    open_conversation(123); assert(message_draft[0] == 0); strcpy(message_draft, "direct draft"); capture();
    go_back(); capture(); assert(message_channel == 1 && message_peer == 0 && strcmp(message_draft, "secondary draft") == 0);
    go_back(); capture(); assert(message_channel == 0 && message_peer == 0 && strcmp(message_draft, "primary draft") == 0);
    open_conversation(123); assert(strcmp(message_draft, "direct draft") == 0);
    go_home(); capture();
    home_selection = 4; activate_home_card(); assert(current_screen == SCREEN_SETTINGS_MENU && !exit_called);
    go_home(); capture(); home_selection = 5;
    activate_home_card(); assert(exit_called);
    exit_called=false;history_flush_ok=false;home_selection=5;activate_home_card();assert(!exit_called&&home_exit_failed);
    history_flush_ok=true;activate_home_card();assert(exit_called&&!home_exit_failed);
    exit_called=false;go_home();home_selection=5;export_busy=true;activate_home_card();assert(home_exit_waiting&&!exit_called);export_busy=false;
    exit_called=false;go_home();home_selection=5;telemetry_flush_result=0;
    activate_home_card();assert(home_exit_waiting&&!exit_called);
    home_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);assert(!home_exit_waiting&&!home_exit_discard);
    home_selection=5;telemetry_flush_result=-1;activate_home_card();assert(!exit_called&&home_exit_discard);
    activate_home_card();assert(exit_called);exit_called=false;go_home();assert(!home_exit_discard&&!home_exit_waiting);
    telemetry_flush_result=1;
    chat_export_busy=true;go_home();home_selection=5;activate_home_card();assert(home_exit_waiting&&!exit_called);chat_export_busy=false;
    role_save.state=MESH_ROLE_SAVING;go_home();home_selection=5;activate_home_card();assert(home_exit_waiting&&!exit_called);role_save.state=0;
    draft_result=0;go_home();home_selection=5;activate_home_card();assert(home_exit_waiting&&!exit_called);
    draft_result=-1;activate_home_card();assert(draft_exit_discard&&!home_exit_waiting&&!exit_called);
    activate_home_card();assert(exit_called);exit_called=false;draft_result=1;
    go_home();home_selection=5;archive_result=0;activate_home_card();assert(home_exit_waiting&&!exit_called);
    archive_result=-1;activate_home_card();assert(archive_exit_discard&&!home_exit_waiting&&!exit_called);
    activate_home_card();assert(exit_called);exit_called=false;go_home();assert(!archive_exit_discard);archive_result=1;
    go_home();home_selection=5;neighbor_saving=true;activate_home_card();assert(home_exit_waiting&&!exit_called);neighbor_saving=false;activate_home_card();assert(exit_called);exit_called=false;
    setup_saving=true;go_home();home_selection=5;activate_home_card();assert(home_exit_waiting&&!exit_called);setup_saving=false;activate_home_card();assert(exit_called);exit_called=false;
    config_busy=true;go_home();home_selection=5;activate_home_card();assert(home_exit_waiting&&!exit_called);config_busy=false;
    go_home();home_selection=0;
    home_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);home_navigation(BSP_INPUT_NAVIGATION_KEY_UP);assert(!home_selection);
    home_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(home_selection==1);
    home_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);home_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(home_selection==2);
    home_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);assert(home_selection==5);
    home_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);home_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(home_selection==5);
    home_navigation(BSP_INPUT_NAVIGATION_KEY_UP);assert(home_selection==2);
    for(unsigned i=0;i<6;i++)home_key('\\t');assert(home_selection==2);
    meshdeck_screen_t destinations[]={SCREEN_CHANNEL_LIST,SCREEN_DIRECT_MESSAGES,SCREEN_NODES,SCREEN_MAP,SCREEN_SETTINGS_MENU};
    for(unsigned i=0;i<5;i++){go_home();home_selection=i;home_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(current_screen==destinations[i]);}
    go_home();home_selection=3;home_key('\\r');assert(current_screen==SCREEN_MAP);
    // Counts clear only for the visible thread at its latest messages.
    go_home(); mark_visible_thread_read(); assert(mark_read_calls == 0);
    open_thread(0, 1); display_and_lights_off = true;
    mark_visible_thread_read(); assert(mark_read_calls == 0);
    display_and_lights_off = false; message_scroll_offset = 1;
    mark_visible_thread_read(); assert(mark_read_calls == 0);
    message_visible_valid=true;message_visible_peer=0;message_visible_channel=1;message_visible_first=1;message_visible_end=3;
    message_scroll_offset = 0; chat_actions.modal = 1; mark_visible_thread_read(); assert(mark_read_calls == 0);
    chat_actions.modal = 0; mark_visible_thread_read();
    assert(mark_read_calls == 1 && marked_peer == 0 && marked_channel == 1);
    open_conversation(123);message_visible_valid=true;message_visible_peer=123;message_visible_channel=0;mark_visible_thread_read();
    assert(mark_read_calls == 2 && marked_peer == 123 && marked_channel == 0);
    open_thread(0,0); assert(draft_reply_ids[draft_slot]==777);
    open_thread(0,1); assert(!draft_reply_ids[draft_slot]);
    puts("PASS: previous-screen Esc, independent channel/DM drafts, explicit Exit, unread clearing only for awake visible latest thread");
}
'''
with tempfile.TemporaryDirectory(prefix='meshdeck-navigation-') as folder:
    path = Path(folder)
    (path / 'test.c').write_text(harness)
    subprocess.run(['clang', '-std=c11', '-fsanitize=address,undefined', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
