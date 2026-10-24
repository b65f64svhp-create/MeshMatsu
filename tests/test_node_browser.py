from ui_source import read_main_ui
"""Real radio snapshots and browser/UI helpers; fake transport sends no RF."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
s=read_main_ui(root)
def section(a,b): return s[s.index(a):s.index(b,s.index(a))]
enum=section('typedef enum {','} meshdeck_screen_t;')+'} meshdeck_screen_t;'
state=section('// Node browser state.','// End node browser state.')
helpers=section('// Node browser helpers.','// End node browser helpers.')
move=section('static void move_node_selection(', 'static const char *channel_role')
opening=section('static void open_node_actions(', 'static void start_node_trace(')
keys=['UP','DOWN','LEFT','RIGHT','RETURN','ESC','F1','F2','F3','F4','F5','F6']
prefix='#define main backend_tests\n#include "'+str(root/'tests/test_direct_messages.c')+'"\n#undef main\n#include "'+str(root/'main/node_browser.c')+'"\n'
prefix+='#include "'+str(root/'main/location_utils.c')+'"\n'
prefix+=enum+'\n'+''.join(f'#define BSP_INPUT_NAVIGATION_KEY_{key} {i+1}\n' for i,key in enumerate(keys))
prefix+='''
static size_t node_selection;
static unsigned node_modal;
static bool map_action_origin;
static char telemetry_feedback[96];
enum { NODE_ACTIONS=1,NODE_TRACE };
static mesh_node_t action_node;
static size_t node_action_selection;
static char node_action_feedback[80];
static meshdeck_screen_t current_screen=SCREEN_NODES;
'''+state+helpers+move+opening
guard=section('                char key = event.args_keyboard.ascii;', '                if (current_screen == SCREEN_MESSAGES && chat_actions.modal)')
prefix+=r'''
static int64_t chat_action_enter_us;
static unsigned draws;
static void draw_screen(void){draws++;}
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
static void open_chat_actions(void){}
static bool node_actions_navigation(int key){if(key==BSP_INPUT_NAVIGATION_KEY_RETURN){open_node_actions();return true;}return false;}
static void ascii(char value){
 struct { struct {char ascii;} args_keyboard; } event={{value}};
 for(int once=0;once<1;once++){
'''+guard+r'''
 }
}
'''
body=r'''
static time_t now=1800000000;
static void setup(void){
 reset();node_browser=(mesh_node_browser_t){.favorites_first=true};node_selection=0;node_modal=0;node_search_editing=false;
 node_count=7;
 nodes[0]=(mesh_node_t){.number=local_node_number,.name="Me",.favorite_known=true,.is_favorite=true,.last_heard=now};
 nodes[1]=(mesh_node_t){.number=0xabcdef01,.name="Bravo",.short_name="BUL",.user_id="!abcdef01",.last_heard=now-20,.hops_away_available=true,.hops_away=2};
 nodes[2]=(mesh_node_t){.number=0xabcdef02,.name="alpha",.favorite_known=true,.is_favorite=true,.last_heard=now-9000,.hops_away_available=true,.hops_away=0};
 nodes[3]=(mesh_node_t){.number=0xabcdef03,.name="Delta",.last_heard=now-7200,.hops_away_available=true,.hops_away=1};
 nodes[4]=(mesh_node_t){.number=0xabcdef04,.name="charlie",.last_heard=now-10,.is_favorite=true}; // Unconfirmed flag must not be treated as favorite.
 nodes[5]=(mesh_node_t){.number=0xabcdef05,.name="Unknown time"};
 nodes[6]=(mesh_node_t){.number=0xabcdef06,.name="Future time",.last_heard=now+100};
}
static void search(const char *q){snprintf(node_browser.query,sizeof(node_browser.query),"%s",q);node_selection=mesh_node_browser_refresh(&node_browser,now,node_selection);}
int main(void){
 setup();assert(filtered_node_count(now)==6&&node_browser.nodes[0].number==0xabcdef02);
 assert(node_browser.nodes[1].number==0xabcdef04&&node_browser.nodes[2].number==0xabcdef01);
 node_browser.favorites_first=false;filtered_node_count(now);assert(node_browser.nodes[0].number==0xabcdef04);
 // Selection follows identity through reorder, database order changes, and removals.
 assert(node_browser.nodes[node_selection].number==0xabcdef02);
 mesh_node_t swap=nodes[1];nodes[1]=nodes[4];nodes[4]=swap;filtered_node_count(now);
 assert(node_browser.nodes[node_selection].number==0xabcdef02);
 node_browser.filter=MESH_NODES_ACTIVE;assert(filtered_node_count(now)==3); // Inclusive two-hour boundary.
 for(size_t i=0;i<node_browser.count;i++)assert(node_browser.nodes[i].number!=local_node_number);
 node_browser.filter=MESH_NODES_FAVORITES;assert(filtered_node_count(now)==1&&node_browser.nodes[0].number==0xabcdef02);
 nodes[1].favorite_known=true; // charlie now confirmed.
 assert(filtered_node_count(now)==2);
 node_browser.filter=MESH_NODES_ALL;node_browser.sort=MESH_NODES_NAME;filtered_node_count(now);
 assert(!strcmp(node_browser.nodes[0].name,"alpha")&&!strcmp(node_browser.nodes[1].name,"Bravo"));
 node_browser.sort=MESH_NODES_HOPS;filtered_node_count(now);
 assert(node_browser.nodes[0].hops_away==0&&node_browser.nodes[1].hops_away==1&&node_browser.nodes[2].hops_away==2);
 assert(!node_browser.nodes[3].hops_away_available);
 search("bRaVo");assert(node_browser.count==1&&node_browser.nodes[0].number==0xabcdef01);
 search("bul");assert(node_browser.count==1);search("!ABCDEF01");assert(node_browser.count==1);
 search("abcdef01");assert(node_browser.count==1);search("2882400001");assert(node_browser.count==1);
 search("no match");assert(!node_browser.count&&!node_browser.selected&&node_selection==0);
 search("");assert(node_browser.count==6);
 node_browser.filter=MESH_NODES_ACTIVE;assert(!mesh_node_browser_refresh(&node_browser,0,0)&&!node_browser.count);
 node_browser.filter=MESH_NODES_ALL;filtered_node_count(now);
 // The action target is refreshed and frozen before the menu opens.
 node_browser.selected=0xabcdef01;filtered_node_count(time(NULL));
 size_t writes=write_count;open_node_actions();assert(node_modal==NODE_ACTIONS&&action_node.number==0xabcdef01&&write_count==writes);
 nodes[1].last_heard=now;filtered_node_count(now);assert(action_node.number==0xabcdef01);
 node_modal=0;
 // Ignored nodes have an explicit recovery filter and stay out of normal lists.
 nodes[1].is_ignored=true;node_browser.filter=MESH_NODES_ALL;size_t normal=filtered_node_count(now);
 node_browser.filter=MESH_NODES_IGNORED;assert(filtered_node_count(now)==1&&node_browser.nodes[0].number==nodes[1].number);
 nodes[1].is_ignored=false;node_browser.filter=MESH_NODES_ALL;assert(filtered_node_count(now)==normal+1);
 // Actual UI helpers: F/Tab, full filter cycle, live search and cancellation.
 node_browser.filter=MESH_NODES_ALL;node_browser.sort=MESH_NODES_LAST_HEARD;node_browser.favorites_first=true;
 assert(node_browser_key('F')&&!node_browser.favorites_first);
 assert(node_browser_key('\t')&&node_browser.sort==MESH_NODES_NAME);
 assert(node_browser_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT)&&node_browser.filter==MESH_NODES_ACTIVE);
 node_browser_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(node_browser.filter==MESH_NODES_FAVORITES);
 node_browser_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(node_browser.filter==MESH_NODES_NEARBY);
 assert(mesh_node_browser_radius(&node_browser)==25000);node_browser_key('+');assert(mesh_node_browser_radius(&node_browser)==50000);node_browser_key('-');assert(mesh_node_browser_radius(&node_browser)==25000);
 node_browser_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(node_browser.filter==MESH_NODES_IGNORED);
 node_browser_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(node_browser.filter==MESH_NODES_ALL);
 assert(!node_browser_key('h'));
 for(unsigned i=0;i<MESH_NODE_SORT_COUNT;i++)node_browser_key('\t');assert(node_browser.sort==MESH_NODES_NAME);
 search("");uint32_t selected=node_browser.selected;
 node_browser_key('/');node_browser_key('B');node_browser_key('r');filtered_node_count(now);assert(node_search_editing&&node_browser.count==1);
 node_browser_key('+');assert(!strcmp(node_browser.query,"Br+"));node_browser_key('\b');
 node_browser_key('f');assert(!node_browser.favorites_first&&!strcmp(node_browser.query,"Brf"));
 node_browser_key('\b');assert(!strcmp(node_browser.query,"Br"));
 assert(node_browser_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT)&&node_browser.filter==MESH_NODES_ALL);
 assert(node_browser_navigation(BSP_INPUT_NAVIGATION_KEY_ESC)&&!node_search_editing&&!node_browser.query[0]);
 assert(node_browser.selected==selected);
 node_browser_key('/');node_browser_key('b');node_browser_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!node_search_editing&&!strcmp(node_browser.query,"b"));
 node_browser_key('/');node_browser_key('\b');assert(!node_browser.query[0]);
 for(int i=0;i<100;i++)node_browser_key('x');assert(strlen(node_browser.query)==39);
 node_browser_key('\n');assert(!node_search_editing);
 assert(!node_browser_navigation(BSP_INPUT_NAVIGATION_KEY_F5)); // Clover remains unassigned in Nodes.
 // Actual ASCII dispatcher: finishing search cannot also open Actions.
 search("Bravo");node_browser_key('/');
 assert(node_browser_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN));chat_action_enter_us=fake_time;
 ascii('\r');assert(!node_modal);
 fake_time+=200000;ascii('\r');assert(node_modal==NODE_ACTIONS&&action_node.number==0xabcdef01);
 node_modal=0;node_browser_key('/');ascii('\r');assert(!node_search_editing&&!node_modal);
 ascii('\n');assert(!node_modal);
 // Full bounds, deterministic ID tie-break, and fallback when selection disappears.
 setup();node_browser.favorites_first=false;node_browser.sort=MESH_NODES_NAME;node_count=MESH_NODE_LIMIT;
 for(size_t i=0;i<node_count;i++)nodes[i]=(mesh_node_t){.number=(uint32_t)(1000+node_count-i),.name="Same"};
 assert(filtered_node_count(now)==MESH_NODE_LIMIT);for(size_t i=1;i<node_count;i++)assert(node_browser.nodes[i-1].number<node_browser.nodes[i].number);
 node_selection=127;node_browser.selected=node_browser.nodes[127].number;node_count=1;filtered_node_count(now);assert(node_selection==0&&node_browser.selected==nodes[0].number);
 assert(write_count==0);
 puts("PASS: confirmed favorites, active boundary/invalid clocks, all sorting modes and unknown hops, case-insensitive name/short/hex/decimal ID search, bounded input, selection stability, frozen Actions, search save/cancel, controls and 128-node bounds; no radio writes");
}
'''
with tempfile.TemporaryDirectory(prefix='meshmatsu-browser-') as folder:
 p=Path(folder);(p/'test.c').write_text(prefix+body)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'tests/stubs'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=folder,check=True)
