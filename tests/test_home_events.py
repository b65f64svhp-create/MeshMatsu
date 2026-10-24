from ui_source import read_main_ui
"""One physical Home Enter must not activate a second destination action."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];s=read_main_ui(root)
enum=s[s.index('typedef enum {'):s.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
functions=s[s.index('static void activate_home_card('):s.index('static void open_node_actions(')]
nav=s[s.index('                if (home_navigation(event.args_navigation.key)) {'):s.index('                if (node_browser_navigation(event.args_navigation.key)) {')]
a=s.index('                char key = event.args_keyboard.ascii;');b=s.index('                if (position_key(key))',a);guard=s[a:b]
h='''#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
'''+enum+'''
#define BSP_INPUT_NAVIGATION_KEY_LEFT 1
#define BSP_INPUT_NAVIGATION_KEY_RIGHT 2
#define BSP_INPUT_NAVIGATION_KEY_UP 3
#define BSP_INPUT_NAVIGATION_KEY_DOWN 4
#define BSP_INPUT_NAVIGATION_KEY_RETURN 5
static meshdeck_screen_t current_screen=SCREEN_HOME;
static size_t home_selection,node_selection,direct_selection;
static unsigned node_modal,draws,second_actions;
static uint32_t direct_selected_peer;
static bool node_search_editing,home_exit_failed,home_exit_waiting,home_exit_discard;
static unsigned range_stops,restarts;static bool range_exporting;
static bool mesh_range_export_busy(void){return range_exporting;}
static void mesh_radio_range_stop(void){range_stops++;}
static bool archive_exit_discard;
static bool mesh_radio_setup_busy(void){return false;}
static bool mesh_radio_neighbor_config_busy(void){return false;}
static int mesh_archive_flush(int64_t now){return 1;}
static bool export_busy,draft_exit_discard;
static void draft_ui_touch(void){}
static int draft_ui_flush(void){return 1;}
#define CFG_SAVING 3
static bool config_busy;
static bool mesh_config_store_busy(void){return config_busy;}
static int mesh_radio_config_state(void){return 0;}
static bool mesh_tlm_export_busy(void){return export_busy;}
static bool chat_export_busy;
static bool mesh_chat_export_busy(void){return chat_export_busy;}
enum {MESH_ROLE_SAVING=1};
static struct {int state;} role_save;
#define mesh_radio_role_save() (role_save)
static int telemetry_flush_result=1;
static int mesh_tlm_store_flush(int64_t now){return telemetry_flush_result;}
static struct {unsigned selected;} node_browser;
static int64_t now=1000000,chat_action_enter_us;
static int64_t esp_timer_get_time(void){return now;}
static void open_map(void){current_screen=SCREEN_MAP;}
static void open_settings(void){current_screen=SCREEN_SETTINGS_MENU;}
static bool mesh_radio_flush_waypoints(void){return true;}
static bool mesh_radio_flush_history(void){return true;}
static void mesh_radio_flush_node_cache(void){}
static void bsp_device_restart_to_launcher(void){restarts++;}
static bool first_use_key(char key){return false;}
static bool neighbor_key(char key){return false;}
static bool archive_key(char key){return false;}
static bool range_key(char key){return false;}
static bool management_key(char key){return false;}
static void draw_screen(void){draws++;}
'''+functions+'''
static void nav_return(void){
 struct {struct {int key;} args_navigation;} event={{BSP_INPUT_NAVIGATION_KEY_RETURN}};
 for(int once=0;once<1;once++){
'''+nav+'''
 }
}
static void ascii_return(void){
 struct {struct {char ascii;} args_keyboard;} event={{'\\r'}};
 for(int once=0;once<1;once++){
'''+guard+'''
 if(current_screen==SCREEN_NODES||current_screen==SCREEN_SETTINGS_MENU)second_actions++;
 }
}
int main(void){
 home_selection=2;nav_return();assert(current_screen==SCREEN_NODES&&draws==1&&chat_action_enter_us==now);
 ascii_return();assert(!second_actions);now+=200000;ascii_return();assert(second_actions==1);
 current_screen=SCREEN_HOME;home_selection=4;now+=200000;nav_return();ascii_return();assert(current_screen==SCREEN_SETTINGS_MENU&&second_actions==1);
 current_screen=SCREEN_HOME;home_selection=3;assert(home_key('\\r')&&current_screen==SCREEN_MAP);
 assert(!restarts);current_screen=SCREEN_HOME;home_selection=5;range_exporting=true;nav_return();assert(range_stops==1&&home_exit_waiting&&!restarts);range_exporting=false;nav_return();assert(restarts==1&&range_stops==2);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,cwd=d)
