from ui_source import read_main_ui
"""Actual key screen handlers with real radio backend and controlled SD worker completion."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];main=read_main_ui(root)
enum=main[main.index('typedef enum {'):main.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
keys=['UP','DOWN','LEFT','RIGHT','RETURN','ESC','F1','F2','F3','F4','F5','F6']
h='#define main earlier_tests\n#include "'+str(root/'tests/test_direct_messages.c')+'"\n#undef main\n'+enum+'\n'
h+=''.join(f'#define BSP_INPUT_NAVIGATION_KEY_{k} {i+1}\n' for i,k in enumerate(keys))
h+=r'''
static meshdeck_screen_t current_screen;
static bool ui_dirty,display_and_lights_off;
static int64_t last_activity_us;
static bool pending,available;
static mesh_key_result_t completion;
static mesh_key_job_t last_job;
static mesh_key_backup_t captured;
static char captured_password[97];
bool mesh_keys_fingerprint(const uint8_t *key,char out[17]){strcpy(out,"0123456789ABCDEF");return true;}
bool mesh_key_store_start(mesh_key_job_t job,const mesh_key_backup_t *backup,const char *password,const char *path,bool safety){
 if(pending)return false;pending=true;last_job=job;if(backup)captured=*backup;if(password)strcpy(captured_password,password);return true;
}
bool mesh_key_store_take(mesh_key_result_t *out){if(!available)return false;*out=completion;memset(&completion,0,sizeof(completion));available=pending=false;return true;}
'''
h+='#include "'+str(root/'main/key_backup_ui.inc')+'"\n'
h+=r'''
static void ready(void){
 reset();keys_state=MESH_KEYS_IDLE;mesh_radio_keys_clear();pending=available=false;current_screen=SCREEN_HOME;bk_active=false;display_and_lights_off=false;last_activity_us=fake_time;
 session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;
 for(uint8_t i=0;i<8;i++){uint8_t ch[]={8,i,18,7,18,1,1,26,2,'B','L',24,i==0?1:i==1?2:0};decode_channel(ch,sizeof(ch));}
}
static void read_ready(void){status.synchronized=true;config_request_pending=false;keys_state=MESH_KEYS_READY;keys_origin=local_node_number;keys_loaded=keys_have_identity=true;keys_loaded_us=fake_time;keys_revision++;memset(keys_public,3,32);memset(keys_private,4,32);keys_raw_size=0;}
static void complete(mesh_key_job_t job){completion=(mesh_key_result_t){.job=job,.ok=true};strcpy(completion.path,"keys-6b49d200-12345678-abcd1234.mmk");available=true;}
static void type(const char *text){while(*text)key_backup_key(*text++);}
static void unlock(void){
 open_key_backup();bk_selection=3;key_backup_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(last_job==MESH_KEY_JOB_LIST&&bk_mode==BK_WORK);
 complete(MESH_KEY_JOB_LIST);completion.count=1;strcpy(completion.files[0],"keys-6b49d200-12345678-abcd1234.mmk");bk_tick();assert(bk_mode==BK_LIST);
 key_backup_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(bk_mode==BK_PASS);
 type("a long test passphrase");bk_field=1;key_backup_key('\r');assert(last_job==MESH_KEY_JOB_OPEN&&bk_mode==BK_WORK);
 complete(MESH_KEY_JOB_OPEN);completion.backup=(mesh_key_backup_t){.node=local_node_number,.identity=true};memset(completion.backup.public_key,5,32);memset(completion.backup.private_key,6,32);bk_tick();assert(bk_mode==BK_SELECT);
}
int main(void){
 ready();open_key_backup();key_backup_key('\r');assert(bk_mode==BK_PASS);
 type("short");bk_field=2;key_backup_key('\r');assert(bk_mode==BK_PASS&&strstr(bk_feedback,"12 characters"));
 memset(bk_password,0,sizeof(bk_password));bk_field=0;type("a long test passphrase");bk_field=1;type("different text");bk_field=2;key_backup_key('\r');assert(strstr(bk_feedback,"do not match"));
 strcpy(bk_repeat,bk_password);key_backup_key('\r');assert(bk_mode==BK_CAPTURE_SAVE);
 read_ready();size_t writes=write_count;bk_tick();assert(bk_mode==BK_WORK&&last_job==MESH_KEY_JOB_SAVE&&!bk_password[0]&&!bk_repeat[0]&&!keys_loaded&&write_count==writes&&captured.identity&&!captured.channel_mask);
 complete(MESH_KEY_JOB_SAVE);bk_tick();assert(bk_mode==BK_DONE&&strstr(bk_feedback,"saved and verified"));
 ready();unlock();writes=write_count;key_backup_key('\r');assert(bk_mode==BK_CAPTURE_REVIEW&&write_count==writes+1);read_ready();bk_tick();assert(bk_mode==BK_REVIEW&&!bk_confirm);
 writes=write_count;key_backup_key('\r');assert(bk_mode==BK_MENU&&!bk_password[0]&&!bk_saved.identity&&write_count==writes); // Default Cancel.
 ready();unlock();key_backup_key('\r');read_ready();bk_tick();writes=write_count;
 key_backup_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);key_backup_key('\r');assert(bk_mode==BK_SAFETY&&last_job==MESH_KEY_JOB_SAVE&&write_count==writes&&captured.identity&&!captured.channel_mask&&captured.private_key[0]==4);
 key_backup_key('\r');assert(write_count==writes); // Repeated confirm while worker runs does nothing.
 complete(MESH_KEY_JOB_SAVE);bk_tick();assert(bk_mode==BK_BEFORE_APPLY&&write_count==writes+1);
 read_ready();writes=write_count;bk_tick();assert(bk_mode==BK_VERIFY&&keys_state==MESH_KEYS_SAVING&&write_count==writes+1&&!bk_saved.identity&&!bk_password[0]&&keys_desired_private[0]==6);
 keys_state=MESH_KEYS_SAVED;mesh_radio_keys_clear();bk_tick();assert(bk_mode==BK_DONE&&strstr(bk_feedback,"restored and verified"));
 // Changes during safety backup abort without applying; the fresh read is the only write.
 ready();unlock();key_backup_key('\r');read_ready();bk_tick();bk_confirm=true;key_backup_key('\r');complete(MESH_KEY_JOB_SAVE);bk_tick();read_ready();keys_private[0]=9;writes=write_count;bk_tick();assert(bk_mode==BK_DONE&&strstr(bk_feedback,"changed during review")&&write_count==writes);
 // Cancelled worker results cannot revive decrypted data after leaving/reopening.
 ready();open_key_backup();bk_restoring=true;bk_mode=BK_PASS;strcpy(bk_password,"a long test passphrase");strcpy(bk_file,"keys-6b49d200-12345678-abcd1234.mmk");bk_password_submit();assert(pending);
 bk_close();open_key_backup();complete(MESH_KEY_JOB_OPEN);completion.backup.identity=true;bk_tick();assert(bk_mode==BK_MENU&&!bk_saved.identity&&!bk_password[0]);
 ready();unlock();display_and_lights_off=true;bk_tick();assert(!bk_active&&!bk_password[0]&&!bk_saved.identity);
 display_and_lights_off=false;bk_tick();assert(bk_active&&bk_mode==BK_MENU);
 type("x");bk_mode=BK_PASS;strcpy(bk_password,"a long test passphrase");fake_time=last_activity_us+120000001;bk_tick();assert(bk_mode==BK_DONE&&!bk_password[0]);
 // A worker finishing after inactivity must not reopen decrypted data.
 ready();open_key_backup();bk_restoring=true;bk_mode=BK_PASS;strcpy(bk_password,"a long test passphrase");strcpy(bk_file,"keys-6b49d200-12345678-abcd1234.mmk");bk_password_submit();assert(pending);
 fake_time=last_activity_us+120000001;bk_tick();assert(bk_mode==BK_DONE&&!bk_password[0]);
 complete(MESH_KEY_JOB_OPEN);completion.backup.identity=true;bk_tick();assert(bk_mode==BK_DONE&&!bk_saved.identity);
 assert(!key_backup_navigation(BSP_INPUT_NAVIGATION_KEY_F1)&&!key_backup_navigation(BSP_INPUT_NAVIGATION_KEY_F6));
 puts("PASS: actual backup/passphrase/restore UI; default Cancel, encrypted safety before one mutation, fresh change detection, duplicate input, cancelled worker generations, sleeping/timeout secret clearing and global shortcuts");
}
'''
assert main.index('if (enter && chat_action_enter_us')<main.index('if (behavior_key(key)')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'tests/stubs'),str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
