"""Production neighbor report controls and cancel-first local settings review."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'main/main.c').read_text();enum=s[s.index('typedef enum {'):s.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
h='#define NB_CONFIG_TEST_MAIN prior_neighbor_configuration_tests\n#include "'+str(r/'tests/test_neighbor_config.c')+'"\n'+enum+'\n'
for i,key in enumerate(['UP','DOWN','LEFT','RIGHT','RETURN','ESC','F1','F2','F3','F4','F5','F6'],1):h+=f'#define BSP_INPUT_NAVIGATION_KEY_{key} {i}\n'
h+=r'''
static meshdeck_screen_t current_screen=SCREEN_NODES;static unsigned node_modal=1;enum {NODE_ACTIONS=1};
static void go_back(void){current_screen=current_screen==SCREEN_NEIGHBOR_SETTINGS?SCREEN_NEIGHBOR_INFO:SCREEN_NODES;}
#include "neighbor_ui.inc"
static void nav(int key){assert(neighbor_navigation(key));}
static void enter(void){nav(BSP_INPUT_NAVIGATION_KEY_RETURN);}
int main(void){
 nb_config_setup();nodes[0]=(mesh_node_t){.number=peer};node_count=1;
 size_t writes=write_count;open_neighbor_info(peer);assert(current_screen==SCREEN_NEIGHBOR_INFO&&neighbor_target==peer&&neighbor_from_node&&!neighbor_button&&write_count==writes);
 enter();assert(nb_request.state==MESH_NB_WAITING&&write_count==writes+1&&!neighbor_feedback[0]);enter();assert(write_count==writes+1&&strstr(neighbor_feedback,"already waiting"));
 uint8_t payload[233];size_t n=nb_payload(payload,peer,10,-2.5);nb_packet(peer,local_node_number,1,nb_request.id,payload,n,false,1800000000);
 enter();assert(strstr(neighbor_feedback,"Wait")&&write_count==writes+1); // cooldown remains visible even after a prior reply
 for(unsigned i=0;i<20;i++)nav(BSP_INPUT_NAVIGATION_KEY_DOWN);assert(neighbor_scroll==5);nav(BSP_INPUT_NAVIGATION_KEY_UP);assert(neighbor_scroll==4);
 nav(BSP_INPUT_NAVIGATION_KEY_RIGHT);enter();assert(current_screen==SCREEN_NEIGHBOR_SETTINGS&&nb_config.state==MESH_NBC_READING);
 uint8_t raw[180];n=nb_config_bytes(raw,false,0,false,0);nb_config_reply(nb_op.id,raw,n,true,local_node_number,0);neighbor_ui_poll();assert(neighbor_config_loaded&&!neighbor_draft.enabled);
 nav(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(neighbor_draft.enabled&&neighbor_config_edited&&!nb_config.enabled);
 neighbor_config_field=1;nav(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(neighbor_draft.interval==14400);
 neighbor_config_field=2;channels[0].info.default_key=true;nav(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(!neighbor_draft.over_lora&&strstr(neighbor_feedback,"private key"));channels[0].info.default_key=false;
 nav(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(neighbor_draft.over_lora);
 neighbor_config_field=3;writes=write_count;enter();assert(neighbor_review&&!neighbor_confirm&&write_count==writes);enter();assert(!neighbor_review&&write_count==writes); // cancel-first
 enter();nav(BSP_INPUT_NAVIGATION_KEY_RIGHT);enter();assert(nb_config.state==MESH_NBC_CHECKING&&neighbor_config_pending&&write_count==writes+1);
 nb_primary_reply(true);nb_config_reply(nb_op.id,raw,n,true,local_node_number,0);fake_time+=8000000;neighbor_poll(fake_time);n=nb_config_bytes(raw,true,14400,true,0);nb_config_reply(nb_op.id,raw,n,true,local_node_number,0);neighbor_ui_poll();assert(nb_config.state==MESH_NBC_SAVED&&!neighbor_config_pending&&!neighbor_config_edited&&neighbor_draft.revision==nb_config.revision);
 neighbor_config_field=0;nav(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(!neighbor_draft.enabled&&!neighbor_draft.over_lora&&neighbor_config_edited);neighbor_ui_poll();assert(neighbor_config_edited&&!neighbor_draft.enabled); // saved state cannot discard new edits
 neighbor_config_field=4;enter();assert(nb_config.state==MESH_NBC_READING&&!neighbor_config_loaded&&!neighbor_config_edited);nb_config_reply(nb_op.id,raw,n,true,local_node_number,0);neighbor_ui_poll();assert(neighbor_draft.enabled&&neighbor_draft.over_lora);
 nav(BSP_INPUT_NAVIGATION_KEY_ESC);assert(current_screen==SCREEN_NEIGHBOR_INFO);nav(BSP_INPUT_NAVIGATION_KEY_ESC);assert(current_screen==SCREEN_NODES&&node_modal==NODE_ACTIONS);
 open_neighbor_info(0);assert(neighbor_target==local_node_number);assert(!neighbor_navigation(BSP_INPUT_NAVIGATION_KEY_F1)&&!neighbor_navigation(BSP_INPUT_NAVIGATION_KEY_F5)&&!neighbor_navigation(BSP_INPUT_NAVIGATION_KEY_F6));
 current_screen=SCREEN_HOME;assert(!neighbor_key('x')&&!neighbor_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN));
 assert(!message_count&&!message_alerts);
 puts("PASS: frozen selected-node view, bounded scrolling, explicit single requests/cooldown feedback, local-only settings, private-channel guard, cancel-first Save, verified draft refresh, edit preservation/reload, Esc and global shortcuts");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(r/'tests/stubs'),'-I',str(r/'main'),str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
