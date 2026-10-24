#define main message_tests
#include "test_direct_messages.c"
#undef main
#include "../main/node_details.c"
static const uint8_t metadata[]={0x0a,6,'2','.','7','.','1','8',0x48,0xff,1,0x38,0};
static void report(bool admin,uint32_t sender){
 uint8_t payload[150],data[200],packet[250],frame[300];size_t a=0,d=0,p=0,f=0;
 if(admin){
  assert(append_bytes_field(payload,sizeof(payload),&a,13,metadata,sizeof(metadata)));
  assert(append_varint(data,sizeof(data),&d,8)&&append_varint(data,sizeof(data),&d,6));
  assert(append_bytes_field(data,sizeof(data),&d,2,payload,a));
  packet[p++]=0x0d;memcpy(packet+p,&sender,4);p+=4;
  packet[p++]=0x15;memcpy(packet+p,&local_node_number,4);p+=4;
  assert(append_bytes_field(packet,sizeof(packet),&p,4,data,d));
  assert(append_bytes_field(frame,sizeof(frame),&f,2,packet,p));
 }else assert(append_bytes_field(frame,sizeof(frame),&f,13,metadata,sizeof(metadata)));
 received_bytes[0]=0x94;received_bytes[1]=0xc3;received_bytes[2]=f>>8;received_bytes[3]=f;
 memcpy(received_bytes+4,frame,f);received_offset=0;received_size=f+4;mesh_radio_poll();
}
static protobuf_field_t field(const uint8_t *data,size_t size,uint32_t number){
 protobuf_field_t f;size_t n=0;while(n<size){assert(read_field(data,size,&n,&f));if(f.field==number)return f;}assert(false);return (protobuf_field_t){0};
}
int main(void){
 reset();lora_config.loaded=true;assert(!mesh_radio_metadata().loaded);
 assert(mesh_radio_request_metadata()&&radio_metadata.pending);
 protobuf_field_t packet=field(sent_bytes+4,sent_size-4,1),to=field(packet.value,packet.length,2),data=field(packet.value,packet.length,4);
 uint32_t target;memcpy(&target,to.value,4);assert(target==local_node_number);
 assert(field(data.value,data.length,1).integer==6);
 protobuf_field_t admin=field(data.value,data.length,2);assert(field(admin.value,admin.length,12).integer==1);
 size_t writes=write_count;mesh_radio_request_metadata();assert(write_count==writes);
 report(false,0);assert(radio_metadata.loaded&&!radio_metadata.pending&&!strcmp(radio_metadata.firmware_version,"2.7.18"));
 assert(radio_metadata.hardware_model==255&&radio_metadata.role==0&&status.frames_received==1);
 assert(!strcmp(mesh_hardware_model_name(255),"PRIVATE_HW")&&!strcmp(mesh_hardware_model_name(99999),"Unknown model"));
 radio_metadata=(mesh_radio_metadata_t){0};report(true,0xaabbccdd);assert(!radio_metadata.loaded);
 report(true,local_node_number);assert(radio_metadata.loaded);
 const uint8_t broken[]={0x0a,0x7f,'x'};decode_metadata(broken,sizeof(broken));assert(!strcmp(radio_metadata.firmware_version,"2.7.18"));
 fake_time+=4000000;mesh_radio_request_metadata();fake_time+=11000000;mesh_radio_poll();assert(!radio_metadata.pending);
 assert(!message_count&&!reaction_count);begin_radio_reconnect();assert(!radio_metadata.loaded&&!mesh_radio_request_metadata());
 puts("PASS: exact local read-only metadata request, direct/admin responses, foreign reply isolation, unknown hardware IDs, malformed data, timeout/rate limit and reconnect invalidation");
}
