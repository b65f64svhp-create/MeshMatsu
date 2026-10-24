#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define EXT_RAM_BSS_ATTR
#include "map_trails.h"
#include "map_trails.inc"
static const uint32_t now=1800000000;
static bool add(uint32_t id,int32_t lat,int32_t lon,uint32_t time,uint8_t precision){return mesh_trails_add(id,lat,lon,time,precision,now);}
int main(void){
 mesh_trails_reset();mesh_trail_point_t out[64];
 assert(!add(0,0,0,now,0)&&!add(UINT32_MAX,0,0,now,0));assert(!add(42,900000001,0,now,0)&&!add(42,0,-1800000001,now,0));assert(!add(42,0,0,0,0)&&!add(42,0,0,now,33)&&!add(42,0,0,now+301,0));assert(!add(42,0,0,now-86401,0));
 assert(add(42,0,0,now-3600,16));assert(!add(42,0,0,now-3600,16));assert(!add(42,1,0,now-3600,16));assert(!add(42,0,0,now-3601,16));
 assert(add(42,0,0,now-10,16));assert(mesh_trails_count()==1&&mesh_trails_series(42,now,out,64)==1&&out[0].time==now-3600&&out[0].until==now-10);
 assert(add(42,0,1,now,16));assert(mesh_trails_series(42,now,out,64)==2&&mesh_trails_join(&out[0],&out[1],now));
 assert(mesh_trails_series(42,now+86400,out,64)==1&&mesh_trails_series(42,now+86401,out,64)==0); // inclusive 24h boundary
 mesh_trails_reset();assert(add(1,0,0,now-3601,16)&&add(1,1,0,now,16));assert(mesh_trails_series(1,now,out,64)==2&&!mesh_trails_join(&out[0],&out[1],now));
 mesh_trails_reset();assert(add(1,0,0,now-3600,16)&&add(1,1,0,now,16));mesh_trails_series(1,now,out,64);assert(mesh_trails_join(&out[0],&out[1],now));out[1].precision=32;assert(!mesh_trails_join(&out[0],&out[1],now));
 mesh_trails_reset();for(unsigned i=0;i<80;i++)assert(add(42,i,0,now-80+i,32));assert(mesh_trails_count()==64&&mesh_trails_series(42,now,out,64)==64&&out[0].latitude_i==16&&out[63].latitude_i==79);assert(mesh_trails_series(42,now,out,2)==2&&out[0].latitude_i==78&&out[1].latitude_i==79);
 assert(!mesh_trails_series(42,now,NULL,0)&&!mesh_trails_series(42,0,out,64));mesh_trails_forget(42);assert(!mesh_trails_count());
 for(unsigned i=1;i<=MESH_TRAIL_LIMIT+1;i++)assert(add(i,0,0,now,0));assert(mesh_trails_count()==MESH_TRAIL_LIMIT&&!mesh_trails_series(1,now,out,64)&&mesh_trails_series(MESH_TRAIL_LIMIT+1,now,out,64)==1);
 assert(!add(9999,0,0,now-1,0)&&!mesh_trails_series(9999,now,out,64)); // old cached anchors cannot evict newer fixes
 // Stationary coalescing protects a recently heard point from global age eviction.
 mesh_trails_reset();assert(add(1,0,0,now-100,0)&&add(2,0,0,now-90,0)&&add(1,0,0,now,0));for(unsigned i=3;i<=MESH_TRAIL_LIMIT+1;i++)assert(add(i,0,0,now-80,0));assert(mesh_trails_series(1,now,out,64)==1&&!mesh_trails_series(2,now,out,64));
 // Reports before the badge clock is set stay hidden, and far-future entries cannot poison later valid data.
 mesh_trails_reset();assert(mesh_trails_add(1,0,0,now+100000,0,0));assert(!mesh_trails_series(1,now,out,64));assert(add(1,1,0,now,0));assert(mesh_trails_count()==1&&mesh_trails_series(1,now,out,64)==1&&out[0].latitude_i==1);
 mesh_trails_reset();assert(add(1,0,0,now+30,0)&&!mesh_trails_series(1,now,out,64)&&mesh_trails_series(1,now+30,out,64)==1);mesh_trails_reset();
 puts("PASS: dated/valid/zero coordinates, duplicate/stale/conflicting fixes, stationary first/last times, 1h gap and precision breaks, 24h/64-point/2048-point bounds, recent stationary retention, clock recovery and forget/reset");
}
