#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define EXT_RAM_BSS_ATTR
#include "telemetry_data.h"
#include "telemetry_data.inc"
static mesh_tlm_sample_t sample(uint32_t node,uint8_t kind,uint32_t packet,double value){return (mesh_tlm_sample_t){.node=node,.kind=kind,.packet=packet,.present=1,.time=1800000000+packet,.received_us=1000000,.value={value}};}
int main(void){
 mesh_tlm_sample_t a=sample(42,3,1,-20),b,out[96];assert(mesh_tlm_add(&a)&&mesh_tlm_count()==1);
 assert(!mesh_tlm_add(&a));b=a;b.packet=2;assert(!mesh_tlm_add(&b));b.time++;assert(mesh_tlm_add(&b));
 b=sample(42,3,3,0);b.present=2;b.value[1]=0;assert(mesh_tlm_add(&b));
 assert(mesh_tlm_latest(42,3,1,&a)&&a.value[0]==-20&&a.packet==2);
 assert(mesh_tlm_latest(42,3,2,&a)&&a.value[1]==0&&a.packet==3);
 assert(mesh_tlm_fields(42,3)==3&&mesh_tlm_series(42,3,out,96)==3);
 assert(out[0].packet==1&&out[1].packet==2&&out[2].present==2); // gap is preserved
 b=sample(42,3,4,9);b.time=1700000000;assert(!mesh_tlm_add(&b));b.flags=MESH_TLM_CACHED;assert(!mesh_tlm_add(&b));
 b.flags=MESH_TLM_CLOCK_SKEW;assert(mesh_tlm_add(&b)); // bad clocks cannot evict by timestamp
 b=sample(42,5,5,3);b.present=2;b.value[1]=-120;assert(mesh_tlm_add(&b));
 b=sample(42,4,6,0);assert(mesh_tlm_add(&b));b.value[0]=.5;assert(!mesh_tlm_valid(&b));
 b=sample(42,3,7,NAN);assert(!mesh_tlm_valid(&b));b.value[0]=-300;assert(!mesh_tlm_valid(&b));b.value[0]=1;b.present=1u<<22;assert(!mesh_tlm_valid(&b));
 mesh_tlm_reset();for(unsigned p=1;p<=110;p++){a=sample(42,2,p,p%100);assert(mesh_tlm_add(&a));}
 assert(mesh_tlm_count()==96&&mesh_tlm_series(42,2,out,96)==96&&out[0].packet==15&&out[95].packet==110);
 assert(mesh_tlm_series(42,2,out,2)==2&&out[0].packet==109&&out[1].packet==110);
 a=sample(42,2,111,11);assert(!mesh_tlm_import(&a,1)); // old disk data cannot evict newer full series
 mesh_tlm_reset();a=sample(42,3,500,20);assert(mesh_tlm_add(&a));a=sample(42,3,1,-10);assert(mesh_tlm_import(&a,1));
 assert(mesh_tlm_series(42,3,out,96)==2&&out[0].packet==1&&(out[0].flags&MESH_TLM_RESTORED)&&!out[0].received_us&&out[1].packet==500);
 assert(mesh_tlm_latest(42,3,1,&a)&&a.packet==500);
 mesh_tlm_reset();a=sample(42,2,1,50);a.flags=MESH_TLM_CACHED;assert(mesh_tlm_add(&a));assert(!mesh_tlm_add(&a));a.packet=2;a.time++;assert(!mesh_tlm_add(&a));
 mesh_tlm_reset();for(unsigned i=0;i<MESH_TLM_LIMIT+1;i++){a=sample(i+1,2,i+1,50);assert(mesh_tlm_add(&a));}
 assert(mesh_tlm_count()==4096&&!mesh_tlm_latest(1,2,0,&a)&&mesh_tlm_latest(4097,2,0,&a));
 mesh_tlm_sample_t *all=malloc(sizeof(*all)*4096);assert(all);assert(!mesh_tlm_snapshot(all,4095));assert(mesh_tlm_snapshot(all,4096)==4096&&all[0].node==2&&all[4095].node==4097);free(all);
 puts("PASS: signed/zero/missing metrics, stale/cache/packet dedup, per-field ages, graph gaps, 96/4096 bounds, restored-before-live ordering, complete ordered snapshots");
}
