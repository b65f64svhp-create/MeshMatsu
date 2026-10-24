from ui_source import read_main_ui
"""Exercise the actual radio editor against the real protocol/backend test transport."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=read_main_ui(r);enum=s[s.index('typedef enum {'):s.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
harness='#define ROLE_TEST_MAIN previous_role_tests\n#include "'+str(r/'tests/test_role.c')+'"\n'+enum+r'''
#define BSP_INPUT_NAVIGATION_KEY_UP 1
#define BSP_INPUT_NAVIGATION_KEY_DOWN 2
#define BSP_INPUT_NAVIGATION_KEY_LEFT 3
#define BSP_INPUT_NAVIGATION_KEY_RIGHT 4
#define BSP_INPUT_NAVIGATION_KEY_RETURN 5
static meshdeck_screen_t current_screen;
#include "radio_behavior_ui.inc"
int main(void){
 ready();open_behavior_settings();assert(current_screen==SCREEN_RADIO_BEHAVIOR&&!behavior_loaded);behavior_refresh();assert(behavior_loaded&&behavior_mode_draft==0&&behavior_interval_draft==0);
 behavior_field=2;size_t writes=write_count;behavior_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(write_count==writes&&strstr(behavior_feedback,"No changes"));
 behavior_field=0;behavior_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(behavior_interval_draft==10800&&behavior_edited);behavior_field=1;behavior_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(behavior_mode_draft==2); // skips repeater-only decoding
 behavior_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(behavior_mode_draft==3);behavior_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(behavior_mode_draft==5); // skips NONE on Client
 assert(!behavior_navigation(99));behavior_key('\t');assert(behavior_field==2);writes=write_count;behavior_key('\r');assert(behavior_pending&&role_save.state==MESH_ROLE_SAVING&&write_count==writes+1);
 behavior_key('\r');assert(write_count==writes+1);behavior_field=0;behavior_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);assert(behavior_interval_draft==10800);
 role_save.state=MESH_ROLE_FAILED;behavior_refresh();assert(!behavior_pending&&behavior_edited&&strstr(behavior_feedback,"Not confirmed"));
 ready();open_behavior_settings();behavior_refresh();behavior_field=0;behavior_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);uint32_t draft=behavior_interval_draft;config(1);behavior_refresh();assert(behavior_interval_draft==draft&&behavior_edited);behavior_field=2;writes=write_count;behavior_key('\n');assert(!behavior_pending&&strstr(behavior_feedback,"changed")&&write_count==writes);
 open_behavior_settings();behavior_refresh();assert(!behavior_edited&&behavior_baseline.role==1&&behavior_interval_draft==0);
 current_screen=SCREEN_HOME;assert(!behavior_key('x')&&!behavior_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT));
 puts("PASS: real editor drafts/cancel/reload, role-aware cycling, explicit Save, duplicate Return protection, pending edit guard, stale revision rejection and failed-readback feedback");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(harness)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(r/'tests/stubs'),'-I',str(r/'main'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
