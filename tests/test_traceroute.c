#define CHANNEL_TEST_MAIN channel_tests
#include "test_channel_messaging.c"

static protobuf_field_t find_trace_field(const uint8_t *data,size_t size,uint32_t number){
 size_t pos=0;protobuf_field_t field;
 while(pos<size){assert(read_field(data,size,&pos,&field));if(field.field==number)return field;}
 assert(!"Missing trace field");return (protobuf_field_t){0};
}
static void trace_reply(uint32_t sender,uint32_t dest,uint32_t request,const uint8_t *route,size_t size,bool include_payload){
 uint8_t data[400]={8,70},packet[450],frame[500];size_t d=2,p=0,f=0;
 if(include_payload)assert(append_bytes_field(data,sizeof(data),&d,2,route,size));
 data[d++]=0x35;memcpy(data+d,&request,4);d+=4;
 packet[p++]=0x0d;memcpy(packet+p,&sender,4);p+=4;
 packet[p++]=0x15;memcpy(packet+p,&dest,4);p+=4;
 assert(append_bytes_field(packet,sizeof(packet),&p,4,data,d));
 uint32_t id=900;packet[p++]=0x35;memcpy(packet+p,&id,4);p+=4;
 assert(append_bytes_field(frame,sizeof(frame),&f,2,packet,p));decode_from_radio(frame,f);
}
static void check_request(uint32_t node,uint32_t id){
 size_t size=((size_t)sent_bytes[2]<<8)|sent_bytes[3];assert(size+4==sent_size);
 protobuf_field_t packet=find_trace_field(sent_bytes+4,size,1),to=find_trace_field(packet.value,packet.length,2);
 uint32_t actual;memcpy(&actual,to.value,4);assert(actual==node);
 protobuf_field_t data=find_trace_field(packet.value,packet.length,4);
 assert(find_trace_field(data.value,data.length,1).integer==70);
 assert(find_trace_field(data.value,data.length,2).length==0);
 assert(find_trace_field(data.value,data.length,3).integer==1);
 assert(find_trace_field(packet.value,packet.length,9).integer==5);
 assert(find_trace_field(packet.value,packet.length,10).integer==1);
 protobuf_field_t packet_id=find_trace_field(packet.value,packet.length,6);memcpy(&actual,packet_id.value,4);assert(actual==id&&id);
}
static size_t route_payload(uint8_t *out,bool packed){
 size_t n=0;uint32_t forward[]={0x11111111,0x22222222},back[]={0x33333333};
 int32_t snr[]={-7,0,-128},reverse[]={-8,15};
 if(packed){
  assert(append_bytes_field(out,300,&n,1,(uint8_t*)forward,sizeof(forward)));
  uint8_t values[40];size_t v=0;for(size_t i=0;i<3;i++)assert(append_varint(values,sizeof(values),&v,(uint64_t)(int64_t)snr[i]));
  assert(append_bytes_field(out,300,&n,2,values,v));
  assert(append_bytes_field(out,300,&n,3,(uint8_t*)back,sizeof(back)));
  v=0;for(size_t i=0;i<2;i++)assert(append_varint(values,sizeof(values),&v,(uint64_t)(int64_t)reverse[i]));
  assert(append_bytes_field(out,300,&n,4,values,v));
 }else{
  for(size_t i=0;i<2;i++){out[n++]=0x0d;memcpy(out+n,&forward[i],4);n+=4;}
  for(size_t i=0;i<3;i++){out[n++]=0x10;assert(append_varint(out,300,&n,(uint64_t)(int64_t)snr[i]));}
  out[n++]=0x1d;memcpy(out+n,back,4);n+=4;
  for(size_t i=0;i<2;i++){out[n++]=0x20;assert(append_varint(out,300,&n,(uint64_t)(int64_t)reverse[i]));}
 }
 return n;
}
int main(void){
 channel_tests();reset();lora_config.loaded=true;lora_config.hop_limit=5;
 uint32_t node=0xaabbccdd,other=0xabcdef01;mesh_trace_result_t result;
 size_t writes=write_count;assert(mesh_radio_start_traceroute(0)==MESH_TRACE_START_INVALID);
 assert(mesh_radio_start_traceroute(local_node_number)==MESH_TRACE_START_INVALID&&write_count==writes);
 assert(mesh_radio_start_traceroute(node)==MESH_TRACE_START_OK);
 assert(mesh_radio_get_traceroute(node,&result)&&result.state==MESH_TRACE_WAITING);
 uint32_t id=result.request_id;check_request(node,id);
 assert(mesh_radio_start_traceroute(other)==MESH_TRACE_START_BUSY);
 routing_from_radio(local_node_number,id,0);routing_from_radio(node,id,0);
 assert(mesh_radio_get_traceroute(node,&result)&&result.state==MESH_TRACE_WAITING);
 uint8_t payload[300];size_t size=route_payload(payload,true);
 trace_reply(other,local_node_number,id,payload,size,true);
 trace_reply(node,other,id,payload,size,true);
 trace_reply(node,local_node_number,id+1,payload,size,true);
 assert(mesh_radio_get_traceroute(node,&result)&&result.state==MESH_TRACE_WAITING);
 const uint8_t malformed[]={0x0a,3,1,2,3};trace_reply(node,local_node_number,id,malformed,sizeof(malformed),true);
 uint8_t too_many[40]={0x0a,36};trace_reply(node,local_node_number,id,too_many,38,true);
 const uint8_t bad_varint[]={0x12,1,0x80};trace_reply(node,local_node_number,id,bad_varint,sizeof(bad_varint),true);
 assert(mesh_radio_get_traceroute(node,&result)&&result.state==MESH_TRACE_WAITING);
 uint32_t revision=mesh_radio_history_revision();trace_reply(node,local_node_number,id,payload,size,true);
 assert(mesh_radio_get_traceroute(node,&result)&&result.state==MESH_TRACE_COMPLETE);
 assert(result.route_count==2&&result.return_count==1&&result.snr_count==3&&result.return_snr_count==2);
 assert(result.route[0]==0x11111111&&result.route[1]==0x22222222&&result.return_route[0]==0x33333333);
 assert(result.snr[0]==-7&&result.snr[1]==0&&result.snr[2]==-128&&result.return_snr[0]==-8);
 assert(mesh_radio_history_revision()!=revision&&!message_count&&!reaction_count&&!mesh_radio_unread_total(false)&&!mesh_radio_unread_total(true));
 trace_reply(node,local_node_number,id,NULL,0,false);assert(mesh_radio_get_traceroute(node,&result)&&result.route_count==2); // Duplicate cannot replace results.
 assert(mesh_radio_start_traceroute(other)==MESH_TRACE_START_COOLDOWN);
 fake_time+=30000000;assert(mesh_radio_start_traceroute(other)==MESH_TRACE_START_OK);
 assert(mesh_radio_get_traceroute(other,&result));id=result.request_id;
 size=route_payload(payload,false);trace_reply(other,local_node_number,id,payload,size,true);
 assert(mesh_radio_get_traceroute(other,&result)&&result.state==MESH_TRACE_COMPLETE&&result.snr[0]==-7&&result.return_count==1);
 // Empty proto3 payload is a valid older/direct response; return path remains absent.
 fake_time+=30000000;assert(mesh_radio_start_traceroute(node)==MESH_TRACE_START_OK);
 assert(mesh_radio_get_traceroute(node,&result));id=result.request_id;
 trace_reply(node,local_node_number,id,NULL,0,false);
 assert(mesh_radio_get_traceroute(node,&result)&&result.state==MESH_TRACE_COMPLETE&&!result.route_count&&!result.return_count&&!result.return_snr_count);
 fake_time+=30000000;assert(mesh_radio_start_traceroute(node)==MESH_TRACE_START_OK);
 assert(mesh_radio_get_traceroute(node,&result));id=result.request_id;
 routing_from_radio(other,id,1);assert(mesh_radio_get_traceroute(node,&result)&&result.state==MESH_TRACE_WAITING);
 routing_from_radio(local_node_number,id,1);assert(mesh_radio_get_traceroute(node,&result)&&result.state==MESH_TRACE_ERROR&&result.error_reason==1);
 // A timeout changes the UI revision, and a correlated late reply is still useful.
 fake_time+=30000000;assert(mesh_radio_start_traceroute(node)==MESH_TRACE_START_OK);
 assert(mesh_radio_get_traceroute(node,&result));id=result.request_id;
 revision=mesh_radio_history_revision();fake_time+=120000000;mesh_radio_poll();
 assert(mesh_radio_get_traceroute(node,&result)&&result.state==MESH_TRACE_TIMEOUT&&mesh_radio_history_revision()!=revision);
 assert(mesh_radio_traceroute_elapsed(node)>=120);
 trace_reply(node,local_node_number,id,NULL,0,true);assert(mesh_radio_get_traceroute(node,&result)&&result.state==MESH_TRACE_COMPLETE);
 assert(mesh_radio_start_traceroute(other)==MESH_TRACE_START_OK);begin_radio_reconnect();
 assert(mesh_radio_get_traceroute(other,&result)&&result.state==MESH_TRACE_INTERRUPTED);
 assert(mesh_radio_get_traceroute(node,&result)&&result.state==MESH_TRACE_COMPLETE);
 assert(mesh_radio_start_traceroute(node)==MESH_TRACE_START_OFFLINE);
 reset();short_write=true;assert(mesh_radio_start_traceroute(node)==MESH_TRACE_START_WRITE_FAILED&&!mesh_radio_get_traceroute(node,&result));short_write=false;
 // Cache is bounded and viewing never transmits.
 for(uint32_t i=1;i<=9;i++){
  fake_time+=30000000;assert(mesh_radio_start_traceroute(i)==MESH_TRACE_START_OK);
  assert(mesh_radio_get_traceroute(i,&result));trace_reply(i,local_node_number,result.request_id,NULL,0,false);
 }
 assert(!mesh_radio_get_traceroute(1,&result));writes=write_count;
 for(uint32_t i=2;i<=9;i++)assert(mesh_radio_get_traceroute(i,&result)&&result.node==i&&result.state==MESH_TRACE_COMPLETE);
 assert(write_count==writes&&!mesh_radio_get_traceroute(node,NULL));
 puts("PASS: traceroute request fields, matched packed/unpacked replies, negative/zero/unknown SNR, empty legacy results, malformed/stale isolation, ACK vs reply, NAK/timeout/reconnect, UART failure and bounded per-node cache");
}
