#include <assert.h>
#include "range_counter.inc"
int main(void){
 uint32_t seq=0;assert(mesh_range_sequence((const uint8_t*)"seq 4294967295",14,&seq)&&seq==UINT32_MAX);
 assert(mesh_range_sequence((const uint8_t*)"seq 0",5,&seq)&&!seq);assert(!mesh_range_sequence((const uint8_t*)"seq 4294967296",14,&seq));assert(!mesh_range_sequence((const uint8_t*)"seq -1",6,&seq));assert(!mesh_range_sequence((const uint8_t*)"seq 1\n",6,&seq));assert(!mesh_range_sequence((const uint8_t*)"seq 1\0",6,&seq));assert(!mesh_range_sequence((const uint8_t*)"Seq 1",5,&seq));
 mesh_range_counter_t c={0};assert(mesh_range_count(&c,100,false)==MESH_RANGE_NEW&&c.received==1&&!c.gaps&&c.first==100);
 assert(mesh_range_count(&c,103,false)==MESH_RANGE_NEW&&c.received==2&&c.gaps==2);
 assert(mesh_range_count(&c,101,false)==MESH_RANGE_LATE&&c.received==3&&c.gaps==1&&c.late==1);
 assert(mesh_range_count(&c,101,false)==MESH_RANGE_DUPLICATE&&c.received==3&&c.gaps==1);
 assert(mesh_range_count(&c,102,true)==MESH_RANGE_DUPLICATE&&c.received==3&&c.gaps==1);
 assert(mesh_range_count(&c,102,false)==MESH_RANGE_LATE&&c.received==4&&!c.gaps);
 assert(mesh_range_count(&c,99,false)==MESH_RANGE_OUTSIDE&&c.received==4&&!c.gaps);
 assert(mesh_range_count(&c,1,false)==MESH_RANGE_SEGMENT&&c.segment==2&&c.received==5&&!c.gaps);
 assert(mesh_range_count(&c,1,true)==MESH_RANGE_DUPLICATE&&c.segment==2);
 assert(mesh_range_count(&c,500000,false)==MESH_RANGE_SEGMENT&&c.segment==3&&!c.gaps);
 c=(mesh_range_counter_t){0};mesh_range_count(&c,UINT32_MAX-1,false);mesh_range_count(&c,1,false);assert(c.gaps==2&&c.segment==1);mesh_range_count(&c,UINT32_MAX,false);mesh_range_count(&c,0,false);assert(!c.gaps&&c.received==4&&c.late==2);
 c=(mesh_range_counter_t){0};mesh_range_count(&c,0,false);for(unsigned i=1;i<5000;i++)mesh_range_count(&c,i,false);assert(c.received==5000&&!c.gaps);assert(mesh_range_count(&c,2,false)==MESH_RANGE_OUTSIDE);assert(mesh_range_count(&c,4997,false)==MESH_RANGE_DUPLICATE);
 return 0;
}
