#define TELEMETRY_HELPERS_ONLY
#include "test_telemetry.c"
#undef main
static void vf(uint8_t *raw,size_t *n,unsigned tag,float value){assert(append_varint(raw,100,n,(tag<<3)|5));memcpy(raw+*n,&value,4);*n+=4;}
static void vi(uint8_t *raw,size_t *n,unsigned tag,uint64_t value){assert(append_varint(raw,100,n,tag<<3)&&append_varint(raw,100,n,value));}
int main(void){
 reset();uint8_t raw[100];size_t n=0;mesh_tlm_sample_t s;
 vf(raw,&n,1,-20);vf(raw,&n,2,0);vf(raw,&n,6,-120);vi(raw,&n,7,0);vf(raw,&n,22,-5);
 telemetry_packet(42,MESH_BROADCAST_NODE,1,0,3,raw,n);
 assert(mesh_tlm_latest(42,3,0,&s)&&s.value[0]==-20&&s.value[1]==0&&s.value[5]==-120&&s.value[21]==-5&&s.present==((1u<<21)|99));
 assert(!mesh_tlm_latest(42,2,0,&s));size_t count=mesh_tlm_count();
 telemetry_packet(42,MESH_BROADCAST_NODE,1,0,3,raw,n);assert(mesh_tlm_count()==count);
 n=0;vf(raw,&n,1,NAN);vf(raw,&n,2,101);vf(raw,&n,3,1013.25);vf(raw,&n,50,1);
 telemetry_packet(42,MESH_BROADCAST_NODE,2,0,3,raw,n);assert(mesh_tlm_latest(42,3,0,&s)&&s.present==4&&s.value[2]==1013.25);
 n=0;vf(raw,&n,1,1);vf(raw,&n,1,2);telemetry_packet(42,MESH_BROADCAST_NODE,3,0,3,raw,n);assert(mesh_tlm_count()==count+1);
 n=0;vi(raw,&n,1,2);telemetry_packet(42,MESH_BROADCAST_NODE,4,0,3,raw,n);assert(mesh_tlm_count()==count+1); // wrong wire
 n=0;vi(raw,&n,1,0);vi(raw,&n,13,420);telemetry_packet(42,MESH_BROADCAST_NODE,5,0,4,raw,n);
 assert(mesh_tlm_latest(42,4,0,&s)&&s.present==4097&&s.value[0]==0&&s.value[12]==420);
 n=0;vi(raw,&n,13,UINT64_C(1)<<32);telemetry_packet(42,MESH_BROADCAST_NODE,6,0,4,raw,n);assert(mesh_tlm_latest(42,4,0,&s)&&s.packet==5);
 n=0;vf(raw,&n,1,12);vf(raw,&n,2,-100);vf(raw,&n,3,0);vf(raw,&n,6,200);
 telemetry_packet(42,MESH_BROADCAST_NODE,7,0,5,raw,n);assert(mesh_tlm_latest(42,5,0,&s)&&s.present==39&&s.value[1]==-100&&s.value[2]==0&&s.value[5]==200);
 telemetry_packet(43,0xabcdef01,8,0,5,raw,n);assert(!mesh_tlm_latest(43,5,0,&s));
 nodes[0]=(mesh_node_t){.number=44,.is_ignored=true};node_count=1;telemetry_packet(44,MESH_BROADCAST_NODE,9,0,5,raw,n);assert(!mesh_tlm_latest(44,5,0,&s));
 const uint8_t broken[]={0x0d,0,0};telemetry_packet(42,MESH_BROADCAST_NODE,10,0,3,broken,sizeof(broken));assert(mesh_tlm_latest(42,3,0,&s)&&s.packet==2);
 uint8_t ignored[20];size_t z=0;vi(ignored,&z,1,42);vi(ignored,&z,11,1);decode_node(ignored,z);assert(!mesh_tlm_latest(42,3,0,&s)&&!mesh_tlm_latest(42,5,0,&s));
 assert(!write_count&&!message_count&&!reaction_count&&!mesh_radio_unread_total(true)&&!mesh_radio_unread_total(false));
 puts("PASS: actual sensor protobuf tags, signed temperature/current, valid zero, partial validity, integer overflow, duplicate/wrong-wire/truncated input, unknown fields, node/destination scope, no chat/unread/transmit effects");
}
