from ui_source import read_main_ui
"""Exercise actual chat menu navigation and picker/reaction actions with fake UART."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=read_main_ui(root)
a=source.index('typedef enum {');b=source.index('} meshdeck_screen_t;',a)+len('} meshdeck_screen_t;')
enum=source[a:b]
a=source.index('// Chat action state');b=source.index('// End chat action state.',a)
state=source[a:b]
a=source.index('static size_t current_message_count');b=source.index('static bool enabled_channel_at',a)
functions=source[a:b]
history_state=source[source.index('// History settings state.'):source.index('// End history settings state.')]
a=source.index('                char key = event.args_keyboard.ascii;')
b=source.index('                if (current_screen == SCREEN_MANUAL_CHANNEL)',a)
keyboard_guard=source[a:b]
harness='#define EMOJI_TEST_MAIN received_tests\n#include "'+str(root/'tests/test_emoji_reactions.c')+'"\n#include "'+str(root/'main/emoji_picker.c')+'"\n'+'#include "'+str(root/'main/message_reuse.h')+'"\n#include "'+str(root/'main/message_export.c')+'"\n'+'#include "'+str(root/'main/chat_actions.c')+'"\n'+enum+r'''
#define BSP_INPUT_NAVIGATION_KEY_UP 1
#define BSP_INPUT_NAVIGATION_KEY_DOWN 2
#define BSP_INPUT_NAVIGATION_KEY_LEFT 3
#define BSP_INPUT_NAVIGATION_KEY_RIGHT 4
#define BSP_INPUT_NAVIGATION_KEY_RETURN 5
#define BSP_INPUT_NAVIGATION_KEY_ESC 6
#define BSP_INPUT_NAVIGATION_KEY_F1 7
#define BSP_INPUT_NAVIGATION_KEY_F2 8
#define BSP_INPUT_NAVIGATION_KEY_F3 9
#define BSP_INPUT_NAVIGATION_KEY_F4 10
#define BSP_INPUT_NAVIGATION_KEY_F5 11
#define BSP_INPUT_NAVIGATION_KEY_F6 12
static meshdeck_screen_t current_screen=SCREEN_MESSAGES;
static uint32_t message_peer;
static uint8_t message_channel=1;
static char draft[201], message_feedback[64];
static char *message_draft=draft;
static unsigned node_modal;
static size_t message_scroll_offset;
static bool home_key(char key){return false;}
static void open_archive_browser(bool all,uint32_t p,uint8_t c){current_screen=SCREEN_ARCHIVE_BROWSER;}
static bool first_use_key(char key){return false;}
static bool neighbor_key(char key){return false;}
static bool archive_key(char key){return false;}
static bool range_key(char key){return false;}
static bool management_key(char key){return false;}
static bool position_key(char key){return false;}
static bool channel_tools_key(char key){return false;}
static bool waypoint_key(char key){return false;}
static bool map_key(char key){return false;}
static bool packet_key(char key){return false;}
static bool message_search_key(char key){return false;}
static bool behavior_key(char key){return false;}
static bool sound_key(char key){return false;}
static bool telemetry_settings_key(char key){return false;}
static bool telemetry_view_key(char key){(void)key;return false;}
static bool key_backup_key(char key){return false;}
static bool config_backup_key(char key){return false;}
static bool node_nickname_key(char key){return false;}
static bool node_browser_key(char key){return false;}
static bool node_actions_navigation(int key) { (void)key; return false; }
static void open_message_search(void){}
static void draft_ui_touch(void){}
static unsigned draws;
static void draw_screen(void) { draws++; }
'''+state+functions+history_state+r'''
static bool keyboard_reaches_editor(char ascii) {
    struct { struct { char ascii; } args_keyboard; } event={{ascii}};
    for(int once=0;once<1;once++) {
'''+keyboard_guard+r'''
        return true;
    }
    return false;
}
static void enter(void) { assert(chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN)); }
int main(void) {
    reset();
    const uint8_t secondary[]={8,1,24,2}; decode_channel(secondary,sizeof(secondary));
    channel_packet(1,0x11223344,300);
    reaction_packet(0x11223344,local_node_number,0,301,0,"Private DM",false,1);
    size_t writes=write_count;
    strcpy(draft,"My draft ");
    assert(!keyboard_reaches_editor('\t') && chat_actions.modal==CHAT_ACTIONS);
    assert(!chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_F5));
    enter(); assert(chat_actions.modal==CHAT_EMOJI && !chat_actions.picker_reaction);
    chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);
    chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT); enter();
    assert(!chat_actions.modal && !strcmp(draft,"My draft ❤️") && write_count==writes);
    chat_action_enter_us=fake_time;
    assert(!keyboard_reaches_editor('\r') && write_count==writes); // Duplicate ASCII Return.
    fake_time+=200000;
    assert(keyboard_reaches_editor('x'));
    assert(!keyboard_reaches_editor('\t'));
    chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN); enter();
    assert(chat_actions.modal==CHAT_TARGETS && chat_actions.target_count==1 && chat_actions.targets[0].packet_id==300);
    enter(); assert(chat_actions.modal==CHAT_EMOJI && chat_actions.picker_reaction);
    // A newer arrival cannot replace the message selected for this tapback.
    channel_packet(1,0x11223344,302);
    enter();
    assert(!chat_actions.modal && write_count==writes+1 && reactions[0].reply_id==300 && reactions[0].channel==1);
    assert(!strcmp(draft,"My draft ❤️") && strstr(message_feedback,"queued"));
    // Esc walks back within the menu without editing or transmitting.
    assert(!keyboard_reaches_editor('\t')); enter();
    assert(chat_actions.modal==CHAT_EMOJI);
    assert(!keyboard_reaches_editor('a') && !strcmp(draft,"My draft ❤️"));
    chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_ESC); assert(chat_actions.modal==CHAT_ACTIONS);
    chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_ESC); assert(!chat_actions.modal);
    // Quoted reply selection snapshots the parent without editing or transmitting.
    message_peer=0;message_channel=1;writes=write_count;
    open_chat_actions();chat_actions.action_selection=5;enter();assert(chat_actions.modal==CHAT_REPLY_TARGETS&&chat_actions.target_count);
    uint32_t quote=chat_actions.targets[chat_actions.target_selection].packet_id;
    channel_packet(1,0x11223344,303);enter();
    assert(!chat_actions.modal&&draft_reply_ids[draft_slot]==quote&&write_count==writes&&!strcmp(draft,"My draft ❤️"));
    open_chat_actions();chat_actions.action_selection=5;enter();
    assert(!chat_actions.modal&&!draft_reply_ids[draft_slot]&&!strcmp(draft,"My draft ❤️")&&write_count==writes);
    // Byte-limit failures keep the picker open, preserving the original draft.
    memset(draft,'x',198); draft[198]=0;
    assert(!keyboard_reaches_editor('\t')); enter(); enter();
    assert(chat_actions.modal==CHAT_EMOJI && strlen(draft)==198 && strstr(chat_actions.feedback,"200"));
    assert(!chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_F4) && !chat_actions.modal);
    // Direct targets are isolated from channel history.
    message_peer=0x11223344; message_channel=0;
    assert(!keyboard_reaches_editor('\t'));
    chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN); enter();
    assert(chat_actions.target_count==1 && chat_actions.targets[0].packet_id==301 && chat_actions.targets[0].peer_node==message_peer);
    enter(); chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT); enter();
    assert(!chat_actions.modal && reactions[1].reply_id==301 && reactions[1].peer_node==message_peer);
    // Evicted snapshots fail safely instead of reacting to a replacement row.
    message_peer=0; message_channel=1;
    assert(!keyboard_reaches_editor('\t'));
    chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN); enter(); enter();
    writes=write_count;
    for(unsigned i=0;i<510;i++) store_message("Other",(const uint8_t*)"new",3,0,0,false,0);
    enter(); assert(chat_actions.modal==CHAT_EMOJI && write_count==writes && strstr(chat_actions.feedback,"Not queued"));
    chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_F1); assert(!chat_actions.modal);
    current_screen=SCREEN_NODES;
    assert(!chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_F5));

    // Retry requires review and explicit confirmation; duplicate ASCII Return
    // after confirming cannot send an unrelated draft.
    reset();history_fs_ready=true;remove(HISTORY_PATH);remove(HISTORY_BACKUP_PATH);remove(HISTORY_TEMP_PATH);
    current_screen=SCREEN_MESSAGES;message_peer=0;message_channel=1;chat_actions.modal=0;strcpy(draft,"keep draft");
    decode_channel(secondary,sizeof(secondary));assert(mesh_radio_send_channel(1,"try again"));
    ack(local_node_number,messages[0].packet_id,1);writes=write_count;
    assert(!keyboard_reaches_editor('\t'));
    chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);enter();
    assert(chat_actions.modal==CHAT_RETRY_TARGETS&&chat_actions.target_count==1&&write_count==writes);
    enter();assert(chat_actions.modal==CHAT_RETRY_REVIEW&&!chat_actions.confirm_selected);enter();assert(chat_actions.modal==CHAT_RETRY_TARGETS&&write_count==writes);
    enter();chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);enter();
    assert(!chat_actions.modal&&write_count==writes+1&&messages[1].delivery==MESH_DELIVERY_PENDING&&messages[1].channel==1);
    assert(!keyboard_reaches_editor('\r')&&write_count==writes+1&&!strcmp(draft,"keep draft"));
    // Clear affects this channel only, retains pending and does not erase draft.
    channel_packet(0,0x11223344,999);
    assert(!keyboard_reaches_editor('\t'));
    for(unsigned i=0;i<3;i++)chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);enter();
    assert(chat_actions.modal==CHAT_CLEAR_CONFIRM&&!chat_actions.confirm_selected);enter();assert(chat_actions.modal==CHAT_ACTIONS&&message_count==3);
    enter();chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);enter();
    assert(!chat_actions.modal&&message_count==2&&mesh_radio_channel_message_count(0)==1&&!strcmp(draft,"keep draft"));
    assert(messages[0].delivery==MESH_DELIVERY_PENDING&&!keyboard_reaches_editor('\n'));
    writes=write_count;open_chat_actions();chat_actions.action_selection=CHAT_BROWSE_ARCHIVE;enter();
    assert(current_screen==SCREEN_ARCHIVE_BROWSER&&!chat_actions.modal&&write_count==writes);
    current_screen=SCREEN_MESSAGES;
    // Reuse never sends, starts on Cancel, clears quote only after Replace Draft.
    open_chat_actions();chat_actions.action_selection=6;enter();assert(chat_actions.modal==CHAT_COPY_TARGETS);
    mesh_message_t frozen=chat_actions.targets[0];enter();assert(chat_actions.modal==CHAT_COPY_REVIEW&&!chat_actions.confirm_selected);
    writes=write_count;draft_reply_ids[draft_slot]=123;enter();assert(chat_actions.modal==CHAT_COPY_TARGETS&&!strcmp(draft,"keep draft")&&draft_reply_ids[draft_slot]==123);
    enter();chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);enter();assert(!chat_actions.modal&&!strcmp(draft,frozen.text)&&!draft_reply_ids[draft_slot]&&write_count==writes);
    // Individual deletion snapshots a target and requires explicit confirmation.
    open_chat_actions();chat_actions.action_selection=7;enter();assert(chat_actions.modal==CHAT_ACTIONS); // only pending send remains in channel 1
    message_channel=0;open_chat_actions();chat_actions.action_selection=7;enter();assert(chat_actions.modal==CHAT_DELETE_TARGETS);
    size_t count_before=message_count;enter();assert(chat_actions.modal==CHAT_DELETE_REVIEW&&!chat_actions.confirm_selected);enter();assert(chat_actions.modal==CHAT_DELETE_TARGETS&&message_count==count_before);
    enter();chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);enter();assert(!chat_actions.modal&&message_count==count_before-1&&write_count==writes);
    // Export warning is explicit and defaults to Cancel; empty chats do not start work.
    open_chat_actions();chat_actions.action_selection=8;enter();assert(chat_actions.modal==CHAT_EXPORT_REVIEW&&!chat_actions.confirm_selected);enter();assert(chat_actions.modal==CHAT_ACTIONS&&!mesh_chat_export_busy());
    enter();chat_actions_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);enter();assert(chat_actions.modal==CHAT_EXPORT_REVIEW&&strstr(chat_actions.feedback,"No saved")&&write_count==writes);
    // Capacity reduction is a separate explicit confirmation, with duplicate
    // Return events consumed before the second confirmation can be accepted.
    reset();history_fs_ready=true;remove(HISTORY_PATH);remove(HISTORY_BACKUP_PATH);
    for(unsigned i=0;i<300;i++)assert(store_message("Peer",(const uint8_t*)"old",3,0,0,false,0));
    open_history_settings();history_settings_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);assert(history_limit_draft==250);
    history_settings_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);history_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
    assert(history_limit_confirm&&history_limit==500&&message_count==300&&!keyboard_reaches_editor('\r'));
    fake_time+=200000;history_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!history_limit_confirm&&history_limit==250&&message_count==250);
    history_settings_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);open_history_settings();assert(history_limit_draft==250);
    puts("PASS: Tab actions, UTF-8 insert without sending, duplicate Return guard, target snapshots/scope, tapback queue, Esc/circle navigation, byte limit and stale-message rejection");
}
'''
with tempfile.TemporaryDirectory(prefix='meshdeck-chat-actions-') as folder:
 p=Path(folder);(p/'test.c').write_text(harness)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'tests/stubs'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=folder,check=True)
