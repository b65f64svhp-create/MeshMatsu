from ui_source import read_main_ui
"""Exercise production clover/pan/zoom/node selection without radio writes."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=read_main_ui(root)
enum=s[s.index('typedef enum {'):s.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
ui=(root/'main/map_ui.inc').read_text().split('#include "map_trails_draw.inc"')[0]
h='#define main backend_tests\n#include "'+str(root/'tests/test_direct_messages.c')+'"\n#undef main\n#include "'+str(root/'main/map_view.c')+'"\n'+enum+'\n'
for i,k in enumerate(['F1','F5','LEFT','RIGHT','UP','DOWN','RETURN']):h+=f'#define BSP_INPUT_NAVIGATION_KEY_{k} {i+1}\n'
h+=r'''
static meshdeck_screen_t current_screen=SCREEN_HOME;
enum { NODE_ACTIONS=1 };
static struct {unsigned modal;} chat_actions;
static unsigned node_modal;
static size_t node_action_selection;
static bool map_action_origin;
static char node_action_feedback[80];
static mesh_node_t action_node;
static size_t node_selection;
static bool node_search_editing;
static unsigned map_retries;
static void mesh_map_tiles_retry(void){map_retries++;}
static bool get_filtered_node(size_t i,time_t now,mesh_node_t *node){return mesh_radio_get_node(i,node);}
'''+ui+r'''
int main(void){
 reset();size_t writes=write_count;
 assert(map_navigation(BSP_INPUT_NAVIGATION_KEY_F5)&&current_screen==SCREEN_MAP);
 assert(!map_selected_node&&map_view.zoom==8);
 // Regional default is not assigned to a radio node.
 assert(!node_count&&!message_count&&!write_count);
 nodes[0]=(mesh_node_t){.number=42,.position_available=true,.latitude_i=551700000,.longitude_i=-1188000000};
 nodes[1]=(mesh_node_t){.number=43};nodes[2]=(mesh_node_t){.number=44,.position_available=true,.latitude_i=540000000,.longitude_i=-1170000000};node_count=3;
 map_key('\t');assert(map_selected_node==42);double x=map_view.x,y=map_view.y;
 map_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(map_view.x>x&&map_view.y==y);
 map_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);assert(map_view.y>y);
 map_key('c');assert(map_view.x==x&&map_view.y==y);
 map_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(current_screen==SCREEN_NODES&&node_modal==NODE_ACTIONS&&action_node.number==42&&map_action_origin);
 current_screen=SCREEN_MAP;node_modal=0;
 map_key('+');assert(map_view.zoom==9);map_key('-');assert(map_view.zoom==8);
 for(int i=0;i<30;i++)map_key('+');assert(map_view.zoom==13);
 for(int i=0;i<30;i++)map_key('-');assert(map_view.zoom==1);
 map_key('\t');assert(map_selected_node==44);map_key('\t');assert(map_selected_node==42);
 assert(map_key('n')&&current_screen==SCREEN_MAP);
 assert(map_trail_mode==MAP_TRAILS_SELECTED);map_key('t');assert(map_trail_mode==MAP_TRAILS_ALL);map_key('T');assert(map_trail_mode==MAP_TRAILS_OFF);map_key('t');assert(map_trail_mode==MAP_TRAILS_SELECTED);
 map_key('r');map_key('R');assert(map_retries==2);
 assert(!map_navigation(BSP_INPUT_NAVIGATION_KEY_F1));
 current_screen=SCREEN_HOME;assert(!map_key('t')&&map_trail_mode==MAP_TRAILS_SELECTED);current_screen=SCREEN_MAP;
 current_screen=SCREEN_NODES;node_selection=2;map_navigation(BSP_INPUT_NAVIGATION_KEY_F5);assert(map_selected_node==44);
 current_screen=SCREEN_NODES;node_modal=1;action_node=nodes[0];node_selection=2;
 map_navigation(BSP_INPUT_NAVIGATION_KEY_F5);assert(map_selected_node==42&&!node_modal);
 current_screen=SCREEN_MESSAGES;chat_actions.modal=3;map_navigation(BSP_INPUT_NAVIGATION_KEY_F5);assert(!chat_actions.modal&&current_screen==SCREEN_MAP);

 // Filters scope selection, actions and Tab without changing radio favorites.
 const uint32_t now=mesh_radio_time();
 nodes[0].position_time=now;nodes[2].position_time=now-3600;nodes[2].is_favorite=true;
 map_key('f');assert(map_filters.favorites_only&&!map_selected_node);
 map_key('\t');assert(map_selected_node==44);
 map_key('O');assert(map_filters.recent_only&&map_selected_node==44);
 nodes[2].position_time=now-3601;map_key('\t');assert(!map_selected_node);
 map_key('\r');assert(current_screen==SCREEN_MAP&&!node_modal);
 map_key('o');map_key('\t');assert(map_selected_node==44);
 map_key('F');assert(!map_filters.favorites_only);map_key('\t');assert(map_selected_node==42);
 // Unknown/future fix ages are hidden in Recent; boundary is inclusive.
 mesh_map_filters_t filters={.recent_only=true};mesh_node_t n=nodes[0];
 assert(mesh_map_node_visible(&filters,&n,now));
 n.position_time=now-3600;assert(mesh_map_node_visible(&filters,&n,now));
 n.position_time=now-3601;assert(!mesh_map_node_visible(&filters,&n,now));
 n.position_time=0;assert(!mesh_map_node_visible(&filters,&n,now));
 n.position_time=now+1;assert(!mesh_map_node_visible(&filters,&n,now));
 n.position_time=now;assert(!mesh_map_node_visible(&filters,&n,0));
 n.is_ignored=true;assert(!mesh_map_node_visible(&filters,&n,now));
 n.is_ignored=false;n.position_available=false;assert(!mesh_map_node_visible(&filters,&n,now));
 filters.favorites_only=true;n.position_available=true;assert(!mesh_map_node_visible(&filters,&n,now));
 n.is_favorite=true;assert(mesh_map_node_visible(&filters,&n,now));
 map_waypoint_id=7;map_key('v');assert(map_filters.waypoints_hidden&&!map_waypoint_id);
 map_key('V');assert(!map_filters.waypoints_hidden);
 current_screen=SCREEN_HOME;map_key('f');map_key('o');map_key('v');
 assert(!map_filters.favorites_only&&!map_filters.recent_only&&!map_filters.waypoints_hidden);
 current_screen=SCREEN_MAP;
 assert(write_count==writes&&!message_count);
 puts("PASS: global clover map, regional/no-fix state, pan/zoom/recenter, reported node cycling, frozen node target, global navigation passthrough and no radio transmissions");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'tests/stubs'),'-I',str(root/'main'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
