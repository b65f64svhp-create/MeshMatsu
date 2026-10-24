from ui_source import read_main_ui
"""Exercise real configuration handlers and radio dumps with deferred worker results."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];main=read_main_ui(r)
enum=main[main.index('typedef enum {'):main.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
fixture=(r/'tests/test_config_radio.c').read_text();fixture=fixture[fixture.index('static mesh_config_backup_t backup;'):fixture.index('int main(void)')]
h='#define NODE_ALIASES_PATH "aliases.bin"\n#define NODE_PREFS_PATH "prefs.bin"\n#define main backend_tests\n#include "'+str(r/'tests/test_direct_messages.c')+'"\n#undef main\n'+enum+'\n'+fixture
keys=['UP','DOWN','LEFT','RIGHT','RETURN','ESC','F1','F2','F3','F4','F5','F6'];h+=''.join(f'#define BSP_INPUT_NAVIGATION_KEY_{k} {i+1}\n' for i,k in enumerate(keys))
h+=r'''
static meshdeck_screen_t current_screen=SCREEN_HOME;
static bool display_and_lights_off,ui_dirty;
static int64_t last_activity_us;
static bool pending,available,apply_ok=true;
static unsigned app_apply_count,app_value;
static mesh_cfg_result_t completion;
static mesh_config_backup_t captured,saved;
static mesh_cfg_job_t last_job;
static char captured_password[97];
bool mesh_keys_identity_valid(const uint8_t *a,const uint8_t *b){return true;}
static void open_key_backup(void){current_screen=SCREEN_KEY_BACKUP;}
bool mesh_config_app_capture(uint8_t *out,size_t *size){assert(*size>=4);out[0]=1;out[1]=app_value;out[2]=out[3]=0;*size=4;return true;}
bool mesh_config_app_apply(const uint8_t *data,size_t size){app_apply_count++;return apply_ok;}
bool mesh_config_store_start(mesh_cfg_job_t job,const mesh_config_backup_t *a,const char *password,const char *path,bool safety){if(pending)return false;pending=true;last_job=job;if(a)captured=*a;if(password)snprintf(captured_password,97,"%s",password);return true;}
bool mesh_config_store_take(mesh_cfg_result_t *out){if(!available)return false;*out=completion;memset(&completion,0,sizeof(completion));pending=available=false;return true;}
'''
h+='#include "'+str(r/'main/config_backup_ui.inc')+'"\n'
h+=r'''
static void finish(mesh_cfg_job_t job){completion=(mesh_cfg_result_t){.job=job,.ok=true};strcpy(completion.path,"config-6b49d200-12345678-abcd1234.mmc");available=true;}
static void ready(void){setup();cf_active=false;pending=available=false;display_and_lights_off=false;last_activity_us=fake_time;current_screen=SCREEN_HOME;app_value=0;app_apply_count=0;}
static void read_complete(void){dump(false,false);complete(config_nonce);cf_tick();}
static void create(void){open_config_backup();config_backup_key('\r');assert(cf_mode==CF_PASS);strcpy(cf_password,"a long test passphrase");strcpy(cf_repeat,cf_password);cf_field=2;config_backup_key('\r');assert(cf_mode==CF_READ_SAVE);read_complete();assert(last_job==CFG_JOB_SAVE&&pending&&captured.count==35&&cf_mode==CF_WORK&&!cf_password[0]&&!cf_original.count);saved=captured;finish(CFG_JOB_SAVE);cf_tick();assert(cf_mode==CF_DONE&&!cf_saved.count&&strstr(cf_feedback,"saved and verified"));}
static void unlock_app(void){
 open_config_backup();cf_selection=1;config_backup_key('\r');assert(last_job==CFG_JOB_LIST);finish(CFG_JOB_LIST);completion.count=1;strcpy(completion.files[0],"config-6b49d200-12345678-abcd1234.mmc");cf_tick();config_backup_key('\r');strcpy(cf_password,"a long test passphrase");cf_field=1;config_backup_key('\r');assert(cf_mode==CF_WORK);finish(CFG_JOB_OPEN);completion.archive=saved;cf_tick();assert(cf_mode==CF_SELECT);
 cf_selection=mesh_config_find(&cf_saved,CFG_APP,0)-cf_saved.records;config_backup_key('\r');assert(cf_mode==CF_READ_REVIEW);read_complete();assert(cf_mode==CF_REVIEW&&!cf_confirm);
}
int main(void){
 ready();create();assert(!strcmp(captured_password,"a long test passphrase"));
 unlock_app();size_t writes=write_count;config_backup_key('\r');assert(cf_mode==CF_MENU&&!cf_password[0]&&!cf_saved.count&&write_count==writes&&!app_apply_count);
 unlock_app();writes=write_count;config_backup_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);config_backup_key('\r');assert(cf_mode==CF_SAFETY&&pending&&captured.count==35&&write_count==writes);
 finish(CFG_JOB_SAVE);cf_tick();assert(cf_mode==CF_BEFORE_APPLY&&write_count==writes+1);read_complete();assert(cf_mode==CF_DONE&&app_apply_count==1&&strstr(cf_feedback,"Restart MeshMatsu")&&!cf_password[0]);
 unlock_app();cf_confirm=true;config_backup_key('\r');finish(CFG_JOB_SAVE);cf_tick();app_value++;read_complete();assert(cf_mode==CF_DONE&&app_apply_count==1&&strstr(cf_feedback,"changed during review"));app_value=0;
 // Sleep or a cancelled job must never reopen an unlocked backup or send a restore.
 open_config_backup();cf_selection=1;config_backup_key('\r');cf_close();current_screen=SCREEN_HOME;finish(CFG_JOB_OPEN);completion.archive=saved;cf_tick();assert(!cf_saved.count&&!cf_active);
 open_config_backup();cf_mode=CF_PASS;strcpy(cf_password,"a long test passphrase");display_and_lights_off=true;cf_tick();assert(!cf_active&&!cf_password[0]);display_and_lights_off=false;
 open_config_backup();cf_mode=CF_PASS;strcpy(cf_password,"a long test passphrase");fake_time=last_activity_us+120000001;cf_tick();assert(cf_mode==CF_DONE&&!cf_password[0]);last_activity_us=fake_time;
 open_config_backup();cf_selection=2;config_backup_key('\r');assert(current_screen==SCREEN_KEY_BACKUP&&!cf_active);
 puts("PASS: actual full backup/password UI, 35-section snapshot, encrypted-worker ownership/wiping, default Cancel, whole safety snapshot before mutation, fresh baseline guard, local restore/readback, sleep/timeout/cancelled-job locks and legacy key menu");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(r/'tests/stubs'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
