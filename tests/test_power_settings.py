from ui_source import read_main_ui
"""Exercise persisted independent switches and actual Battery Saver navigation."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=read_main_ui(root)
a=s.index('typedef enum {'); b=s.index('} meshdeck_screen_t;',a)+len('} meshdeck_screen_t;');enum=s[a:b]
a=s.index('static void open_power_settings');b=s.index('static esp_err_t apply_display_brightness',a);functions=s[a:b]
a=s.index('    if (current_screen == SCREEN_POWER_SETTINGS) {',s.index('static bool settings_navigation'));b=s.index('    if (current_screen == SCREEN_DISPLAY_ALERTS)',a);nav=s[a:b]
harness=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "power_settings.h"
#include "nvs.h"
#define ESP_LOGW(...) ((void)0)
static uint8_t durable[7], pending[7]; static size_t stored_size;
static bool exists; static int storage_error;
int nvs_open(const char *ns,int mode,nvs_handle_t *h) { assert(!strcmp(ns,"meshdeck_mui")); *h=1; return !exists&&!mode?ESP_ERR_NVS_NOT_FOUND:0; }
int nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size) { assert(!strcmp(key,"power_v1")); if(stored_size>*size)return 9;memcpy(out,durable,stored_size);*size=stored_size;return 0; }
int nvs_set_blob(nvs_handle_t h,const char *key,const void *in,size_t size) { assert(!strcmp(key,"power_v1")&&size==7);memcpy(pending,in,7);return 0; }
int nvs_commit(nvs_handle_t h) { if(storage_error)return storage_error;memcpy(durable,pending,7);stored_size=7;exists=true;return 0; }
void nvs_close(nvs_handle_t h) {}
static mesh_power_settings_t power_settings,power_draft;
static size_t power_field;static char power_feedback[80];static int64_t last_activity_us,now=1000000;
static unsigned lights,backlights;
static int64_t esp_timer_get_time(void){return now;}
static void update_idle_backlights(int64_t now,bool queued){backlights++;}
static void update_message_light(void){lights++;}
'''+enum+r'''
static meshdeck_screen_t current_screen;
'''+functions+r'''
#define BSP_INPUT_NAVIGATION_KEY_UP 1
#define BSP_INPUT_NAVIGATION_KEY_DOWN 2
#define BSP_INPUT_NAVIGATION_KEY_RETURN 3
#define BSP_INPUT_NAVIGATION_KEY_LEFT 4
#define BSP_INPUT_NAVIGATION_KEY_RIGHT 5
static bool navigate(int key){
'''+nav+r'''
 return false;
}
int main(void){
 power_settings=mesh_power_defaults();
 assert(power_settings.decorative_lights && !power_settings.keyboard_timeout_seconds && !power_settings.dim_timeout_seconds && !power_settings.reduced_background);
 assert(mesh_power_load(&power_settings)==ESP_ERR_NVS_NOT_FOUND);
 open_power_settings();assert(current_screen==SCREEN_POWER_SETTINGS);
 navigate(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(!power_draft.decorative_lights && power_settings.decorative_lights);
 open_power_settings();assert(power_draft.decorative_lights); // Discard unsaved edits.
 for(unsigned bits=0;bits<16;bits++){
  mesh_power_settings_t value={bits&1,((bits>>1)&1)?15:0,((bits>>2)&1)?15:0,(bits>>3)&1};
  assert(mesh_power_save(&value)==0);
  mesh_power_settings_t loaded=mesh_power_defaults();assert(mesh_power_load(&loaded)==0&&!memcmp(&loaded,&value,sizeof(value)));
 }
 // Change each option through the real navigation branch and save with Enter.
 open_power_settings();
 for(unsigned i=0;i<4;i++){navigate(BSP_INPUT_NAVIGATION_KEY_RIGHT);if(i<3)navigate(BSP_INPUT_NAVIGATION_KEY_DOWN);}
 navigate(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(!power_settings.decorative_lights && power_settings.keyboard_timeout_seconds && power_settings.dim_timeout_seconds && power_settings.reduced_background);
 assert(lights==1&&backlights==1&&last_activity_us==now&&strstr(power_feedback,"Saved"));
 mesh_power_settings_t baseline=power_settings;
 power_draft.decorative_lights=1;storage_error=9;save_power_settings();
 assert(!memcmp(&power_settings,&baseline,sizeof(baseline))&&strstr(power_feedback,"Not saved")); storage_error=0;
 for(unsigned i=0;i<7;i++){uint8_t old=durable[i];durable[i]=255;mesh_power_settings_t value=baseline;assert(mesh_power_load(&value)==ESP_ERR_INVALID_ARG&&!memcmp(&value,&baseline,sizeof(value)));durable[i]=old;}
 stored_size=2;assert(mesh_power_load(&power_settings)==ESP_ERR_INVALID_ARG);
 mesh_power_settings_t invalid={2,0,0,0};assert(mesh_power_save(&invalid)==ESP_ERR_INVALID_ARG);
 // Old booleans migrate to their original 15-second behavior.
 const uint8_t legacy[]={1,1,1,1,0};memcpy(durable,legacy,5);stored_size=5;
 assert(mesh_power_load(&power_settings)==0&&power_settings.keyboard_timeout_seconds==15&&power_settings.dim_timeout_seconds==15);
 // New timers are independent at every supported boundary.
 for(unsigned seconds=15;seconds<=60;seconds+=15){
  mesh_power_settings_t value={1,seconds,60,0};assert(mesh_power_save(&value)==0);
  mesh_power_settings_t copy={0};assert(mesh_power_load(&copy)==0&&copy.keyboard_timeout_seconds==seconds&&copy.dim_timeout_seconds==60);
  mesh_power_backlights_t out=mesh_power_backlights(&value,false,1000000+(int64_t)seconds*1000000-1,1000000,80,60);assert(out.keyboard==60);
  out=mesh_power_backlights(&value,false,1000000+(int64_t)seconds*1000000,1000000,80,60);assert(!out.keyboard&&out.screen==(seconds==60?20:80));
 }
 // Independent policies, exact 15-second boundary, backwards clock and wake.
 for(unsigned bits=0;bits<16;bits++){
  mesh_power_settings_t value={bits&1,((bits>>1)&1)?15:0,((bits>>2)&1)?15:0,(bits>>3)&1};
  mesh_power_backlights_t out=mesh_power_backlights(&value,false,15999999,1000000,80,60);assert(out.screen==80&&out.keyboard==60);
  out=mesh_power_backlights(&value,false,16000000,1000000,80,60);assert(out.screen==(value.dim_timeout_seconds?20:80)&&out.keyboard==(value.keyboard_timeout_seconds?0:60));
  out=mesh_power_backlights(&value,true,16000000,1000000,80,60);assert(!out.screen&&!out.keyboard);
  out=mesh_power_backlights(&value,false,16000000,16000000,80,60);assert(out.screen==80&&out.keyboard==60);
  out=mesh_power_backlights(&value,false,1,1000000,80,0);assert(out.screen==80&&!out.keyboard);
  assert(mesh_power_wait_ms(&value,false,true)==10);
  assert(mesh_power_wait_ms(&value,false,false)==(value.reduced_background?50:20));
  assert(mesh_power_wait_ms(&value,true,false)==(value.reduced_background?100:20));
  assert(mesh_power_redraw_interval(&value,false,false)==(value.reduced_background?30000000:1000000));
  assert(mesh_power_redraw_interval(&value,true,false)==1000000&&mesh_power_redraw_interval(&value,false,true)==1000000);
 }
 puts("PASS: all independent power switches, defaults, draft cancel, save dispatch/persistence/failures, 15-second dim/key policies, wake and reduced-idle scheduling");
}
'''
with tempfile.TemporaryDirectory(prefix='meshdeck-power-') as folder:
 p=Path(folder);(p/'test.c').write_text(harness)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'main'),'-I',str(root/'tests/stubs'),str(root/'main/power_settings.c'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
