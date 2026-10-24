#define ROLE_TEST_MAIN existing_role_tests
#include "test_role.c"
#undef main
static void behavior_reply(uint32_t role,uint32_t mode,uint32_t interval,uint32_t id,uint32_t sender){
 uint8_t raw[80],cfg[100],admin[120],data[150];size_t n=0,c=0,a=0,d=0;
 assert(append_varint(raw,sizeof(raw),&n,8)&&append_varint(raw,sizeof(raw),&n,role));
 assert(append_varint(raw,sizeof(raw),&n,48)&&append_varint(raw,sizeof(raw),&n,mode));
 assert(append_varint(raw,sizeof(raw),&n,56)&&append_varint(raw,sizeof(raw),&n,interval));
 assert(append_bytes_field(cfg,sizeof(cfg),&c,1,raw,n)&&append_bytes_field(admin,sizeof(admin),&a,6,cfg,c));
 assert(append_varint(data,sizeof(data),&d,8)&&append_varint(data,sizeof(data),&d,6)&&append_bytes_field(data,sizeof(data),&d,2,admin,a));
 data[d++]=0x35;memcpy(data+d,&id,4);d+=4;decode_data_message(data,d,sender,0,local_node_number,400,0);
}
int main(void){
 for(unsigned role=0;role<13;role++)for(unsigned mode=0;mode<7;mode++)assert(mesh_radio_behavior_supported(role,mode)==(mode<=5&&(mode!=1||role==4)&&(mode!=4||role==5||role==6||role==10)));
 ready();uint32_t rev=device_config.revision;size_t writes=write_count;
 assert(!mesh_radio_set_behavior(2,3599,rev)&&!mesh_radio_set_behavior(6,3600,rev)&&!mesh_radio_set_behavior(1,3600,rev)&&!mesh_radio_set_behavior(4,3600,rev)&&!mesh_radio_set_behavior(2,3600,rev+1)&&write_count==writes);
 assert(mesh_radio_set_behavior(3,10800,rev)&&write_count==writes+1&&role_save.state==MESH_ROLE_SAVING);
 protobuf_field_t pkt=field(sent_bytes+4,sent_size-4,1),data=field(pkt.value,pkt.length,4),admin=field(data.value,data.length,2),cfg=field(admin.value,admin.length,34),raw=field(cfg.value,cfg.length,1);
 assert(field(raw.value,raw.length,1).integer==0&&field(raw.value,raw.length,6).integer==3&&field(raw.value,raw.length,7).integer==10800);
 assert(field(raw.value,raw.length,4).integer==7&&field(raw.value,raw.length,100).integer==123&&field(raw.value,raw.length,11).length==4&&field(admin.value,admin.length,101).length==4);
 assert(!mesh_radio_set_behavior(2,3600,rev)&&!mesh_radio_set_role(1,rev));ack(local_node_number,role_write_id,0);assert(role_save.state==MESH_ROLE_SAVING);
 fake_time+=1500001;mesh_radio_poll();assert(role_read_id);uint32_t id=role_read_id;
 behavior_reply(0,3,10800,id+1,local_node_number);assert(role_save.state==MESH_ROLE_SAVING);
 behavior_reply(0,3,10800,id,0xabcdef01);assert(role_save.state==MESH_ROLE_SAVING);
 behavior_reply(0,3,10800,id,local_node_number);assert(role_save.state==MESH_ROLE_SAVED&&device_config.node_info_broadcast_secs==10800&&device_config.rebroadcast_mode==3);
 ready();assert(mesh_radio_set_behavior(2,3600,device_config.revision));fake_time+=1600000;mesh_radio_poll();behavior_reply(0,0,3600,role_read_id,local_node_number);assert(role_save.state==MESH_ROLE_FAILED);
 ready();assert(mesh_radio_set_behavior(2,3600,device_config.revision));begin_radio_reconnect();assert(role_save.state==MESH_ROLE_SAVING);status.synchronized=true;status.reconnecting=false;fake_time+=2000000;mesh_radio_poll();behavior_reply(0,2,3600,role_read_id,local_node_number);assert(role_save.state==MESH_ROLE_SAVED);
 ready();assert(mesh_radio_set_behavior(2,3600,device_config.revision));ack(local_node_number,role_write_id,33);assert(role_save.state==MESH_ROLE_FAILED);
 ready();assert(mesh_radio_set_behavior(2,3600,device_config.revision));fake_time+=60000001;mesh_radio_poll();assert(role_save.state==MESH_ROLE_TIMEOUT);
 ready();session_passkey_received_us=fake_time-240000001;assert(!mesh_radio_set_behavior(2,3600,device_config.revision));
 ready();const uint8_t duplicate[]={10,4,48,2,48,3};decode_config(duplicate,sizeof(duplicate));assert(!device_config.loaded&&!mesh_radio_set_behavior(2,3600,device_config.revision));
 puts("PASS: role-filtered rebroadcast modes, minimum announcement interval, preserved device/unknown fields, single local admin write, stale/duplicate/expired rejection, ACK insufficiency, matched readback, normalization, reboot recovery, NAK and timeout");
}
