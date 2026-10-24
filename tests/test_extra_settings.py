from ui_source import read_main_ui
"""Exercise the actual preferences navigation and NVS implementation."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=read_main_ui(root)
enum=s[s.index('typedef enum {'):s.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
helpers=s[s.index('// Message text preferences.'):s.index('// End message text preferences.')]
harness=r'''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "text_settings.h"
#include "nvs.h"
#include "radio_link.h"
static uint8_t stored[2],pending[2];static size_t stored_size;static int fail_write,fail_commit;static unsigned refreshes;
int nvs_open(const char *ns,int mode,nvs_handle_t *h){assert(!strcmp(ns,"meshdeck_mui"));*h=1;return 0;}
int nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size){assert(!strcmp(key,"text_size_v1"));memcpy(out,stored,stored_size);*size=stored_size;return 0;}
int nvs_set_blob(nvs_handle_t h,const char *key,const void *in,size_t size){assert(!strcmp(key,"text_size_v1"));memcpy(pending,in,size);return fail_write;}
int nvs_commit(nvs_handle_t h){if(fail_commit)return fail_commit;memcpy(stored,pending,2);stored_size=2;return 0;}
void nvs_close(nvs_handle_t h){}
bool mesh_radio_request_metadata(void){refreshes++;return true;}
#define BSP_INPUT_NAVIGATION_KEY_LEFT 1
#define BSP_INPUT_NAVIGATION_KEY_RIGHT 2
#define BSP_INPUT_NAVIGATION_KEY_RETURN 3
#define BSP_INPUT_NAVIGATION_KEY_ESC 4
#define BSP_INPUT_NAVIGATION_KEY_UP 5
#define BSP_INPUT_NAVIGATION_KEY_DOWN 6
'''+enum+r'''
static meshdeck_screen_t current_screen=SCREEN_HOME;
static size_t message_scroll_offset=4;
'''+helpers+r'''int main(void){
 uint8_t size=MESH_TEXT_MEDIUM;assert(mesh_text_size_load(&size)!=0&&size==MESH_TEXT_MEDIUM);
 open_text_settings();assert(text_size_draft==MESH_TEXT_MEDIUM&&current_screen==SCREEN_TEXT_SETTINGS);
 extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(text_size_draft==MESH_TEXT_LARGE&&message_text_size==MESH_TEXT_MEDIUM);
 assert(!extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_ESC));open_text_settings();assert(text_size_draft==MESH_TEXT_MEDIUM);
 extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(message_text_size==MESH_TEXT_SMALL&&message_scroll_offset==0);size=2;assert(!mesh_text_size_load(&size)&&size==0);
 extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);fail_commit=7;extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(message_text_size==MESH_TEXT_SMALL&&strstr(text_size_feedback,"retry"));fail_commit=0;fail_write=7;
 extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(message_text_size==MESH_TEXT_SMALL);fail_write=0;
 assert(mesh_text_size_save(3)!=0);stored[0]=99;size=1;assert(mesh_text_size_load(&size)!=0&&size==1);
 for(unsigned i=0;i<3;i++){assert(mesh_message_font(i)==(i==0?12:i==1?14:18));assert(mesh_message_line_height(i)>=mesh_message_font(i)+6);}
 current_screen=SCREEN_ABOUT;extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(refreshes==1);
 extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(about_action_selection==1);
 extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(current_screen==SCREEN_RADIO_ACTIVITY&&refreshes==1);
 assert(extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN)&&refreshes==1);
 assert(!extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_ESC));
 current_screen=SCREEN_ABOUT;extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);
 extra_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!about_action_selection&&refreshes==2);
 puts("PASS: text size default, preview/cancel, save/reload, failed writes/commits, invalid settings, font spacing and metadata refresh action");
}
'''
# Exercise the actual reordered settings menu dispatch as well.
menu=(root/'main/settings_menu_ui.inc').read_text()
extra='#include "'+str(root/'main/settings_menu.c')+'"\n'+r''' 
#define BSP_INPUT_NAVIGATION_KEY_UP 5
#define BSP_INPUT_NAVIGATION_KEY_DOWN 6
static size_t settings_menu_selection,channel_selection,node_settings_selection,display_settings_selection;
static unsigned import_slot,owner_requests;
static bool import_pending;
static char import_feedback[80];
bool mesh_radio_request_owner(void){owner_requests++;return true;}
uint32_t mesh_radio_local_node(void){return 0x12345678;}
static void open_archive_settings(void){current_screen=SCREEN_ARCHIVE_SETTINGS;}
static void open_first_use(bool automatic){current_screen=SCREEN_FIRST_USE;}
static void open_neighbor_info(uint32_t n){current_screen=SCREEN_NEIGHBOR_INFO;}
static void open_range_testing(uint32_t n){current_screen=SCREEN_RANGE_TEST;}
static void open_security_centre(uint32_t n){current_screen=SCREEN_SECURITY;}
static void open_position_settings(void){current_screen=SCREEN_POSITION_SETTINGS;}
static void open_behavior_settings(void){current_screen=SCREEN_RADIO_BEHAVIOR;}
static void open_radio_settings(void){current_screen=SCREEN_RADIO_SETTINGS;}
static void open_manual_channel(void){current_screen=SCREEN_MANUAL_CHANNEL;}
static void open_display_settings(void){current_screen=SCREEN_DISPLAY_ALERTS;}
static void open_power_settings(void){current_screen=SCREEN_POWER_SETTINGS;}
static void open_sound_settings(void){current_screen=SCREEN_SOUND_SETTINGS;}
static void open_units_settings(void){current_screen=SCREEN_UNITS;}
static void open_telemetry_settings(void){current_screen=SCREEN_TELEMETRY_SETTINGS;}
static void open_key_backup(void){current_screen=SCREEN_KEY_BACKUP;}
static void open_config_backup(void){current_screen=SCREEN_CONFIG_BACKUP;}
static bool history_settings_navigation(int key){return false;}
static void open_history_settings(void){current_screen=SCREEN_HISTORY_SETTINGS;}
static void open_channel_alerts(void){current_screen=SCREEN_CHANNEL_ALERTS;}
static void open_time_settings(void){current_screen=SCREEN_TIME_SETTINGS;}
static void open_role_settings(void){current_screen=SCREEN_ROLE_SETTINGS;}
'''+menu+r'''
static bool menu_navigation(int key){return settings_menu_navigation(key);}
static void check_menu(void){
 meshdeck_screen_t expected[]={SCREEN_ABOUT,SCREEN_NODE_SETTINGS_MENU,SCREEN_CHANNELS,SCREEN_CHANNEL_IMPORT,SCREEN_MANUAL_CHANNEL,SCREEN_DISPLAY_SETTINGS_MENU,SCREEN_POWER_SETTINGS,SCREEN_FIRST_USE};
 for(unsigned i=0;i<8;i++){
  current_screen=SCREEN_SETTINGS_MENU;settings_menu_selection=i;
  assert(menu_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN));assert(current_screen==expected[i]);
  current_screen=SCREEN_SETTINGS_MENU;assert(menu_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT));assert(current_screen==expected[i]);
 }
 current_screen=SCREEN_SETTINGS_MENU;settings_menu_selection=0;
 menu_navigation(BSP_INPUT_NAVIGATION_KEY_UP);assert(settings_menu_selection==0);
 for(int i=0;i<20;i++)menu_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);
 assert(settings_menu_selection==7&&owner_requests==2);
 meshdeck_screen_t node_expected[]={SCREEN_SETTINGS,SCREEN_ROLE_SETTINGS,SCREEN_RADIO_SETTINGS,SCREEN_POSITION_SETTINGS,SCREEN_TELEMETRY_SETTINGS,SCREEN_CONFIG_BACKUP,SCREEN_RANGE_TEST,SCREEN_SECURITY,SCREEN_RADIO_BEHAVIOR,SCREEN_NEIGHBOR_INFO};
 for(unsigned i=0;i<10;i++) for(unsigned right=0;right<2;right++) {
  current_screen=SCREEN_NODE_SETTINGS_MENU;node_settings_selection=i;
  assert(menu_navigation(right?BSP_INPUT_NAVIGATION_KEY_RIGHT:BSP_INPUT_NAVIGATION_KEY_RETURN));assert(current_screen==node_expected[i]);
 }
 meshdeck_screen_t display_expected[]={SCREEN_DISPLAY_ALERTS,SCREEN_CHANNEL_ALERTS,SCREEN_TEXT_SETTINGS,SCREEN_TIME_SETTINGS,SCREEN_HISTORY_SETTINGS,SCREEN_ARCHIVE_SETTINGS,SCREEN_SOUND_SETTINGS,SCREEN_UNITS};
 for(unsigned i=0;i<8;i++) for(unsigned right=0;right<2;right++) {
  current_screen=SCREEN_DISPLAY_SETTINGS_MENU;display_settings_selection=i;
  assert(menu_navigation(right?BSP_INPUT_NAVIGATION_KEY_RIGHT:BSP_INPUT_NAVIGATION_KEY_RETURN));assert(current_screen==display_expected[i]);
 }
 current_screen=SCREEN_NODE_SETTINGS_MENU;node_settings_selection=0;menu_navigation(BSP_INPUT_NAVIGATION_KEY_UP);assert(!node_settings_selection);
 for(unsigned i=0;i<10;i++)menu_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);assert(node_settings_selection==9);
 assert(!menu_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT)&&!menu_navigation(BSP_INPUT_NAVIGATION_KEY_ESC));
 current_screen=SCREEN_DISPLAY_SETTINGS_MENU;display_settings_selection=0;menu_navigation(BSP_INPUT_NAVIGATION_KEY_UP);assert(!display_settings_selection);
 for(unsigned i=0;i<10;i++)menu_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);assert(display_settings_selection==7);
 assert(!menu_navigation(BSP_INPUT_NAVIGATION_KEY_ESC));
}
'''
harness=harness.replace('int main(void){',extra+'\nint main(void){ check_menu();refreshes=0;')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(harness)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'tests/stubs'),'-I',str(root/'main'),str(p/'test.c'),str(root/'main/text_settings.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
