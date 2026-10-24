"""Exercise production review/navigation against the real fake-radio backend."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'main/main.c').read_text();enum=s[s.index('typedef enum {'):s.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
h='#define SECURITY_TEST_MAIN earlier_security_tests\n#include "'+str(r/'tests/test_security_admin.c')+'"\n'+enum+'\n'
keys=['UP','DOWN','LEFT','RIGHT','RETURN','ESC','F1','F2','F3','F4','F6']
h+=''.join(f'#define BSP_INPUT_NAVIGATION_KEY_{k} {i+1}\n' for i,k in enumerate(keys))
h+=r'''
#include "third_party/qrcodegen.h"
#include "role_options.h"
#include "composer_view.h"
static meshdeck_screen_t current_screen=SCREEN_NODES;
static unsigned node_modal=1;
enum {NODE_ACTIONS=1};
static void go_back(void){current_screen=SCREEN_NODES;}
#include "management_ui.inc"
int main(void){
 setup();open_security_centre(peer);size_t writes=write_count;
 assert(current_screen==SCREEN_SECURITY&&sc_mode==SC_MENU&&writes==write_count);
 security_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(sc_mode==SC_QR&&sc_qr_valid&&write_count==writes);
 security_navigation(BSP_INPUT_NAVIGATION_KEY_ESC);sc_selection=1;security_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(sc_mode==SC_COMPARE);
 security_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(sc_mode==SC_REVIEW&&!sc_confirm&&write_count==writes);
 security_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(sc_mode==SC_MENU&&mesh_radio_contact_trust(peer,nodes[0].public_key,NULL)!=MESH_TRUST_VERIFIED);
 sc_selection=1;security_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);security_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);security_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);security_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(sc_mode==SC_COMPARE&&mesh_radio_contact_trust(peer,nodes[0].public_key,NULL)==MESH_TRUST_VERIFIED&&write_count==writes);
 security_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);nodes[0].public_key[0]++;security_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);security_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(sc_mode==SC_REVIEW&&strstr(sc_feedback,"Not applied"));nodes[0].public_key[0]--;
 security_navigation(BSP_INPUT_NAVIGATION_KEY_ESC);security_navigation(BSP_INPUT_NAVIGATION_KEY_ESC);assert(current_screen==SCREEN_NODES&&node_modal==NODE_ACTIONS);
 open_security_centre(peer);sc_selection=3;security_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(sc_mode==SC_ADMINS&&security_state.state==MESH_MANAGE_READING);
 sec_fixture(1);sec_response(security_op.id);sc_slot=0;management_key('d');assert(sc_mode==SC_REVIEW&&sc_action==SC_ADMIN_REMOVE&&!sc_confirm);
 writes=write_count;security_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);security_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(sc_mode==SC_WAIT&&security_state.state==MESH_MANAGE_CHECKING&&write_count==writes+1);
 security_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(write_count==writes+1);sec_response(security_op.id);assert(security_state.state==MESH_MANAGE_SAVING&&write_count==writes+2);
 setup();assert(mesh_radio_verify_contact(peer,nodes[0].public_key));current_screen=SCREEN_NODES;open_remote_manager(peer);
 for(unsigned i=0;i<4;i++)response(i,remote_op.id,true,peer,local_node_number,"Remote",i==2?0:3);
 remote_refresh_ui();assert(rm_loaded&&!strcmp(rm_name,"Remote"));rm_field=1;management_key('N');management_key('e');management_key('w');assert(!strcmp(rm_name,"New"));
 management_key('\b');assert(!strcmp(rm_name,"Ne"));rm_field=4;writes=write_count;remote_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(rm_review&&!rm_confirm&&write_count==writes);
 remote_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!rm_review&&write_count==writes);
 remote_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);remote_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);remote_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!rm_review&&!rm_loaded&&remote.state==MESH_MANAGE_CHECKING&&write_count==writes+1);
 remote_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(write_count==writes+1);response(0,remote_op.id,true,peer,local_node_number,"Remote",0);assert(remote.state==MESH_MANAGE_SAVING&&write_count==writes+2);
 assert(!remote_navigation(BSP_INPUT_NAVIGATION_KEY_F6));remote_navigation(BSP_INPUT_NAVIGATION_KEY_ESC);assert(current_screen==SCREEN_NODES&&node_modal==NODE_ACTIONS);
 current_screen=SCREEN_HOME;assert(!management_key('A')&&!management_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN));
 puts("PASS: actual contact QR/review/verify UI, cancel default/no radio side effect, frozen-key recheck, local admin deletion review, pending duplicate Enter guards, remote name edits/review/preflight, Back to node Actions and screen shortcut handoff");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 sources=['third_party/qrcodegen.c','role_options.c','composer_view.c','emoji_text.c']
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(r/'tests/stubs'),'-I',str(r/'main'),str(p/'test.c'),*[str(r/'main'/s) for s in sources],'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
