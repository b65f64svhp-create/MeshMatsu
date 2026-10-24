#define main message_tests
#include "test_direct_messages.c"
#undef main
static protobuf_field_t field(const uint8_t *data,size_t size,uint32_t tag){
 size_t n=0;protobuf_field_t f;while(n<size){assert(read_field(data,size,&n,&f));if(f.field==tag)return f;}assert(false);return (protobuf_field_t){0};
}
static void config(uint32_t role){
 uint8_t raw[]={8,0,0x20,7,0x5a,4,'U','T','C','0',0xa0,6,123},cfg[80];size_t n=0;raw[1]=role;
 assert(append_bytes_field(cfg,sizeof(cfg),&n,1,raw,sizeof(raw)));decode_config(cfg,n);
}
static void ready(void){
 reset();lora_config.loaded=true;session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;
 config(0);radio_metadata.loaded=true;strcpy(radio_metadata.firmware_version,"2.7.18");
}
static void reply(uint32_t role,uint32_t id,uint32_t sender){
 uint8_t device[]={8,role},cfg[40],admin[60],data[90];size_t c=0,a=0,d=0;
 assert(append_bytes_field(cfg,sizeof(cfg),&c,1,device,sizeof(device)));
 assert(append_bytes_field(admin,sizeof(admin),&a,6,cfg,c));
 assert(append_varint(data,sizeof(data),&d,8)&&append_varint(data,sizeof(data),&d,6));
 assert(append_bytes_field(data,sizeof(data),&d,2,admin,a));data[d++]=0x35;memcpy(data+d,&id,4);d+=4;
 decode_data_message(data,d,sender,0,local_node_number,400,0);
}
#ifndef ROLE_TEST_MAIN
#define ROLE_TEST_MAIN main
#endif
int ROLE_TEST_MAIN(void){
 ready();uint32_t rev=device_config.revision;
 assert(!mesh_radio_role_supported(3)&&!mesh_radio_role_supported(4)&&!mesh_radio_role_supported(999));
 strcpy(radio_metadata.firmware_version,"2.6.7");assert(!mesh_radio_role_supported(12));strcpy(radio_metadata.firmware_version,"2.7.18");
 assert(mesh_radio_set_role(1,rev));assert(role_save.state==MESH_ROLE_SAVING&&device_config.role==0);
 protobuf_field_t packet=field(sent_bytes+4,sent_size-4,1),to=field(packet.value,packet.length,2),data=field(packet.value,packet.length,4);
 uint32_t destination;memcpy(&destination,to.value,4);assert(destination==local_node_number);
 protobuf_field_t admin=field(data.value,data.length,2),cfg=field(admin.value,admin.length,34),raw=field(cfg.value,cfg.length,1);
 assert(field(data.value,data.length,1).integer==6&&field(raw.value,raw.length,1).integer==1);
 assert(field(raw.value,raw.length,4).integer==7&&field(raw.value,raw.length,100).integer==123);
 protobuf_field_t tz=field(raw.value,raw.length,11);assert(tz.length==4&&!memcmp(tz.value,"UTC0",4));
 assert(field(admin.value,admin.length,101).length==4);
 assert(!mesh_radio_set_role(2,rev));ack(local_node_number,role_write_id,0);assert(role_save.state==MESH_ROLE_SAVING);
 fake_time+=1500001;mesh_radio_poll();assert(role_read_id);
 const uint8_t malformed[]={0x32,4,0x0a,2,8,0x80};
 parse_admin_response(malformed,sizeof(malformed),role_read_id);
 assert(role_save.state==MESH_ROLE_SAVING&&device_config.revision==rev);
 uint32_t id=role_read_id;reply(1,id+1,local_node_number);assert(role_save.state==MESH_ROLE_SAVING);
 reply(1,id,0xaabbccdd);assert(role_save.state==MESH_ROLE_SAVING);
 reply(1,id,local_node_number);assert(role_save.state==MESH_ROLE_SAVED&&device_config.role==1);
 ready();assert(!mesh_radio_set_role(1,device_config.revision+1));
 assert(mesh_radio_set_role(2,device_config.revision));begin_radio_reconnect();assert(role_save.state==MESH_ROLE_SAVING&&!device_config.loaded);
 // A reboot does not re-send the mutation; only a new local read can verify it.
 status.synchronized=true;status.reconnecting=false;fake_time+=2000000;mesh_radio_poll();assert(role_read_id);
 reply(2,role_read_id,local_node_number);assert(role_save.state==MESH_ROLE_SAVED);
 ready();assert(mesh_radio_set_role(2,device_config.revision));fake_time+=2000000;mesh_radio_poll();
 reply(0,role_read_id,local_node_number);assert(role_save.state==MESH_ROLE_FAILED); // Firmware normalization/rejection.
 ready();assert(mesh_radio_set_role(1,device_config.revision));ack(local_node_number,role_write_id,33);assert(role_save.state==MESH_ROLE_FAILED);
 ready();short_write=true;assert(!mesh_radio_set_role(1,device_config.revision)&&role_save.state==MESH_ROLE_IDLE);short_write=false;
 ready();session_passkey_received_us=fake_time-240000001;assert(!mesh_radio_set_role(1,device_config.revision));
 ready();assert(mesh_radio_set_role(1,device_config.revision));fake_time+=60000001;mesh_radio_poll();assert(role_save.state==MESH_ROLE_TIMEOUT);
 assert(!message_count&&!reaction_count);
 puts("PASS: role support/version gating, local admin encoding, untouched/unknown device fields, stale revision, no optimistic success, matched readback, reboot recovery, normalization, NAK, expiry, write failure and timeout");
 return 0;
}
