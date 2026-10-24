from ui_source import read_main_ui
"""Exercise actual node Actions navigation against the fake radio transport."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
s=read_main_ui(root)
enum=s[s.index('typedef enum {'):s.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
state=s[s.index('// Node actions keep'):s.index('// End node action state.')]
functions=s[s.index('static void open_node_actions'):s.index('static void resolve_direct_selection')]
keys=['UP','DOWN','LEFT','RIGHT','RETURN','ESC','F1','F2','F3','F4','F5','F6']
harness='#define NODE_ALIASES_PATH "nicknames-ui.bin"\n#define main backend_tests\n#include "'+str(root/'tests/test_direct_messages.c')+'"\n#undef main\n'+enum+'\n'+''.join(f'#define BSP_INPUT_NAVIGATION_KEY_{k} {i+1}\n' for i,k in enumerate(keys))+r'''
#define UI_HINT_OFFSET 60
static unsigned logical_v_res=480;
static meshdeck_screen_t current_screen=SCREEN_NODES;
static size_t node_selection;
static uint32_t opened_peer;
static uint32_t neighbor_opened;
static void open_neighbor_info(uint32_t n){neighbor_opened=n;current_screen=SCREEN_NEIGHBOR_INFO;}
static void open_range_testing(uint32_t n){current_screen=SCREEN_RANGE_TEST;}
static void open_security_centre(uint32_t n){current_screen=SCREEN_SECURITY;}
static void open_remote_manager(uint32_t n){current_screen=SCREEN_REMOTE;}
static void open_conversation(uint32_t peer) { opened_peer=peer;current_screen=SCREEN_MESSAGES; }
static bool home_key(char key){return false;}
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
static void go_back(void){current_screen=SCREEN_MAP;}
static void open_chat_actions(void){}
static bool node_browser_key(char key){return false;}
static size_t filtered_node_count(time_t now){return 1;}
static bool has_node=true;
static uint32_t selected_node=0xaabbccdd;
static bool get_filtered_node(size_t index,time_t now,mesh_node_t *out) {
 (void)index;(void)now;if(!has_node)return false;
 *out=(mesh_node_t){.number=selected_node};return true;
}
'''+state+r'''
static uint8_t telemetry_kind=2;
static bool telemetry_export_visible;
static void telemetry_view_open(uint32_t node,bool local){telemetry_kind=local?6:2;}
static bool telemetry_view_navigation(int key){return key==BSP_INPUT_NAVIGATION_KEY_DOWN||key==BSP_INPUT_NAVIGATION_KEY_UP||key==BSP_INPUT_NAVIGATION_KEY_LEFT||key==BSP_INPUT_NAVIGATION_KEY_RIGHT;}
'''+functions+r'''
static struct {unsigned modal;} chat_actions;
static unsigned draws;
static int64_t chat_action_enter_us;
static bool chat_actions_navigation(int key){return false;}
static void draw_screen(void){draws++;}

static void key(int k){assert(node_actions_navigation(k));}
int main(void){
 reset();size_t writes=write_count;
 assert(!node_actions_navigation(BSP_INPUT_NAVIGATION_KEY_F5));
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(node_modal==NODE_ACTIONS&&action_node.number==selected_node&&write_count==writes);
 selected_node=0xabcdef01; // Node updates must not retarget the open menu.
 key(BSP_INPUT_NAVIGATION_KEY_DOWN);key(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(node_modal==NODE_TRACE&&write_count==writes+1);
 mesh_trace_result_t trace;assert(mesh_radio_get_traceroute(0xaabbccdd,&trace)&&trace.state==MESH_TRACE_WAITING);
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(write_count==writes+1);
 key(BSP_INPUT_NAVIGATION_KEY_ESC);key(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(node_modal==NODE_ACTIONS&&strstr(node_action_feedback,"already waiting"));
 key(BSP_INPUT_NAVIGATION_KEY_DOWN);key(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(node_modal==NODE_TRACE&&write_count==writes+1);
 assert(!node_actions_navigation(BSP_INPUT_NAVIGATION_KEY_F6)&&node_modal==NODE_TRACE);
 traces[0].state=MESH_TRACE_COMPLETE;traces[0].route_count=8;traces[0].return_count=1;
 for(int i=0;i<20;i++)key(BSP_INPUT_NAVIGATION_KEY_DOWN);
 assert(trace_scroll==4);key(BSP_INPUT_NAVIGATION_KEY_UP);assert(trace_scroll==3);
 key(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(trace_return_selected&&trace_scroll==0);
 key(BSP_INPUT_NAVIGATION_KEY_DOWN);assert(!trace_scroll);
 key(BSP_INPUT_NAVIGATION_KEY_LEFT);assert(!trace_return_selected);
 key(BSP_INPUT_NAVIGATION_KEY_ESC);key(BSP_INPUT_NAVIGATION_KEY_ESC);assert(!node_modal);
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(action_node.number==selected_node);
 assert(!node_actions_navigation(BSP_INPUT_NAVIGATION_KEY_F4)&&!node_modal);
 has_node=false;key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!node_modal&&write_count==writes+1);
 current_screen=SCREEN_MESSAGES;assert(!node_actions_navigation(BSP_INPUT_NAVIGATION_KEY_F5));
 // Favorites keep the frozen action target and wait for radio confirmation.
 current_screen=SCREEN_NODES;has_node=true;selected_node=0xaabbccdd;
 nodes[0]=(mesh_node_t){.number=selected_node,.favorite_known=true};node_count=1;
 session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);for(int i=0;i<3;i++)key(BSP_INPUT_NAVIGATION_KEY_DOWN);
 assert(node_action_selection==3);selected_node=0xabcdef01;writes=write_count;
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(favorite_status.node==0xaabbccdd&&favorite_status.desired&&favorite_status.state==MESH_FAVORITE_SAVING);
 assert(node_modal==NODE_ACTIONS&&write_count==writes+1&&!nodes[0].is_favorite);
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(write_count==writes+1&&strstr(node_action_feedback,"already"));
 favorite_status.state=MESH_FAVORITE_SAVED;nodes[0].is_favorite=true;
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!favorite_status.desired&&write_count==writes+2);
 assert(!node_actions_navigation(BSP_INPUT_NAVIGATION_KEY_F6)&&node_modal==NODE_ACTIONS);
 for(int i=0;i<3;i++)key(BSP_INPUT_NAVIGATION_KEY_UP);assert(node_action_selection==0);
 writes=write_count;key(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(!node_modal&&opened_peer==0xaabbccdd&&current_screen==SCREEN_MESSAGES&&write_count==writes);
 current_screen=SCREEN_NODES;node_modal=NODE_ACTIONS;map_action_origin=true;node_action_selection=4;
 selected_node=0x11111111;key(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(node_modal==NODE_TELEMETRY&&action_node.number==0xaabbccdd&&write_count==writes);
 size_t previous_scroll=trace_scroll;key(BSP_INPUT_NAVIGATION_KEY_DOWN);
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(node_modal==NODE_TELEMETRY&&trace_scroll==previous_scroll&&write_count==writes+1);
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(write_count==writes+1);writes=write_count;
 assert(!node_actions_navigation(BSP_INPUT_NAVIGATION_KEY_F6));
 key(BSP_INPUT_NAVIGATION_KEY_ESC);assert(node_modal==NODE_ACTIONS&&map_action_origin);

 key(BSP_INPUT_NAVIGATION_KEY_ESC);assert(current_screen==SCREEN_MAP&&!node_modal&&!map_action_origin&&write_count==writes);
 current_screen=SCREEN_NODES;node_modal=NODE_TELEMETRY;action_node.number=local_node_number;
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(write_count==writes&&strstr(telemetry_feedback,"own radio"));
 // New radio controls require review; Enter initially chooses Cancel.
 reset();current_screen=SCREEN_NODES;node_modal=NODE_ACTIONS;action_node.number=0xaabbccdd;
 nodes[0]=(mesh_node_t){.number=action_node.number,.favorite_known=true};node_count=1;
 session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;
 node_action_selection=6;writes=write_count;key(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(node_modal==NODE_CONTROL_CONFIRM&&!node_control_confirm&&write_count==writes);
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(node_modal==NODE_ACTIONS&&write_count==writes);
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);key(BSP_INPUT_NAVIGATION_KEY_RIGHT);key(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(node_modal==NODE_ACTIONS&&node_control.state==MESH_FAVORITE_SAVING&&node_control.kind==MESH_NODE_IGNORE&&write_count==writes+1);
 node_control.state=MESH_FAVORITE_SAVED;nodes[0].is_ignored=true;
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(node_control_kind==MESH_NODE_UNIGNORE);key(BSP_INPUT_NAVIGATION_KEY_ESC);
 node_action_selection=7;writes=write_count;key(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(node_modal==NODE_CONTROL_CONFIRM&&node_control_kind==MESH_NODE_REMOVE&&!node_control_confirm&&write_count==writes);
 key(BSP_INPUT_NAVIGATION_KEY_RIGHT);key(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(node_control.kind==MESH_NODE_REMOVE&&write_count==writes+1);
 history_fs_ready=true;node_modal=NODE_ACTIONS;node_action_selection=9;writes=write_count;key(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(node_modal==NODE_NICKNAME&&nickname_replace&&!nickname_draft[0]);strcpy(nickname_draft,"Camp North");key(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(node_modal==NODE_ACTIONS&&!strcmp(mesh_radio_nickname(action_node.number),"Camp North")&&write_count==writes);
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(node_modal==NODE_NICKNAME&&!strcmp(nickname_draft,"Camp North"));key(BSP_INPUT_NAVIGATION_KEY_ESC);assert(node_modal==NODE_ACTIONS&&!strcmp(mesh_radio_nickname(action_node.number),"Camp North"));
 key(BSP_INPUT_NAVIGATION_KEY_RETURN);nickname_draft[0]=0;key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!mesh_radio_nickname(action_node.number)[0]&&write_count==writes);
 current_screen=SCREEN_NODES;node_modal=NODE_ACTIONS;node_action_selection=13;action_node.number=0xaabbccdd;writes=write_count;key(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(current_screen==SCREEN_NEIGHBOR_INFO&&neighbor_opened==0xaabbccdd&&write_count==writes);
 puts("PASS: Direct Message first/default and no-send open; favorite add/remove and busy protection use frozen node, no optimistic marker, screen-off; node target snapshot, explicit start, result viewing without sending, pending protection, path scrolling, Esc, global navigation, screen-off and empty list");
}
'''
source=read_main_ui(root)
a=source.index('                char key = event.args_keyboard.ascii;')
b=source.index('                if (current_screen == SCREEN_MANUAL_CHANNEL)',a)
guard=source[a:b]
keyboard=r'''
static void check_duplicate_return(void){
 struct { struct { char ascii; } args_keyboard; } event={{'\r'}};
 for(int once=0;once<1;once++){
'''+guard+r'''
  assert(!"Return escaped its guard");
 }
 assert(node_modal==NODE_ACTIONS&&current_screen==SCREEN_NODES&&!opened_peer);
}
'''
harness=harness.replace('int main(void){',keyboard+'\nint main(void){')
harness=harness.replace(' selected_node=0xabcdef01; // Node updates', ' chat_action_enter_us=fake_time;check_duplicate_return();\n key(BSP_INPUT_NAVIGATION_KEY_F5);assert(node_modal==NODE_ACTIONS&&node_action_selection==0);\n selected_node=0xabcdef01; // Node updates')
with tempfile.TemporaryDirectory(prefix='meshmatsu-node-actions-') as folder:
 p=Path(folder);(p/'test.c').write_text(harness)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'tests/stubs'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=folder,check=True)
