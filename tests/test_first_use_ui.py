"""Exercise the setup guide against the actual local-radio transaction backend."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'main/main.c').read_text()
enum=s[s.index('typedef enum {'):s.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
options=s[s.index('typedef struct { uint32_t value; const char *name; } radio_option_t;'):s.index('static const radio_option_t preset_options')]
step=s[s.index('static uint32_t step_radio_option('):s.index('static void radio_option_name(')]
h='#define FIRST_RADIO_TEST_MAIN earlier_first_radio_tests\n#include "'+str(r/'tests/test_first_use_radio.c')+'"\n#include "composer_view.h"\n'+enum+options+step
for i,key in enumerate(['UP','DOWN','LEFT','RIGHT','RETURN','ESC','F1','F2','F3','F4','F5','F6','BACKSPACE'],1):h+=f'#define BSP_INPUT_NAVIGATION_KEY_{key} {i}\n'
h+=r'''
static meshdeck_screen_t current_screen=SCREEN_HOME;static unsigned completions;static int store_fail;
static void go_home(void){current_screen=SCREEN_HOME;}
static void go_back(void){current_screen=SCREEN_SETTINGS_MENU;}
esp_err_t mesh_first_use_complete(void){completions++;return store_fail;}
#include "first_use_ui.inc"
static void enter(void){assert(first_use_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN));}
static void connected(void){
 first_radio_reset();open_first_use(true);assert(current_screen==SCREEN_FIRST_USE&&first_step==FIRST_WELCOME&&!first_pending);enter();assert(first_step==FIRST_CONNECT);
 first_use_poll();assert(first_read_attempted&&setup_radio.state==MESH_SETUP_READING);
 first_reply(0,setup_owner,setup_owner_n);first_reply(1,setup_lora,setup_lora_n);first_use_poll();assert(first_step==FIRST_REGION&&first_region==1&&!strcmp(first_name,"Existing Radio"));
}
int main(void){
 first_radio_reset();size_t writes=write_count;open_first_use(true);first_use_poll();assert(write_count==writes);first_field=1;enter();assert(current_screen==SCREEN_HOME&&!completions&&write_count==writes);
 open_first_use(false);first_use_navigation(BSP_INPUT_NAVIGATION_KEY_ESC);assert(current_screen==SCREEN_SETTINGS_MENU&&!completions);
 connected();writes=write_count;first_field=1;enter();assert(first_step==FIRST_NAMES&&write_count==writes);first_field=2;enter();assert(first_pending==3&&write_count==writes+1);
 first_reply(0,setup_owner,setup_owner_n);first_reply(1,setup_lora,setup_lora_n);first_use_poll();assert(first_step==FIRST_COMPLETE&&first_completed&&completions==1&&write_count==writes+2);enter();assert(current_screen==SCREEN_HOME);
 connected();first_field=0;first_use_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(first_region==3);first_field=1;writes=write_count;enter();assert(first_review&&!first_confirm&&write_count==writes);enter();assert(!first_review&&write_count==writes);
 enter();first_use_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);enter();assert(first_pending==1&&setup_radio.state==MESH_SETUP_CHECKING);first_reply(1,setup_lora,setup_lora_n);assert(setup_radio.state==MESH_SETUP_SAVING);
 uint8_t desired_region[224];size_t rn=setup_op.wanted_size;memcpy(desired_region,setup_op.wanted,rn);fake_time+=8000000;setup_poll(fake_time);first_reply(1,desired_region,rn);first_use_poll();assert(first_step==FIRST_NAMES&&first_baseline.region==3);
 first_field=0;strcpy(first_name,"");assert(first_use_key('N')&&first_use_key('e')&&first_use_key('w'));first_field=1;strcpy(first_short,"N");first_field=2;enter();assert(first_review&&!first_confirm);first_use_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);enter();assert(first_pending==2);
 first_reply(0,setup_owner,setup_owner_n);uint8_t desired_owner[224];size_t on=setup_op.wanted_size;memcpy(desired_owner,setup_op.wanted,on);fake_time+=8000000;setup_poll(fake_time);first_reply(0,desired_owner,on);first_use_poll();assert(first_pending==3);first_reply(0,desired_owner,on);first_reply(1,desired_region,rn);store_fail=9;first_use_poll();assert(first_step==FIRST_COMPLETE&&!first_completed&&strstr(first_feedback,"could not be saved"));
 writes=write_count;enter();assert(first_pending==3&&first_step==FIRST_NAMES);first_reply(0,desired_owner,on);first_reply(1,desired_region,rn);store_fail=0;first_use_poll();assert(first_completed&&first_step==FIRST_COMPLETE&&write_count==writes+2);
 connected();first_field=1;enter();first_field=2;writes=write_count;enter();first_reply(0,setup_owner,setup_owner_n);setup_lora[5]=3;first_reply(1,setup_lora,setup_lora_n);first_use_poll();assert(first_step==FIRST_NAMES&&!first_pending&&strstr(first_feedback,"not confirmed")&&write_count==writes+2); // foreign region cannot mark complete
 connected();first_field=0;strcpy(first_name,"ABC");first_step=FIRST_NAMES;first_use_key('\b');assert(!strcmp(first_name,"AB"));first_use_key('\t');assert(first_field==1);strcpy(first_short,"1234");first_use_key('5');assert(!strcmp(first_short,"1234"));
 assert(!first_use_navigation(BSP_INPUT_NAVIGATION_KEY_F1)&&!first_use_navigation(BSP_INPUT_NAVIGATION_KEY_F5)&&!first_use_navigation(BSP_INPUT_NAVIGATION_KEY_F6));
 first_use_navigation(BSP_INPUT_NAVIGATION_KEY_ESC);assert(first_step==FIRST_REGION);status.reconnecting=true;first_use_poll();assert(first_step==FIRST_CONNECT&&!first_review);status.reconnecting=false;
 current_screen=SCREEN_HOME;assert(!first_use_key('x')&&!first_use_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN));
 puts("PASS: actual setup welcome/later/reopen, confirmed connection and prefill, unchanged values read-only, explicit region/name review+save, final two-field verification, persistence failure/retry, editing/limits, Esc/shortcuts and radio-change recovery");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(r/'tests/stubs'),'-I',str(r/'main'),str(p/'test.c'),str(r/'main/composer_view.c'),str(r/'main/emoji_text.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
