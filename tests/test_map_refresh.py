from ui_source import read_main_ui
"""A finished map bypasses idle redraw delays, with input/sleep protection."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=read_main_ui(root)
a=s.index('        bool map_ready = current_screen == SCREEN_MAP');b=s.index('        bsp_input_event_t event;',a)
condition=s[a:b]
h=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define SCREEN_MAP 1
#define SCREEN_MESSAGES 2
#define SCREEN_HOME 3
static int current_screen=SCREEN_MAP;
static bool display_and_lights_off,input_waiting,typing_recently,compose_redraw_pending,pending;
static int64_t now_us=100,last_screen_draw_us=0,redraw_interval=30000000;
static unsigned draws,checks,clock_checks;
static bool clock_pending;
static bool home_clock_needs_draw(void){clock_checks++;return clock_pending;}
static bool mesh_map_tiles_needs_draw(void){checks++;return pending;}
static void draw_screen(void){draws++;pending=false;clock_pending=false;last_screen_draw_us=now_us;}
static void poll_draw(void){
'''+condition+r'''
}
int main(void){
 pending=true;poll_draw();assert(draws==1&&!pending&&checks==1);
 // No repeated redraws once the frame is consumed, even in reduced activity mode.
 poll_draw();assert(draws==1);
 pending=true;input_waiting=true;poll_draw();assert(draws==1&&pending);
 input_waiting=false;poll_draw();assert(draws==2&&!pending);
 pending=true;display_and_lights_off=true;unsigned before=checks;poll_draw();assert(draws==2&&checks==before);
 display_and_lights_off=false;current_screen=SCREEN_MESSAGES;poll_draw();assert(draws==2&&checks==before);
 current_screen=SCREEN_MAP;typing_recently=true;poll_draw();assert(draws==2);
 typing_recently=false;compose_redraw_pending=true;poll_draw();assert(draws==2);
 compose_redraw_pending=false;poll_draw();assert(draws==3);
 // The ordinary periodic refresh is preserved when no map frame is pending.
 now_us+=redraw_interval;poll_draw();assert(draws==4);
 current_screen=SCREEN_HOME;clock_pending=true;poll_draw();assert(draws==5&&!clock_pending);
 poll_draw();assert(draws==5);
 clock_pending=true;display_and_lights_off=true;before=clock_checks;poll_draw();assert(draws==5&&clock_checks==before);
 display_and_lights_off=false;input_waiting=true;poll_draw();assert(draws==5&&clock_pending);
 input_waiting=false;poll_draw();assert(draws==6&&!clock_pending);
 clock_pending=true;current_screen=SCREEN_MESSAGES;before=clock_checks;poll_draw();assert(draws==6&&clock_checks==before);
 puts("PASS: completed maps display without 1/30-second timer delay; no redraw loop, no offscreen/asleep updates, input/composer precedence and periodic refresh preserved");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
