#define NB_REPORT_TEST_MAIN prior_neighbor_tests
#include "test_neighbor_reports.c"
static void nb_config_setup(void){nb_setup();memcpy(session_passkey,"OLD",3);session_passkey_size=3;session_passkey_received_us=fake_time;radio_metadata.loaded=true;strcpy(radio_metadata.firmware_version,"2.7.18.abc");channels[0].info=(mesh_channel_t){.loaded=true,.role=1,.has_key=true};}
static size_t nb_config_bytes(uint8_t *raw,bool enabled,uint32_t interval,bool lora,unsigned extra){size_t n=0;assert(append_varint(raw,180,&n,8)&&append_varint(raw,180,&n,enabled)&&append_varint(raw,180,&n,16)&&append_varint(raw,180,&n,interval)&&append_varint(raw,180,&n,24)&&append_varint(raw,180,&n,lora));if(extra){assert(append_varint(raw,180,&n,800)&&append_varint(raw,180,&n,extra));}return n;}
static void nb_config_reply(uint32_t id,const uint8_t *raw,size_t n,bool token,uint32_t sender,uint32_t to){uint8_t module[200],admin[235],data[260];size_t m=0,a=0,d=0;assert(append_bytes_field(module,sizeof(module),&m,10,raw,n)&&append_bytes_field(admin,sizeof(admin),&a,8,module,m));if(token)assert(append_bytes_field(admin,sizeof(admin),&a,101,(const uint8_t*)"FRESH",5));assert(append_varint(data,sizeof(data),&d,8)&&append_varint(data,sizeof(data),&d,6)&&append_bytes_field(data,sizeof(data),&d,2,admin,a));data[d++]=0x35;memcpy(data+d,&id,4);d+=4;decode_data_message(data,d,sender,1800000000,to,555,0);}
static void nb_primary_reply(bool private_key){
 uint8_t settings[80],channel[110],admin[140];uint8_t key[16];memset(key,7,sizeof(key));size_t s=0,c=0,a=0;
 assert(append_bytes_field(settings,sizeof(settings),&s,2,private_key?key:(const uint8_t*)"\1",private_key?16:1)&&append_bytes_field(channel,sizeof(channel),&c,2,settings,s)&&append_varint(channel,sizeof(channel),&c,24)&&append_varint(channel,sizeof(channel),&c,1)&&append_bytes_field(admin,sizeof(admin),&a,2,channel,c));
 parse_admin_response(admin,a,nb_op.id);
}
static void nb_read(const uint8_t *raw,size_t n){assert(mesh_radio_neighbor_config_read());nb_config_reply(nb_op.id,raw,n,true,local_node_number,local_node_number);assert(nb_config.loaded&&nb_config.state==MESH_NBC_READY);}
static protobuf_field_t nb_sent(uint32_t tag){protobuf_field_t packet,data,admin,out;assert(manage_field(sent_bytes+4,sent_size-4,1,&packet)&&manage_field(packet.value,packet.length,4,&data)&&manage_field(data.value,data.length,2,&admin)&&manage_field(admin.value,admin.length,tag,&out));return out;}

