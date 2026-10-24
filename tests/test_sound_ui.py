from ui_source import read_main_ui
"""Exercise the production sound settings UI and message-alert drain policy."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
main = read_main_ui(root)
start = main.index("typedef enum {")
end = main.index("} meshdeck_screen_t;", start) + len("} meshdeck_screen_t;")
screen_enum = main[start:end]
harness = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "sound_player.h"
'''+screen_enum+r'''
static meshdeck_screen_t current_screen=SCREEN_HOME;
#define BSP_INPUT_NAVIGATION_KEY_UP 1
#define BSP_INPUT_NAVIGATION_KEY_DOWN 2
#define BSP_INPUT_NAVIGATION_KEY_LEFT 3
#define BSP_INPUT_NAVIGATION_KEY_RIGHT 4
#define BSP_INPUT_NAVIGATION_KEY_RETURN 5
#define UI_SELECTED 1
#define UI_PANEL 2
#define UI_GREEN 3
#define UI_BORDER 4
#define UI_MUTED 5
#define UI_HINT_OFFSET 60
static int fb, pax_font_sky_mono;
static unsigned logical_h_res=800,logical_v_res=480;
static struct {bool use_24h;} time_settings;
static unsigned text_draws;
static char labels[32][100];
typedef struct {float x,y;} point_t;
static point_t pax_text_size(int font,unsigned size,const char *text){return(point_t){strlen(text)*size*.6f,0};}
static void pax_draw_rect(int *buf,unsigned color,float x,float y,float w,float h){assert(buf==&fb&&x>=0&&x+w<=800&&y>=0&&y+h<=480);}
static void pax_draw_text(int *buf,unsigned color,int font,unsigned size,float x,float y,const char *text){
 assert(buf==&fb&&text_draws<32&&x>=0&&y>=0&&y+size<=480);snprintf(labels[text_draws++],100,"%s",text);
}
static void draw_rich_text(const char *text,int x,int y,unsigned size,unsigned width,unsigned line,unsigned count,unsigned color){assert(text&&x>=0&&y>=0&&y+line*count<480);}
static mesh_sound_settings_t persisted,applied;
static unsigned saves,applies,previews,takes,plays;
static int save_error;
static bool preview_success=true,play_success=true;
static uint8_t preview_volume,channel_alerts_mask=0xff;
static mesh_sound_event_t last_play;
static uint16_t pending_events;
static unsigned unread_channels=8,unread_direct=5;
int mesh_sound_save(const mesh_sound_settings_t *settings){saves++;if(save_error)return save_error;persisted=*settings;return ESP_OK;}
void mesh_sound_apply(const mesh_sound_settings_t *settings){applies++;applied=*settings;}
bool mesh_sound_test(uint8_t volume){previews++;preview_volume=volume;return preview_success;}
bool mesh_sound_play(mesh_sound_event_t event){plays++;last_play=event;return play_success;}
static uint16_t mesh_radio_take_message_alerts(void){takes++;uint16_t events=pending_events;pending_events=0;return events;}
#include "sound_ui.inc"
int main(void){
 // Silent first-run defaults are the policy copied into the draft.
 sound_settings=(mesh_sound_settings_t){0,0,35,0,22,7};
 assert(!sound_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT)&&!sound_key('\t'));
 open_sound_settings();assert(current_screen==SCREEN_SOUND_SETTINGS&&!sound_field&&!sound_feedback[0]);
 assert(!memcmp(&sound_settings,&sound_draft,sizeof(sound_settings)));
 assert(sound_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT)&&sound_draft.message_sounds&&!sound_settings.message_sounds&&!applies&&!saves);
 assert(strstr(sound_feedback,"Unsaved"));
 current_screen=SCREEN_DISPLAY_ALERTS;open_sound_settings(); // Leave without saving, then reopen.
 assert(!sound_draft.message_sounds&&!sound_field&&!sound_feedback[0]&&!applies&&!saves);
 // All selectable rows and directional bounds use the real handlers.
 assert(sound_navigation(BSP_INPUT_NAVIGATION_KEY_UP)&&sound_field==0);
 for(unsigned i=0;i<20;i++)sound_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);
 assert(sound_field==7);
 for(unsigned i=0;i<20;i++)sound_navigation(BSP_INPUT_NAVIGATION_KEY_UP);
 assert(sound_field==0);
 for(unsigned i=0;i<16;i++){assert(sound_key('\t'));assert(sound_field==(i+1)%8);}
 assert(!sound_navigation(99)&&sound_key('z'));
 // Enter/Space alter ordinary rows, not the active saved policy.
 assert(sound_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN)&&sound_draft.message_sounds);
 sound_field=1;assert(sound_key(' ')&&sound_draft.game_sounds);
 sound_field=3;assert(sound_key('\r')&&sound_draft.quiet_enabled);
 assert(!sound_settings.message_sounds&&!sound_settings.game_sounds&&!sound_settings.quiet_enabled);
 sound_field=2;for(unsigned i=0;i<30;i++)sound_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(sound_draft.volume==100);
 for(unsigned i=0;i<30;i++)sound_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);assert(sound_draft.volume==0);
 for(unsigned i=0;i<7;i++)sound_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(sound_draft.volume==35);
 sound_field=4;sound_draft.quiet_start_hour=23;sound_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(sound_draft.quiet_start_hour==0);
 sound_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);assert(sound_draft.quiet_start_hour==23);
 sound_field=5;sound_draft.quiet_end_hour=0;sound_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);assert(sound_draft.quiet_end_hour==23);
 sound_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(sound_draft.quiet_end_hour==0);
 // Explicit Test Sound uses the unsaved volume, even with categories disabled or quiet.
 sound_draft.message_sounds=sound_draft.game_sounds=0;sound_draft.quiet_enabled=1;
 sound_draft.volume=65;sound_field=6;sound_key('\n');
 assert(previews==1&&preview_volume==65&&!saves&&!applies&&strstr(sound_feedback,"quiet hours bypassed"));
 preview_success=false;sound_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(previews==2&&strstr(sound_feedback,"unavailable or busy"));
 sound_draft.volume=0;sound_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(previews==2&&strstr(sound_feedback,"muted at volume 0"));
 sound_draft.volume=65;
 unsigned previous_saves=saves,previous_applies=applies;
 sound_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);sound_key(' ');
 assert(saves==previous_saves&&applies==previous_applies); // Arrows/Space on Test never trigger it.
 sound_field=7;save_error=9;sound_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(saves==1&&!applies&&!sound_settings.quiet_enabled&&sound_settings.volume==35&&strstr(sound_feedback,"Could not save"));
 save_error=0;sound_key('\r');
 assert(saves==2&&applies==1&&!memcmp(&persisted,&sound_draft,sizeof(persisted))&&!memcmp(&applied,&sound_settings,sizeof(applied)));
 assert(sound_settings.quiet_enabled&&sound_settings.volume==65&&strstr(sound_feedback,"saved"));
 sound_draft.volume=20;open_sound_settings();assert(sound_draft.volume==65); // Saved settings survive a new draft.
 char label[16];time_settings.use_24h=true;sound_hour_label(0,label,sizeof(label));assert(!strcmp(label,"00:00"));
 sound_hour_label(23,label,sizeof(label));assert(!strcmp(label,"23:00"));
 time_settings.use_24h=false;sound_hour_label(0,label,sizeof(label));assert(!strcmp(label,"12am"));
 sound_hour_label(12,label,sizeof(label));assert(!strcmp(label,"12pm"));
 sound_hour_label(23,label,sizeof(label));assert(!strcmp(label,"11pm"));
 draw_sound_settings();assert(text_draws==17&&!strcmp(labels[0],"Message sounds")&&!strcmp(labels[12],"TEST SOUND")&&!strcmp(labels[14],"SAVE SETTINGS"));
 // Drain notification events once, coalesce channel bursts and prefer a DM.
 for(unsigned mask=0;mask<=255;mask+=17)for(unsigned events=0;events<=511;events++){
  channel_alerts_mask=mask;pending_events=events;unsigned old_takes=takes,old_plays=plays;
  play_message_alerts();bool direct=(events&0x100)!=0,channel=(events&mask&0xff)!=0;
  assert(takes==old_takes+1&&!pending_events&&plays==old_plays+(direct||channel));
  if(direct||channel)assert(last_play==(direct?MESH_SOUND_DIRECT:MESH_SOUND_CHANNEL));
  assert(unread_channels==8&&unread_direct==5); // Counts are independent of sound policy.
  old_plays=plays;play_message_alerts();assert(plays==old_plays); // Repaint cannot replay old sounds.
 }
 // Ignored high event bits and muted/failed playback are still consumed.
 pending_events=0x8000;unsigned old_plays=plays;play_message_alerts();assert(!pending_events&&plays==old_plays);
 play_success=false;pending_events=0x101;play_message_alerts();old_plays=plays;play_message_alerts();assert(!pending_events&&plays==old_plays&&last_play==MESH_SOUND_DIRECT);
 puts("PASS: production sound UI drafts/cancel/save/failure, Enter/Tab/arrow bounds and hours, explicit draft-volume preview, time labels, event coalescing/DM priority/channel masks and independent unread counts");
}
'''
with tempfile.TemporaryDirectory(prefix="meshmatsu-sound-ui-") as folder:
    folder = Path(folder)
    (folder / "test.c").write_text(harness)
    subprocess.run(["clang", "-std=c11", "-fsanitize=address,undefined",
                    "-I", str(root / "tests/stubs"), "-I", str(root / "main"),
                    str(folder / "test.c"), "-o", str(folder / "test")], check=True)
    subprocess.run([str(folder / "test")], check=True)
