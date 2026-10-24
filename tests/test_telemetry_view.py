from ui_source import read_main_ui
"""Actual telemetry navigation, sparse fields, unit formatting and age labels."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];main=read_main_ui(r);enum=main[main.index('typedef enum {'):main.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
h='#define main units_tests\n#include "'+str(r/'tests/test_units.c')+'"\n#undef main\n'+enum+'''
#include <stdio.h>
#include <time.h>
#include "telemetry_data.h"
#define EXT_RAM_BSS_ATTR
#include "telemetry_data.inc"
static meshdeck_screen_t current_screen=SCREEN_NODES;
enum {NODE_TELEMETRY=3};static unsigned node_modal=NODE_TELEMETRY;
static uint8_t temperature_units;
static char telemetry_feedback[96];
#include "telemetry_export.h"
static unsigned exports;static uint32_t exported_node;static uint8_t exported_kind;static mesh_tlm_export_start_t export_result=MESH_EXPORT_OK;
mesh_tlm_export_start_t mesh_tlm_export_start(uint32_t node,uint8_t kind){exports++;exported_node=node;exported_kind=kind;return export_result;}
static unsigned retries;static void mesh_tlm_store_retry(void){retries++;}
static uint32_t mesh_radio_local_node(void){return 1;}
static int64_t esp_timer_get_time(void){return 5000000;}
static time_t test_wall(time_t *out){if(out)*out=1800000000;return 1800000000;}
enum {BSP_INPUT_NAVIGATION_KEY_UP=1,BSP_INPUT_NAVIGATION_KEY_DOWN,BSP_INPUT_NAVIGATION_KEY_LEFT,BSP_INPUT_NAVIGATION_KEY_RIGHT};
#define time(...) test_wall(__VA_ARGS__)
#include "telemetry_view.inc"
#undef time
int main(void){
 mesh_tlm_sample_t s={.node=42,.kind=3,.present=1|(1u<<21),.value={[0]=-20,[21]=-10}};assert(mesh_tlm_add(&s));
 telemetry_view_open(42,false);assert(telemetry_kind==2&&!telemetry_graph&&telemetry_selected()==1);
 assert(telemetry_view_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT)&&telemetry_kind==3);
 telemetry_view_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);assert(telemetry_selected()==22);telemetry_view_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);assert(telemetry_selected()==1);
 telemetry_view_navigation(BSP_INPUT_NAVIGATION_KEY_UP);assert(telemetry_selected()==22);
 char text[150];temperature_units=1;telemetry_value_text(1,-20,text,sizeof(text));assert(!strcmp(text,"-4.0 F")&&fabs(telemetry_display_value(1,-20)+4)<1e-9);
 temperature_units=0;telemetry_value_text(22,-10,text,sizeof(text));assert(!strcmp(text,"-10.0 C"));
 assert(telemetry_view_key('E')&&exports==1&&exported_node==42&&exported_kind==3&&telemetry_export_visible&&!telemetry_feedback[0]);export_result=MESH_EXPORT_BUSY;telemetry_view_key('e');assert(!telemetry_export_visible&&strstr(telemetry_feedback,"already"));export_result=MESH_EXPORT_EMPTY;telemetry_view_key('E');assert(strstr(telemetry_feedback,"No readings"));
 assert(telemetry_view_key('\t')&&telemetry_graph);assert(telemetry_view_key('r')&&retries==1);assert(!telemetry_view_key('x'));
 telemetry_view_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);telemetry_view_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);assert(telemetry_kind==5);telemetry_value_text(2,-100,text,sizeof(text));assert(!strcmp(text,"-100.00 mA"));
 telemetry_view_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(telemetry_kind==2);telemetry_value_text(1,101,text,sizeof(text));assert(!strcmp(text,"External power"));
 current_screen=SCREEN_HOME;assert(!telemetry_view_key('E')&&!telemetry_view_key('\t')&&!telemetry_view_key('r')&&retries==1);
 telemetry_view_open(1,true);assert(telemetry_kind==6&&!telemetry_graph);telemetry_view_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(telemetry_kind==2);telemetry_view_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);assert(telemetry_kind==6);
 s.flags=MESH_TLM_CACHED;telemetry_age(&s,text,sizeof(text));assert(strstr(text,"cache"));s.flags=MESH_TLM_RESTORED;s.time=1799999940;telemetry_age(&s,text,sizeof(text));assert(strstr(text,"1m 0s"));
 s.time=0;telemetry_age(&s,text,sizeof(text));assert(strstr(text,"Saved report"));s.flags=0;s.received_us=2000000;telemetry_age(&s,text,sizeof(text));assert(strstr(text,"Received 3s"));s.flags=MESH_TLM_CLOCK_SKEW;telemetry_age(&s,text,sizeof(text));assert(strstr(text,"clock differs"));
 puts("PASS: sparse field wrap/scroll selection, per-report tabs, local-only LocalStats, graph toggle/retry scope, signed C/F/current, external power and honest live/saved/cache/skew ages");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(r/'tests/stubs'),'-I',str(r/'main'),str(p/'test.c'),'-lm','-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],cwd=d,check=True)
