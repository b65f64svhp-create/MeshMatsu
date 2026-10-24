#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "radio_link.h"
#include "map_trails.h"
#include "map_filters.h"
static mesh_map_filters_t map_filters;
#define EXT_RAM_BSS_ATTR
#include "map_trails.inc"
#include "../main/map_view.c"
typedef uint32_t pax_col_t;
#define UI_GREEN 0xFF67EA94
static int fb;
static mesh_map_view_t map_view;
static unsigned map_trail_mode=1;
enum {MAP_TRAILS_OFF,MAP_TRAILS_SELECTED,MAP_TRAILS_ALL};
static uint32_t map_selected_node=42;
static mesh_node_t database[5];static size_t total=5;
size_t mesh_radio_node_count(void){return total;}
uint32_t mesh_radio_local_node(void){return 1;}
bool mesh_radio_get_node(size_t i,mesh_node_t *out){if(i>=total)return false;*out=database[i];return true;}
static size_t drawn;static uint32_t colors[30];
static void pax_draw_line(int *buffer,uint32_t color,double x0,double y0,double x1,double y1){assert(buffer==&fb&&drawn<30);assert(x0>=20&&x0<=779&&x1>=20&&x1<=779&&y0>=94&&y0<=337&&y1>=94&&y1<=337);if(color!=0xFF101513)colors[drawn++]=color;}
#include "map_trails_draw.inc"
static const uint32_t now=1800000000;
int main(void){mesh_map_center(&map_view,0,0,8);uint32_t ids[]={1,42,43,44,45};for(unsigned i=0;i<5;i++){database[i]=(mesh_node_t){.number=ids[i],.position_available=true,.position_time=now};assert(mesh_trails_add(ids[i],0,-1000000,now-10,0,now)&&mesh_trails_add(ids[i],0,1000000,now,0,now));}database[3].is_ignored=true;database[4].position_available=false;
 draw_map_trails(20,94,760,244,now);assert(drawn==1&&colors[0]==UI_GREEN);
 drawn=0;map_trail_mode=MAP_TRAILS_ALL;draw_map_trails(20,94,760,244,now);assert(drawn==3&&colors[0]==0xFF36B8FF&&colors[1]==0xFF788F81&&colors[2]==UI_GREEN); // selected is last, ignored/unlocated hidden
 drawn=0;map_filters.favorites_only=true;draw_map_trails(20,94,760,244,now);assert(!drawn);
 database[1].is_favorite=true;draw_map_trails(20,94,760,244,now);assert(drawn==1&&colors[0]==UI_GREEN);
 drawn=0;map_filters.recent_only=true;database[1].position_time=now-3601;draw_map_trails(20,94,760,244,now);assert(!drawn);
 database[1].position_time=now;draw_map_trails(20,94,760,244,now);assert(drawn==1);
 map_filters=(mesh_map_filters_t){0};
 drawn=0;map_trail_mode=MAP_TRAILS_OFF;draw_map_trails(20,94,760,244,now);assert(!drawn);map_trail_mode=MAP_TRAILS_SELECTED;
 draw_map_trails(20,94,760,244,0);draw_map_trails(20,94,760,244,now+86401);assert(!drawn);
 char text[100];map_trail_summary(42,now,text,sizeof(text));assert(!strcmp(text,"1 trail link"));map_trail_summary(99,now,text,sizeof(text));assert(strstr(text,"2 timed fixes"));map_trail_summary(42,0,text,sizeof(text));assert(strstr(text,"valid clock"));
 mesh_trails_reset();assert(mesh_trails_add(42,0,0,now-3601,0,now)&&mesh_trails_add(42,0,1000,now,0,now));draw_map_trails(20,94,760,244,now);assert(!drawn);map_trail_summary(42,now,text,sizeof(text));assert(strstr(text,"gaps"));
 mesh_trails_reset();assert(mesh_trails_add(42,0,0,now-1,16,now)&&mesh_trails_add(42,0,1000,now,32,now));draw_map_trails(20,94,760,244,now);assert(!drawn);
 mesh_trails_reset();assert(mesh_trails_add(42,0,-100000000,now-1,0,now)&&mesh_trails_add(42,0,100000000,now,0,now));draw_map_trails(20,94,760,244,now);assert(drawn==1); // visible middle even with offscreen endpoints
 puts("PASS: actual trail renderer selected/all/off modes, local/other/selected colors and order, ignore/location/clock/age guards, gap/precision breaks, clipping and honest empty-history status");}
