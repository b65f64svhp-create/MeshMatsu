from ui_source import read_main_ui
"""Production location form navigation, editing and radio-write boundaries."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=read_main_ui(root);enum=s[s.index('typedef enum {'):s.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
h='#define main backend_tests\n#include "'+str(root/'tests/test_direct_messages.c')+'"\n#undef main\n#include "'+str(root/'main/location_utils.c')+'"\n'+enum+'\n'
for i,k in enumerate(['UP','DOWN','LEFT','RIGHT','RETURN','ESC','F1','F2','F3','F4','F5','F6']):h+=f'#define BSP_INPUT_NAVIGATION_KEY_{k} {i+1}\n'
h+=r'''
static meshdeck_screen_t current_screen=SCREEN_HOME;
static size_t channel_selection;
'''+(root/'main/location_ui.inc').read_text()+r'''
int main(void){
 reset();session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;
 const uint8_t raw[]={8,0,16,1};decode_position_config(raw,sizeof(raw));
 size_t writes=write_count;open_position_settings();assert(current_screen==SCREEN_POSITION_SETTINGS&&position_loaded);
 writes=write_count;position_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(position_edited&&position_draft.broadcast_secs==300&&write_count==writes);
 assert(!position_navigation(BSP_INPUT_NAVIGATION_KEY_ESC)&&!position_navigation(BSP_INPUT_NAVIGATION_KEY_F6));
 position_field=4;position_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(current_screen==SCREEN_CHANNELS&&write_count==writes);
 open_position_settings();position_field=3;position_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(current_screen==SCREEN_FIXED_POSITION);
 writes=write_count;assert(!fixed_lat[0]&&!fixed_lon[0]);
 position_key('5');position_key('5');position_key('.');position_key('1');assert(!strcmp(fixed_lat,"55.1")&&write_count==writes);
 position_key('\b');assert(!strcmp(fixed_lat,"55."));position_key('2');position_key('\t');
 const char *lon="-118.8";for(const char *p=lon;*p;p++)position_key(*p);
 assert(!strcmp(fixed_lon,lon));position_key('\t');assert(fixed_field==2);
 position_key('\r');assert(fixed_pending&&location_save.state==MESH_LOCATION_SAVING&&write_count==writes+1);
 position_key('\r');assert(write_count==writes+1);position_key('9');assert(!strcmp(fixed_lon,lon));
 reset();decode_position_config(raw,sizeof(raw));open_fixed_settings();strcpy(fixed_lat,"nan");strcpy(fixed_lon,"2");fixed_field=2;
 writes=write_count;position_key('\r');assert(write_count==writes&&strstr(fixed_feedback,"latitude"));
 strcpy(fixed_lat,"91");position_key('\r');assert(write_count==writes);
 // A new form seeds its fields from a real position; first typing replaces it.
 nodes[0]=(mesh_node_t){.number=local_node_number,.position_available=true,.latitude_i=551700000,.longitude_i=-1188000000};node_count=1;
 open_fixed_settings();assert(!strcmp(fixed_lat,"55.1700000"));position_key('4');assert(!strcmp(fixed_lat,"4"));
 assert(!message_count&&!reaction_count);
 puts("PASS: position navigation, fixed coordinate draft/edit/delete, validation, explicit save, pending-write guard and no accidental chat sends");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'tests/stubs'),'-I',str(root/'main'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
