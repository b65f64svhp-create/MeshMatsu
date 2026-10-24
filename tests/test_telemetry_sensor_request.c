#define TELEMETRY_REQUEST_HELPERS_ONLY
#include "test_telemetry_request.c"
#undef main
static void sensor_reply(uint32_t node,uint32_t id,uint8_t kind){uint8_t m[8],tel[20];size_t n=0,t=0;float v=12;assert(append_varint(m,sizeof(m),&n,kind==4?8:13));if(kind==4){assert(append_varint(m,sizeof(m),&n,0));}else{memcpy(m+n,&v,4);n+=4;}assert(append_bytes_field(tel,sizeof(tel),&t,kind,m,n));decode_telemetry(node,local_node_number,333+id,id,tel,t);}
int main(void){
 for(unsigned kind=3;kind<=5;kind++){
  ready_request();assert(mesh_radio_request_telemetry_kind(1,kind)==MESH_TELEMETRY_START_OK);
  mesh_telemetry_request_t r=request_result(1);assert(r.kind==kind&&r.state==MESH_TELEMETRY_WAITING);
  protobuf_field_t packet=tf(sent_bytes+4,sent_size-4,1),data=tf(packet.value,packet.length,4),tel=tf(data.value,data.length,2),sub=tf(tel.value,tel.length,kind);
  assert(sub.length==0&&tf(data.value,data.length,3).integer==1&&tf(packet.value,packet.length,9).integer==5);
  ack(1,r.request_id,0);sensor_reply(1,r.request_id,kind==3?4:3);assert(request_result(1).state==MESH_TELEMETRY_WAITING);
  sensor_reply(2,r.request_id,kind);assert(request_result(1).state==MESH_TELEMETRY_WAITING);
  sensor_reply(1,r.request_id,kind);assert(request_result(1).state==MESH_TELEMETRY_REPLY);
  assert(mesh_radio_request_telemetry_kind(2,kind)==MESH_TELEMETRY_START_COOLDOWN);
  size_t writes=write_count;fake_time+=120000000;poll_request();assert(write_count==writes);
 }
 ready_request();assert(mesh_radio_request_telemetry_kind(1,1)==MESH_TELEMETRY_START_INVALID&&mesh_radio_request_telemetry_kind(1,6)==MESH_TELEMETRY_START_INVALID);
 mesh_tlm_sample_t s={.node=local_node_number,.kind=3,.present=(1u<<15)|(1u<<21),.value={[15]=5,[21]=-20}};assert(mesh_tlm_add(&s));
 assert(mesh_radio_request_telemetry_kind(1,3)==MESH_TELEMETRY_START_OK);
 protobuf_field_t packet=tf(sent_bytes+4,sent_size-4,1),data=tf(packet.value,packet.length,4),tel=tf(data.value,data.length,2),sub=tf(tel.value,tel.length,3);
 protobuf_field_t a=tf(sub.value,sub.length,16),b=tf(sub.value,sub.length,22);float av,bv;memcpy(&av,a.value,4);memcpy(&bv,b.value,4);assert(a.wire==5&&b.wire==5&&av==5&&bv==-20); // multi-byte field tags must survive
 ready_request();s.flags=MESH_TLM_RESTORED;assert(mesh_tlm_import(&s,1));assert(mesh_radio_request_telemetry_kind(1,3)==MESH_TELEMETRY_START_OK);
 packet=tf(sent_bytes+4,sent_size-4,1);data=tf(packet.value,packet.length,4);tel=tf(data.value,data.length,2);assert(tf(tel.value,tel.length,3).length==0); // don't rebroadcast saved sensor values as new
 assert(!message_count&&!reaction_count);puts("PASS: typed sensor requests, kind-matched replies, delivery ACK isolation, cooldown/no resends, genuine live values including >15 tags, no fabricated/restored values");
}
