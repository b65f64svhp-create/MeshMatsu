#define main earlier_messaging_tests
#include "test_direct_messages.c"
#undef main
static uint8_t setup_owner[160],setup_lora[180];static size_t setup_owner_n,setup_lora_n;
static void first_radio_reset(void) {
 reset();setup_reset();neighbor_reset();mesh_radio_management_clear();cfg_state=CFG_IDLE;keys_state=MESH_KEYS_IDLE;
 channel_verify_index=-1;fake_time=1000000;
 session_passkey_size=3;memcpy(session_passkey,"OLD",3);session_passkey_received_us=fake_time;
 radio_metadata.loaded=true;strcpy(radio_metadata.firmware_version,"2.7.18.abc");
 setup_owner_n=setup_lora_n=0;
 assert(append_bytes_field(setup_owner,sizeof(setup_owner),&setup_owner_n,1,(const uint8_t*)"!12345678",9));
 assert(append_bytes_field(setup_owner,sizeof(setup_owner),&setup_owner_n,2,(const uint8_t*)"Existing Radio",14));
 assert(append_bytes_field(setup_owner,sizeof(setup_owner),&setup_owner_n,3,(const uint8_t*)"OLD",3));
 assert(append_varint(setup_owner,sizeof(setup_owner),&setup_owner_n,48)&&append_varint(setup_owner,sizeof(setup_owner),&setup_owner_n,1));
 assert(append_varint(setup_owner,sizeof(setup_owner),&setup_owner_n,800)&&append_varint(setup_owner,sizeof(setup_owner),&setup_owner_n,99));
 const uint8_t base[]={8,1,16,0,56,1,64,3,0xa0,6,77};
 memcpy(setup_lora,base,sizeof(base));setup_lora_n=sizeof(base);
}
static void first_radio_reply(unsigned part,const uint8_t *raw,size_t n,uint32_t id,bool token,uint32_t from,uint32_t to) {
 uint8_t config[220],admin[250],data[280];size_t c=0,a=0,d=0;
 if(part){assert(append_bytes_field(config,sizeof(config),&c,6,raw,n));assert(append_bytes_field(admin,sizeof(admin),&a,6,config,c));}
 else assert(append_bytes_field(admin,sizeof(admin),&a,4,raw,n));
 if(token)assert(append_bytes_field(admin,sizeof(admin),&a,101,(const uint8_t*)"FRESH",5));
 assert(append_varint(data,sizeof(data),&d,8)&&append_varint(data,sizeof(data),&d,6)&&append_bytes_field(data,sizeof(data),&d,2,admin,a));
 data[d++]=0x35;memcpy(data+d,&id,4);d+=4;
 decode_data_message(data,d,from,0,to,0,0);
}
static void first_reply(unsigned part,const uint8_t *raw,size_t n) {first_radio_reply(part,raw,n,setup_op.id,true,local_node_number,0);}
static void first_read(void) {
 assert(mesh_radio_setup_read());first_reply(0,setup_owner,setup_owner_n);
 assert(setup_radio.state==MESH_SETUP_READING&&setup_op.part==1);
 first_reply(1,setup_lora,setup_lora_n);
 assert(setup_radio.state==MESH_SETUP_READY&&setup_radio.loaded&&setup_radio.region==1&&!strcmp(setup_radio.name,"Existing Radio"));
}
static protobuf_field_t first_sent(uint32_t tag) {
 protobuf_field_t packet,data,admin,out;assert(manage_field(sent_bytes+4,sent_size-4,1,&packet)&&manage_field(packet.value,packet.length,4,&data)&&manage_field(data.value,data.length,2,&admin)&&manage_field(admin.value,admin.length,tag,&out));return out;
}
#ifndef FIRST_RADIO_TEST_MAIN
#define FIRST_RADIO_TEST_MAIN main
#endif
int FIRST_RADIO_TEST_MAIN(void) {
 first_radio_reset();size_t writes=write_count;status.synchronized=false;assert(!mesh_radio_setup_read()&&write_count==writes);status.synchronized=true;
 strcpy(radio_metadata.firmware_version,"2.7.180");assert(!mesh_radio_setup_supported()&&!mesh_radio_setup_read());strcpy(radio_metadata.firmware_version,"2.8.1");assert(!mesh_radio_setup_read());strcpy(radio_metadata.firmware_version,"2.7.18");
 assert(mesh_radio_setup_read());uint32_t id=setup_op.id;first_radio_reply(0,setup_owner,setup_owner_n,id+1,true,local_node_number,0);assert(!setup_op.part);
 first_radio_reply(0,setup_owner,setup_owner_n,id,true,0xaabbccdd,0);assert(!setup_op.part);first_radio_reply(0,setup_owner,setup_owner_n,id,true,local_node_number,0xaabbccdd);assert(!setup_op.part);
 uint8_t malformed[]={0x12,1,'x',0x12,1,'y'};first_radio_reply(0,malformed,sizeof(malformed),id,true,local_node_number,0);assert(!setup_op.part);
 first_reply(0,setup_owner,setup_owner_n);first_reply(1,setup_lora,setup_lora_n);assert(setup_radio.loaded);
 assert(!mesh_radio_setup_region(0,setup_radio.revision)&&!mesh_radio_setup_region(4,setup_radio.revision));
 assert(!mesh_radio_setup_region(3,setup_radio.revision+1));writes=write_count;assert(mesh_radio_setup_region(3,setup_radio.revision));assert(write_count==writes+1&&first_sent(5).integer==5&&!mesh_radio_admin_ready());
 assert(!mesh_radio_set_owner("No","NO"));first_reply(1,setup_lora,setup_lora_n);assert(setup_radio.state==MESH_SETUP_SAVING&&write_count==writes+2);
 protobuf_field_t config=first_sent(34),body,token=first_sent(101),f;assert(manage_field(config.value,config.length,6,&body));
 assert(manage_field(body.value,body.length,7,&f)&&f.integer==3);assert(manage_field(body.value,body.length,100,&f)&&f.integer==77);
 assert(manage_field(body.value,body.length,8,&f)&&f.integer==3&&token.length==5&&!memcmp(token.value,"FRESH",5));
 uint8_t region_raw[224];size_t region_n=body.length;memcpy(region_raw,body.value,region_n);
 ack(local_node_number,setup_op.write_id,0);assert(setup_radio.state==MESH_SETUP_SAVING);ack(local_node_number,setup_op.write_id,8);assert(setup_radio.state==MESH_SETUP_SAVING);
 fake_time+=8000000;setup_poll(fake_time);assert(setup_radio.state==MESH_SETUP_VERIFYING);uint32_t old=setup_op.id;fake_time+=5000000;setup_poll(fake_time);assert(setup_op.id!=old);
 first_radio_reply(1,region_raw,region_n,old,true,local_node_number,0);assert(setup_radio.state==MESH_SETUP_VERIFYING);
 first_reply(1,region_raw,region_n);assert(setup_radio.state==MESH_SETUP_SAVED&&setup_radio.region==3&&write_count==writes+4);
 // Names preserve user ID, licensed status, unknown/read-only fields and use a fresh token.
 writes=write_count;assert(mesh_radio_setup_names("New Radio","NEW",setup_radio.revision));first_reply(0,setup_owner,setup_owner_n);assert(setup_radio.state==MESH_SETUP_SAVING&&write_count==writes+2);
 body=first_sent(32);assert(manage_field(body.value,body.length,1,&f)&&f.length==9);assert(manage_field(body.value,body.length,6,&f)&&f.integer==1);assert(manage_field(body.value,body.length,100,&f)&&f.integer==99);
 uint8_t new_owner[224];size_t new_owner_n=body.length;memcpy(new_owner,body.value,new_owner_n);
 // A same-node reconnect can only repeat reads, never the submitted mutation.
 begin_radio_reconnect();writes=write_count;fake_time+=8000000;setup_poll(fake_time);assert(write_count==writes);
 status.synchronized=true;status.reconnecting=false;config_request_pending=false;radio_metadata.loaded=true;strcpy(radio_metadata.firmware_version,"2.7.18");setup_poll(fake_time);assert(write_count==writes+1);
 first_reply(0,new_owner,new_owner_n);assert(setup_radio.state==MESH_SETUP_SAVED&&setup_radio.loaded&&!strcmp(setup_radio.name,"New Radio"));
 // Completion reads both fields again; no write, no ACK-based success.
 session_passkey_size=5;session_passkey_received_us=fake_time;
 writes=write_count;assert(mesh_radio_setup_confirm(3,"New Radio","NEW",setup_radio.revision));first_reply(0,new_owner,new_owner_n);assert(setup_radio.state==MESH_SETUP_CHECKING);first_reply(1,region_raw,region_n);assert(setup_radio.state==MESH_SETUP_SAVED&&write_count==writes+2);
 // A fresh compare detects changed raw settings and cannot mutate them.
 first_radio_reset();first_read();assert(mesh_radio_setup_region(3,setup_radio.revision));writes=write_count;setup_lora[sizeof((uint8_t[]){8,1,16,0,56,1,64,3,0xa0,6,77})-1]=78;first_reply(1,setup_lora,setup_lora_n);assert(setup_radio.state==MESH_SETUP_CHANGED&&write_count==writes);
 first_radio_reset();first_read();assert(mesh_radio_setup_names("New","NEW",setup_radio.revision));writes=write_count;first_radio_reply(0,setup_owner,setup_owner_n,setup_op.id,false,local_node_number,0);assert(setup_radio.state==MESH_SETUP_FAILED&&write_count==writes);
 first_radio_reset();first_read();fake_time+=60000000;assert(!mesh_radio_setup_region(3,setup_radio.revision));
 first_radio_reset();assert(mesh_radio_setup_read());fake_time+=90000000;setup_poll(fake_time);assert(setup_radio.state==MESH_SETUP_TIMEOUT);
 first_radio_reset();first_read();assert(mesh_radio_setup_confirm(3,"Existing Radio","OLD",setup_radio.revision));first_reply(0,setup_owner,setup_owner_n);first_reply(1,setup_lora,setup_lora_n);assert(setup_radio.state==MESH_SETUP_CHANGED);
 first_radio_reset();first_read();assert(mesh_radio_setup_region(3,setup_radio.revision));first_reply(1,setup_lora,setup_lora_n);local_node_number++;writes=write_count;setup_poll(fake_time);assert(setup_radio.state==MESH_SETUP_INTERRUPTED&&!setup_radio.loaded&&write_count==writes);
 first_radio_reset();first_read();assert(mesh_radio_setup_names("New","NEW",setup_radio.revision));first_reply(0,setup_owner,setup_owner_n);ack(local_node_number,setup_op.write_id,9);assert(setup_radio.state==MESH_SETUP_FAILED&&setup_radio.error==9);
 // An unset region has native first-region defaults; valid repeated ignore lists survive.
 first_radio_reset();setup_lora[5]=0;
 for(unsigned i=0;i<2;i++)assert(append_varint(setup_lora,sizeof(setup_lora),&setup_lora_n,103u<<3)&&append_varint(setup_lora,sizeof(setup_lora),&setup_lora_n,100+i));
 assert(mesh_radio_setup_read());first_reply(0,setup_owner,setup_owner_n);first_reply(1,setup_lora,setup_lora_n);assert(setup_radio.loaded&&!setup_radio.region);
 assert(mesh_radio_setup_region(3,setup_radio.revision));first_reply(1,setup_lora,setup_lora_n);assert(setup_radio.state==MESH_SETUP_SAVING);
 uint8_t normalized[224];size_t nn=setup_op.wanted_size;memcpy(normalized,setup_op.wanted,nn);
 assert(append_varint(normalized,sizeof(normalized),&nn,72)&&append_varint(normalized,sizeof(normalized),&nn,1));
 assert(append_varint(normalized,sizeof(normalized),&nn,80)&&append_varint(normalized,sizeof(normalized),&nn,27));
 assert(append_varint(normalized,sizeof(normalized),&nn,40)&&append_varint(normalized,sizeof(normalized),&nn,5));
 fake_time+=8000000;setup_poll(fake_time);first_reply(1,normalized,nn);assert(setup_radio.state==MESH_SETUP_SAVED&&setup_radio.region==3);
 first_radio_reset();first_read();assert(!mesh_radio_setup_names("   ","A",setup_radio.revision)&&!mesh_radio_setup_names("A","  ",setup_radio.revision));
 first_radio_reset();short_write=true;assert(!mesh_radio_setup_read()&&setup_radio.state==MESH_SETUP_FAILED);short_write=false;
 assert(!message_count&&!message_alerts);
 puts("PASS: setup firmware/source/freshness guards, region/name preservation, fresh token and compare, one-shot writes, correlated readback/confirm, ACK distinction, restart/read retry, stale/malformed/foreign replies, failures and ownership changes");return 0;
}
