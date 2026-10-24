"""Actual range controller, transport, counter and host export integration."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'main/main.c').read_text();enum=s[s.index('typedef enum {'):s.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
h='#define RANGE_RADIO_TEST_MAIN previous_range_tests\n#include "'+str(r/'tests/test_range_radio.c')+'"\n'+enum+'\n'
keys=['UP','DOWN','LEFT','RIGHT','RETURN','ESC','F1','F2','F3','F4','F6'];h+=''.join(f'#define BSP_INPUT_NAVIGATION_KEY_{k} {i+1}\n' for i,k in enumerate(keys))
h+=r'''
#define MESH_EXPORT_ROOT "range-ui-export"
#include "range_export.c"
static meshdeck_screen_t current_screen=SCREEN_NODES;static unsigned node_modal=1;enum {NODE_ACTIONS=1};
static void go_back(void){current_screen=SCREEN_NODES;}
#include "range_ui.inc"
int main(void){
 range_setup();size_t writes=write_count;open_range_testing(peer);assert(current_screen==SCREEN_RANGE_TEST&&rt_mode==MESH_RANGE_RECEIVE&&rt_peer==peer&&range_native.pending&&write_count==writes+1);
 native_reply(range_native_id,false,0);writes=write_count;rt_field=4;range_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(rt_review==1&&!rt_confirm);
 range_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!rt_review&&range_status.state==MESH_RANGE_STOPPED&&write_count==writes);
 range_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);range_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);range_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!rt_review&&range_status.state==MESH_RANGE_ACTIVE&&range_status.peer==peer&&write_count==writes);
 range_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!rt_review&&strstr(rt_feedback,"active"));rt_field=0;range_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(rt_mode==MESH_RANGE_RECEIVE&&strstr(rt_feedback,"Stop"));
 range_packet(peer,22,1,0,false,0,true);assert(range_status.counter.received==1);range_key('s');assert(range_status.state==MESH_RANGE_STOPPED&&range_status.records==1);
 rt_field=0;range_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(rt_mode==MESH_RANGE_SEND);rt_field=4;range_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);range_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);range_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(range_status.state==MESH_RANGE_ACTIVE&&range_status.mode==MESH_RANGE_SEND&&write_count==writes);
 fake_time+=5000000;range_poll(fake_time);assert(range_status.queued==1&&write_count==writes+1);
 range_key('e');assert(rt_review==2&&!rt_confirm&&!mesh_range_export_busy());range_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!rt_review&&!mesh_range_export_busy());
 range_key('e');range_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);range_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!rt_review&&mesh_range_export_busy());mesh_range_export_poll();assert(mesh_range_export_status().success&&mesh_range_export_status().rows==1);
 assert(!range_navigation(BSP_INPUT_NAVIGATION_KEY_F6));range_navigation(BSP_INPUT_NAVIGATION_KEY_ESC);assert(current_screen==SCREEN_NODES&&node_modal==NODE_ACTIONS&&range_status.state==MESH_RANGE_ACTIVE); // leaving page does not stop timed background test
 mesh_radio_range_stop();current_screen=SCREEN_HOME;assert(!range_key('e')&&!range_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN));
 puts("PASS: receive/send defaults, reviewed start/cancel, active edit and duplicate Enter guards, explicit stop, plaintext export review, back/shape shortcuts and background test continuity");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(r/'tests/stubs'),'-I',str(r/'main'),str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
