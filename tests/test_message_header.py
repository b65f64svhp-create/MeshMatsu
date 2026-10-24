from ui_source import read_main_ui
"""Actual message header reserves timestamp width rather than ten name bytes."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];s=read_main_ui(root)
a=s.index('static int draw_message_timestamp(');b=s.index('typedef struct { int x, y, font_size;',a)
stamp=s[a:b]
a=s.index('static void draw_message_header(');b=s.index('#include "message_search_draw.inc"',a)
header=s[a:b]
assert s.count('draw_message_header(')==4
assert '"%.10s:", message.from' not in s
prelude=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "radio_link.h"
typedef struct {float x,y;} pax_vec2f;
static int fb,font,*pax_font_sky_mono=&font,time_settings;
static unsigned logical_h_res=800;
#define UI_MUTED 1
#define UI_GREEN 2
static int timestamp_x,name_width,draws;
static const char *name="Greenview Base Radio with long name";
static char rendered[64];
static void mesh_time_stamp(int *settings,uint32_t timestamp,char *out,size_t size){snprintf(out,size,"%s",*settings?"OCT-17 11:59pm":"OCT-17 23:59");}
static pax_vec2f pax_text_size(int *font,int size,const char *text){return (pax_vec2f){strlen(text)*8,11};}
static void pax_draw_text(int *fb,unsigned color,int *font,int size,int x,int y,const char *text){timestamp_x=x;}
static int draw_rich_text(const char *text,int x,int y,int size,int width,int line_height,int lines,unsigned color){
 assert(x==32&&width>0&&x+width+12==timestamp_x&&lines==1);name_width=width;draws++;snprintf(rendered,sizeof(rendered),"%s",text);return y+line_height;
}
void mesh_radio_message_sender_name(const mesh_message_t *message,char *out,size_t size){snprintf(out,size,"%s",message->outgoing?"You":name);}
'''
body=r'''
int main(void){mesh_message_t message={0};
 for(unsigned width=480;width<=800;width+=320)for(int clock=0;clock<2;clock++){
  logical_h_res=width;time_settings=clock;draw_message_header(&message,100,0);
  assert(!strcmp(rendered,"Greenview Base Radio with long name:")&&name_width<(int)width-64);
  draw_message_header(&message,100,17);assert(!strcmp(rendered,"17. Greenview Base Radio with long name"));
 }
 message.outgoing=true;draw_message_header(&message,100,0);assert(!strcmp(rendered,"You:")&&draws==9);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(prelude+stamp+header+body)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'main'),str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