#ifndef NB_CONFIG_TEST_MAIN
#define NB_CONFIG_TEST_MAIN main
#endif
int NB_CONFIG_TEST_MAIN(void){
 nb_config_setup();uint8_t raw[180],wanted[180];size_t n=nb_config_bytes(raw,false,0,false,77);nb_read(raw,n);size_t writes=write_count;
 mesh_neighbor_config_t desired=nb_config;desired.enabled=true;desired.interval=14400;desired.over_lora=true;
 desired.revision++;assert(!mesh_radio_neighbor_config_save(&desired)&&write_count==writes);desired.revision--;
 assert(mesh_radio_neighbor_config_save(&desired)&&nb_config.state==MESH_NBC_CHECKING&&write_count==writes+1&&!mesh_radio_admin_ready());assert(nb_sent(1).integer==1);nb_primary_reply(true);assert(nb_sent(7).integer==9);writes++;
 assert(!mesh_radio_set_owner("Name","Nm")&&!mesh_radio_manage_channel(1,0,raw,n,0));
 nb_config_reply(nb_op.id+1,raw,n,true,local_node_number,local_node_number);assert(nb_config.state==MESH_NBC_CHECKING);
 nb_config_reply(nb_op.id,raw,n,true,other,local_node_number);assert(nb_config.state==MESH_NBC_CHECKING);
 nb_config_reply(nb_op.id,raw,n,true,local_node_number,other);assert(nb_config.state==MESH_NBC_CHECKING);
 nb_config_reply(nb_op.id,raw,n,true,local_node_number,local_node_number);assert(nb_config.state==MESH_NBC_SAVING&&write_count==writes+2);
 protobuf_field_t module=nb_sent(35),body,token=nb_sent(101);assert(manage_field(module.value,module.length,10,&body)&&token.length==5&&!memcmp(token.value,"FRESH",5));
 protobuf_field_t field;assert(manage_field(body.value,body.length,1,&field)&&field.integer==1);assert(manage_field(body.value,body.length,2,&field)&&field.integer==14400);assert(manage_field(body.value,body.length,3,&field)&&field.integer==1);assert(manage_field(body.value,body.length,100,&field)&&field.integer==77);
 ack(local_node_number,nb_op.write_id,8);assert(nb_config.state==MESH_NBC_SAVING);ack(local_node_number,nb_op.write_id,0);assert(nb_config.state==MESH_NBC_SAVING);fake_time+=8000000;neighbor_poll(fake_time);assert(nb_config.state==MESH_NBC_VERIFYING&&write_count==writes+3);
 uint32_t verify=nb_op.id;nb_config_reply(verify,raw,n,true,local_node_number,local_node_number);assert(nb_config.state==MESH_NBC_FAILED&&!mesh_radio_neighbor_config_busy()); // old values never confirm
 // A fresh, correct response confirms, including preserved unknown fields.
 nb_config_setup();nb_read(raw,n);desired=nb_config;desired.enabled=true;desired.interval=21600;assert(mesh_radio_neighbor_config_save(&desired));nb_config_reply(nb_op.id,raw,n,true,local_node_number,0);assert(nb_config.state==MESH_NBC_SAVING);
 size_t wn=nb_config_bytes(wanted,true,21600,false,77);fake_time+=8000000;neighbor_poll(fake_time);nb_config_reply(nb_op.id,wanted,wn,true,local_node_number,local_node_number);assert(nb_config.state==MESH_NBC_SAVED&&nb_config.enabled&&mesh_radio_admin_ready());
 // Re-read the primary channel before enabling periodic RF sharing.
 nb_config_setup();nb_read(raw,n);desired=nb_config;desired.enabled=desired.over_lora=true;assert(mesh_radio_neighbor_config_save(&desired));writes=write_count;nb_primary_reply(false);assert(nb_config.state==MESH_NBC_CHANGED&&write_count==writes);
 // Changed preflight or missing fresh session token cannot cause a mutation.
 nb_config_setup();nb_read(raw,n);desired=nb_config;desired.enabled=true;assert(mesh_radio_neighbor_config_save(&desired));writes=write_count;raw[n-1]=78;nb_config_reply(nb_op.id,raw,n,true,local_node_number,0);assert(nb_config.state==MESH_NBC_CHANGED&&write_count==writes);raw[n-1]=77;
 nb_config_setup();nb_read(raw,n);desired=nb_config;desired.enabled=true;assert(mesh_radio_neighbor_config_save(&desired));writes=write_count;nb_config_reply(nb_op.id,raw,n,false,local_node_number,0);assert(nb_config.state==MESH_NBC_FAILED&&write_count==writes);
 // Saving may restart the radio. Same-node recovery re-reads; no mutation retry.
 nb_config_setup();nb_read(raw,n);desired=nb_config;desired.enabled=true;assert(mesh_radio_neighbor_config_save(&desired));nb_config_reply(nb_op.id,raw,n,true,local_node_number,0);writes=write_count;uint32_t write_id=nb_op.write_id;begin_radio_reconnect();assert(nb_config.state==MESH_NBC_SAVING&&!nb_config.loaded);fake_time+=8000000;neighbor_poll(fake_time);assert(write_count==writes);
 status.synchronized=true;status.reconnecting=false;config_request_pending=false;neighbor_poll(fake_time);assert(nb_config.state==MESH_NBC_VERIFYING&&nb_op.write_id==write_id&&write_count==writes+1);wn=nb_config_bytes(wanted,true,21600,false,77);nb_config_reply(nb_op.id,wanted,wn,true,local_node_number,0);assert(nb_config.state==MESH_NBC_SAVED); // default zero may expand to 6h
 // Readback requests may repeat; the write is never repeated. ACK/NAK is scoped.
 nb_config_setup();nb_read(raw,n);desired=nb_config;desired.enabled=true;assert(mesh_radio_neighbor_config_save(&desired));nb_config_reply(nb_op.id,raw,n,true,local_node_number,0);writes=write_count;fake_time+=8000000;neighbor_poll(fake_time);uint32_t old_id=nb_op.id;fake_time+=5000000;neighbor_poll(fake_time);assert(write_count==writes+2&&nb_op.id!=old_id);nb_config_reply(old_id,wanted,wn,true,local_node_number,0);assert(nb_config.state==MESH_NBC_VERIFYING);ack(other,nb_op.id,33);assert(nb_config.state==MESH_NBC_VERIFYING);ack(local_node_number,nb_op.id,33);assert(nb_config.state==MESH_NBC_FAILED&&nb_config.error==33);
 nb_config_setup();nb_read(raw,n);desired=nb_config;desired.enabled=true;desired.interval=14399;assert(!mesh_radio_neighbor_config_save(&desired));desired.interval=0;desired.over_lora=true;channels[0].info.default_key=true;assert(!mesh_radio_neighbor_lora_allowed()&&!mesh_radio_neighbor_config_save(&desired));channels[0].info.default_key=false;
 strcpy(radio_metadata.firmware_version,"2.8.1");assert(!mesh_radio_neighbor_config_save(&desired));strcpy(radio_metadata.firmware_version,"2.7.18");fake_time+=60000001;assert(!mesh_radio_neighbor_config_save(&desired));
 nb_config_setup();assert(mesh_radio_neighbor_config_read());fake_time+=90000001;neighbor_poll(fake_time);assert(nb_config.state==MESH_NBC_TIMEOUT);
 puts("PASS: module 10/admin 7=9, current/revision/firmware/privacy guards, fresh compare/token, one whole-section-preserving write, correlated readback, ACK distinctions, reboot recovery/read retries, stale review, failures and timeout");return 0;
}
