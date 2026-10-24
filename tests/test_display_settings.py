from ui_source import read_main_ui
"""Exercise real persistence, settings edits/save, and idle policy without hardware."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = read_main_ui(root)
a = source.index('typedef enum {'); b = source.index('} meshdeck_screen_t;', a) + len('} meshdeck_screen_t;')
enum = source[a:b]
a = source.index('static esp_err_t apply_display_brightness('); b = source.index('static void open_manual_channel', a)
functions = source[a:b]
a=source.index('static esp_err_t apply_backlight_levels(');b=source.index('static void update_idle_backlights',a)
functions=source[a:b]+functions
a = source.index('    if (current_screen == SCREEN_DISPLAY_ALERTS) {', source.index('static bool settings_navigation'))
b = source.index('    if (current_screen == SCREEN_MANUAL_CHANNEL)', a)
navigation = source[a:b]
harness = r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define ESP_ERR_INVALID_STATE 1
#include "display_settings.h"
#include "backlight.h"
#include "nvs.h"
#define ESP_LOGW(...) ((void)0)
static uint8_t durable[8], pending[8];
static size_t durable_size;
static int open_error, write_error, commit_error;
static unsigned closes, commits, light_updates;
static bool exists;
int nvs_open(const char *ns, int mode, nvs_handle_t *h) {
    assert(!strcmp(ns,"meshdeck_mui")); *h=1;
    return open_error ? open_error : !mode && !exists ? ESP_ERR_NVS_NOT_FOUND : 0;
}
int nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *size) {
    assert(!strcmp(key,"display_v1"));
    if (durable_size>*size) return 7;
    memcpy(out,durable,durable_size); *size=durable_size; return 0;
}
int nvs_set_blob(nvs_handle_t h, const char *key, const void *in, size_t size) {
    assert(size==8); memcpy(pending,in,8); return write_error;
}
int nvs_commit(nvs_handle_t h) {
    commits++; if(commit_error) return commit_error;
    memcpy(durable,pending,8); durable_size=8; exists=true; return 0;
}
void nvs_close(nvs_handle_t h) { closes++; }
static uint8_t screen=80, keyboard=60, saved_display_brightness=80, saved_keyboard_brightness=60;
static uint8_t active_display_brightness=80, active_keyboard_brightness=60;
static bool display_preview_active;
static int applied_screen=-1, applied_keyboard=-1;
static bool ignore_screen_write;
static mesh_display_settings_t display_settings, display_draft;
static size_t display_field;
static char display_feedback[80];
static int64_t now=1000000, last_activity_us;
static int screen_error, keyboard_error;
static int bsp_display_set_backlight_brightness(uint8_t value) { if(screen_error) return screen_error; if(!ignore_screen_write) screen=value; return 0; }
static int bsp_input_set_backlight_brightness(uint8_t value) { if(keyboard_error) return keyboard_error; keyboard=value; return 0; }
int mesh_backlight_set(bool is_keyboard, uint8_t value) {
    if (!is_keyboard && ignore_screen_write) return ESP_ERR_INVALID_STATE;
    return is_keyboard ? bsp_input_set_backlight_brightness(value) : bsp_display_set_backlight_brightness(value);
}
static int64_t esp_timer_get_time(void) { return now; }
static void update_message_light(void) { light_updates++; }
'''+enum+r'''
static meshdeck_screen_t current_screen;
'''+functions+r'''
#define BSP_INPUT_NAVIGATION_KEY_UP 1
#define BSP_INPUT_NAVIGATION_KEY_DOWN 2
#define BSP_INPUT_NAVIGATION_KEY_RETURN 3
#define BSP_INPUT_NAVIGATION_KEY_LEFT 4
#define BSP_INPUT_NAVIGATION_KEY_RIGHT 5
static bool navigate(int key) {
'''+navigation+r'''
    return false;
}
int main(void) {
    display_settings=mesh_display_defaults(80,60);
    assert(mesh_display_load(&display_settings)==ESP_ERR_NVS_NOT_FOUND);
    assert(display_settings.screen_brightness==80 && !display_settings.timeout_seconds && display_settings.night_brightness==20);
    assert(mesh_display_valid(&display_settings));
    open_display_settings();
    assert(current_screen==SCREEN_DISPLAY_ALERTS && display_field==0);
    step_display_setting(false); assert(display_draft.screen_brightness==75 && screen==75 && display_preview_active);
    open_display_settings(); assert(display_draft.screen_brightness==80 && screen==80 && !display_preview_active); // Discarded draft.
    for(int i=0;i<100;i++) step_display_setting(false);
    assert(display_draft.screen_brightness==10);
    display_field=1; for(int i=0;i<100;i++) step_display_setting(false);
    assert(!display_draft.keyboard_brightness);
    display_field=2;
    const unsigned choices[]={15,30,45,60,120,300,900,0};
    for(unsigned i=0;i<8;i++){step_display_setting(true);assert(display_draft.timeout_seconds==choices[i]);}
    step_display_setting(false); assert(display_draft.timeout_seconds==900);
    display_field=3; step_display_setting(true); assert(!display_draft.unread_light);
    display_field=4; for(int i=0;i<100;i++) step_display_setting(false);
    assert(display_draft.night_brightness==5);
    display_field=5; step_display_setting(true); assert(display_draft.alert_sources==MESH_ALERT_CHANNEL);
    step_display_setting(true); assert(display_draft.alert_sources==MESH_ALERT_DIRECT);
    // Enter saves directly from the first row after successful driver checks.
    display_field=0;
    assert(navigate(BSP_INPUT_NAVIGATION_KEY_RETURN));
    assert(screen==10 && keyboard==0 && light_updates==1 && last_activity_us==now && strstr(display_feedback,"Saved"));
    mesh_display_settings_t reloaded=mesh_display_defaults(100,100);
    assert(mesh_display_load(&reloaded)==0 && !memcmp(&reloaded,&display_settings,sizeof(reloaded)));
    display_draft.screen_brightness=95;
    ignore_screen_write=true;
    save_display_settings();
    assert(strstr(display_feedback,"Not saved") && display_settings.screen_brightness==10);
    ignore_screen_write=false;
    commit_error=9; save_display_settings();
    assert(screen==10 && keyboard==0 && display_settings.screen_brightness==10 && strstr(display_feedback,"Not saved"));
    commit_error=0; assert(mesh_display_load(&reloaded)==0 && reloaded.screen_brightness==10);
    display_draft.keyboard_brightness=5;
    keyboard_error=9; unsigned count=commits; save_display_settings(); assert(commits==count && screen==10);
    keyboard_error=0;
    display_field=1; display_draft=display_settings;
    assert(navigate(BSP_INPUT_NAVIGATION_KEY_RIGHT) && keyboard==5 && display_preview_active);
    discard_display_preview(); assert(keyboard==0 && !display_preview_active);
    for(unsigned i=0;i<8;i++) {
        uint8_t original=durable[i]; durable[i]=255;
        reloaded=mesh_display_defaults(80,60);
        assert(mesh_display_load(&reloaded)==ESP_ERR_INVALID_ARG && reloaded.screen_brightness==80);
        durable[i]=original;
    }
    // Version 1 stored linear hardware percentages. Preserve light output,
    // convert only brightness, then persist v2 without a second conversion.
    uint8_t before[8]; memcpy(before,durable,8);
    const uint8_t legacy[]={1,50,20,5,1,20,3};memcpy(durable,legacy,7);durable_size=7;
    assert(mesh_display_load(&reloaded)==0);
    assert(abs((int)mesh_backlight_raw(reloaded.screen_brightness)-127)<=3);
    assert(abs((int)mesh_backlight_raw(reloaded.keyboard_brightness)-51)<=3);
    assert(reloaded.timeout_seconds==300 && reloaded.night_brightness==20);
    durable[0]=2;assert(mesh_display_load(&reloaded)==0&&reloaded.screen_brightness==50&&reloaded.keyboard_brightness==20&&reloaded.timeout_seconds==300);
    assert(mesh_display_save(&reloaded)==0 && durable[0]==3);
    mesh_display_settings_t again=mesh_display_defaults(100,100);
    assert(mesh_display_load(&again)==0 && !memcmp(&again,&reloaded,sizeof(again)));
    memcpy(durable,before,8);
    durable_size=3; assert(mesh_display_load(&reloaded)==ESP_ERR_INVALID_ARG);
    durable_size=8; write_error=9; count=commits;
    assert(mesh_display_save(&display_settings)==9 && commits==count); write_error=0;
    open_error=9; assert(mesh_display_load(&reloaded)==9); open_error=0;
    const uint16_t intervals[]={15,30,45,60,120,300,900};
    for(size_t i=0;i<7;i++) {
        display_settings.timeout_seconds=intervals[i];
        assert(mesh_display_save(&display_settings)==ESP_OK);
        assert(mesh_display_load(&reloaded)==ESP_OK && reloaded.timeout_seconds==intervals[i]);
        int64_t delta=(int64_t)intervals[i]*1000000;
        assert(!mesh_display_idle_due(&display_settings,now+delta-1,now));
        assert(mesh_display_idle_due(&display_settings,now+delta,now));
        assert(!mesh_display_idle_due(&display_settings,now-1,now));
    }
    puts("PASS: defaults, live preview/cancel, save dispatch, setting bounds/options, v1/v2 migration, persistence/reload, invalid blobs, failed saves/rollback, 15/30/45-second and 1/2/5/15-minute idle thresholds");
}
'''
with tempfile.TemporaryDirectory(prefix='meshdeck-display-') as folder:
    path=Path(folder); (path/'test.c').write_text(harness)
    subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'main'),'-I',str(root/'tests/stubs'),str(path/'test.c'),str(root/'main/display_settings.c'),str(root/'main/backlight_curve.c'),'-lm','-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
