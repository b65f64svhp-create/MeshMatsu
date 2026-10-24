#define CONTACT_TRUST_PATH "contact-trust.bin"
#define main original_tests
#include "test_direct_messages.c"
#undef main
static protobuf_field_t get(const uint8_t *bytes,size_t n,uint32_t tag){protobuf_field_t f={0};assert(manage_field(bytes,n,tag,&f));return f;}
static const uint32_t peer=0xaabbccdd;
static void setup(void){reset();security_state=(mesh_security_t){0};memset(&security_op,0,sizeof(security_op));remote=(mesh_remote_t){0};memset(&remote_op,0,sizeof(remote_op));
 security_pending_size=0;trust_recovered=trust_future=false;contact_trust_count=0;contact_trust_ok=true;contact_trust_dirty=false;history_fs_ready=true;
 remove(CONTACT_TRUST_PATH);remove(CONTACT_TRUST_PATH ".bak");
 nodes[0]=(mesh_node_t){.number=peer,.public_key_available=true,.user_info_available=true};memset(nodes[0].public_key,3,32);strcpy(nodes[0].name,"Remote");strcpy(nodes[0].short_name,"TEST");node_count=1;
 session_passkey_size=4;memcpy(session_passkey,"LOCL",4);session_passkey_received_us=fake_time;
}
static size_t owner_bytes(uint8_t *bytes,const char *name){size_t n=0;uint8_t key[32];memset(key,3,32);
 assert(append_bytes_field(bytes,160,&n,2,(const uint8_t*)name,strlen(name)));assert(append_bytes_field(bytes,160,&n,3,(const uint8_t*)"TEST",4));assert(append_bytes_field(bytes,160,&n,8,key,32));
 assert(append_varint(bytes,160,&n,80)&&append_varint(bytes,160,&n,6));return n;}
static void response(uint8_t stage,uint32_t id,bool pki,uint32_t sender,uint32_t dest,const char *name,uint32_t value){
 uint8_t inner[180],body[200],admin[224],data[256],packet[360];size_t n=0,b=0,a=0,d=0,p=0;
 if(stage==0){n=owner_bytes(inner,name);assert(append_bytes_field(admin,sizeof(admin),&a,4,inner,n));}
 else if(stage==1){assert(append_bytes_field(inner,sizeof(inner),&n,1,(const uint8_t*)"2.7.18",6));assert(append_bytes_field(admin,sizeof(admin),&a,13,inner,n));}
 else{assert(append_varint(inner,sizeof(inner),&n,(stage==2?1:8)<<3)&&append_varint(inner,sizeof(inner),&n,value));assert(append_varint(inner,sizeof(inner),&n,800)&&append_varint(inner,sizeof(inner),&n,123));
 assert(append_bytes_field(body,sizeof(body),&b,stage==2?1:6,inner,n));assert(append_bytes_field(admin,sizeof(admin),&a,6,body,b));}
 assert(append_bytes_field(admin,sizeof(admin),&a,101,(const uint8_t*)"REMOTE!!",8));
 assert(append_varint(data,sizeof(data),&d,8)&&append_varint(data,sizeof(data),&d,6));assert(append_bytes_field(data,sizeof(data),&d,2,admin,a));data[d++]=0x35;memcpy(data+d,&id,4);d+=4;
 packet[p++]=0x0d;memcpy(packet+p,&sender,4);p+=4;packet[p++]=0x15;memcpy(packet+p,&dest,4);p+=4;assert(append_bytes_field(packet,sizeof(packet),&p,4,data,d));
 uint8_t key[32];memset(key,3,32);assert(append_bytes_field(packet,sizeof(packet),&p,16,key,32));assert(append_varint(packet,sizeof(packet),&p,17<<3)&&append_varint(packet,sizeof(packet),&p,pki));decode_mesh_packet(packet,p);
}
static void read_remote(void){assert(mesh_radio_verify_contact(peer,nodes[0].public_key));assert(mesh_radio_remote_read(peer));
 for(unsigned i=0;i<4;i++){assert(remote.stage==i&&remote.state==MESH_MANAGE_READING);response(i,remote_op.id,true,peer,local_node_number,"Remote",i==2?0:3);}
 assert(remote.state==MESH_MANAGE_READY&&remote.hops==3&&remote.role==0&&!strcmp(remote.name,"Remote"));}

#ifndef MANAGEMENT_TEST_MAIN
#define MANAGEMENT_TEST_MAIN main
#endif
int MANAGEMENT_TEST_MAIN(void){
 setup();mesh_contact_t c;assert(mesh_radio_contact(peer,&c));char url[256];assert(mesh_radio_contact_url(&c,url,sizeof(url)));mesh_contact_t parsed;assert(mesh_radio_parse_contact_url(url,&parsed)&&!memcmp(&c,&parsed,sizeof(c)));
 assert(!mesh_radio_parse_contact_url("https://evil.org/v/#AA",&parsed));assert(!mesh_radio_parse_contact_url("https://meshtastic.org/e/#AA",&parsed));
 char invalid[256];strcpy(invalid,url);strcat(invalid,"A");assert(!mesh_radio_parse_contact_url(invalid,&parsed));assert(!mesh_radio_contact_url(&c,url,24));
 assert(mesh_radio_contact_trust(peer,c.key,NULL)==MESH_TRUST_UNKNOWN);trust_poll(fake_time);assert(mesh_radio_contact_trust(peer,c.key,NULL)==MESH_TRUST_SEEN);
 assert(mesh_radio_verify_contact(peer,c.key));memset(contact_trust,0,sizeof(contact_trust));contact_trust_count=0;trust_load();assert(mesh_radio_contact_trust(peer,c.key,NULL)==MESH_TRUST_VERIFIED);
 nodes[0].public_key[0]++;trust_poll(fake_time);assert(mesh_radio_contact_trust(peer,nodes[0].public_key,NULL)==MESH_TRUST_CHANGED);size_t writes=write_count;assert(!mesh_radio_remote_read(peer)&&writes==write_count);nodes[0].public_key[0]--;
 assert(mesh_radio_remote_read(peer));uint32_t id=remote_op.id;writes=write_count;
 response(0,id,false,peer,local_node_number,"Spoof",0);response(0,id,true,peer+1,local_node_number,"Spoof",0);response(0,id+1,true,peer,local_node_number,"Spoof",0);response(0,id,true,peer,local_node_number+1,"Spoof",0);
 assert(remote.stage==0&&write_count==writes&&!remote.name[0]&&session_passkey_size==4&&!memcmp(session_passkey,"LOCL",4));
 response(0,id,true,peer,local_node_number,"Remote",0);assert(remote.stage==1);response(0,id,true,peer,local_node_number,"Old",0);assert(remote.stage==1);
 response(1,remote_op.id,true,peer,local_node_number,"",0);response(2,remote_op.id,true,peer,local_node_number,"",0);response(3,remote_op.id,true,peer,local_node_number,"",3);assert(remote.state==MESH_MANAGE_READY);
 assert(!owner_config_received||strcmp(local_long_name,"Remote"));assert(!device_config.loaded&&!lora_config.loaded);
 writes=write_count;assert(!mesh_radio_remote_change(MESH_REMOTE_HOPS,NULL,NULL,8,remote.revision)&&write_count==writes);
 assert(!mesh_radio_remote_change(MESH_REMOTE_HOPS,NULL,NULL,2,remote.revision+1));assert(mesh_radio_remote_change(MESH_REMOTE_HOPS,NULL,NULL,2,remote.revision)&&write_count==writes+1);
 assert(remote.state==MESH_MANAGE_CHECKING);response(3,remote_op.id,true,peer,local_node_number,"",3);assert(remote.state==MESH_MANAGE_SAVING&&write_count==writes+2);
 protobuf_field_t packet=get(sent_bytes+4,sent_size-4,1);assert(get(packet.value,packet.length,2).length==4);assert(get(packet.value,packet.length,17).integer==1);assert(get(packet.value,packet.length,16).length==32);
 protobuf_field_t data=get(packet.value,packet.length,4),admin=get(data.value,data.length,2),config=get(admin.value,admin.length,34),raw=get(config.value,config.length,6);
 assert(get(raw.value,raw.length,8).integer==2&&get(raw.value,raw.length,100).integer==123);assert(get(admin.value,admin.length,101).length==8&&!memcmp(get(admin.value,admin.length,101).value,"REMOTE!!",8));
 ack(local_node_number,remote_op.id,0);ack(peer,remote_op.id,0);assert(remote.state==MESH_MANAGE_SAVING);assert(!mesh_radio_remote_change(MESH_REMOTE_HOPS,NULL,NULL,2,remote.revision));
 fake_time+=15000001;management_poll(fake_time);assert(remote.state==MESH_MANAGE_VERIFYING&&write_count==writes+3);
 response(3,remote_op.id,true,peer,local_node_number,"",2);assert(remote.state==MESH_MANAGE_SAVED&&remote.hops==2);
 setup();read_remote();writes=write_count;assert(mesh_radio_remote_change(MESH_REMOTE_OWNER,"New name","TEST",0,remote.revision));response(0,remote_op.id,true,peer,local_node_number,"Remote",0);
 packet=get(sent_bytes+4,sent_size-4,1);data=get(packet.value,packet.length,4);admin=get(data.value,data.length,2);raw=get(admin.value,admin.length,32);assert(get(raw.value,raw.length,10).integer==6);
 fake_time+=15000001;management_poll(fake_time);response(0,remote_op.id,true,peer,local_node_number,"Old name",0);assert(remote.state==MESH_MANAGE_FAILED&&write_count==writes+3);
 setup();read_remote();assert(mesh_radio_remote_change(MESH_REMOTE_REBOOT,NULL,NULL,0,remote.revision));response(0,remote_op.id,true,peer,local_node_number,"Remote",0);ack(local_node_number,remote_op.id,0);assert(remote.state==MESH_MANAGE_SAVING);ack(peer,remote_op.id,0);assert(remote.state==MESH_MANAGE_REBOOT_ACK);
 setup();read_remote();assert(mesh_radio_remote_change(MESH_REMOTE_ROLE,NULL,NULL,1,remote.revision));response(2,remote_op.id,true,peer,local_node_number,"",0);ack(peer,remote_op.id,33);assert(remote.state==MESH_MANAGE_FAILED&&remote.error==33);
 setup();read_remote();nodes[0].public_key[0]++;management_poll(fake_time); // Ready snapshot cannot be used with a changed key.
 writes=write_count;assert(!mesh_radio_remote_change(MESH_REMOTE_REBOOT,NULL,NULL,0,remote.revision)&&write_count==writes);
 setup();read_remote();fake_time+=120000001;assert(!mesh_radio_remote_change(MESH_REMOTE_REBOOT,NULL,NULL,0,remote.revision));
 setup();assert(mesh_radio_verify_contact(peer,nodes[0].public_key));assert(mesh_radio_remote_read(peer));fake_time+=120000001;management_poll(fake_time);assert(remote.state==MESH_MANAGE_TIMEOUT);
 setup();assert(mesh_radio_verify_contact(peer,nodes[0].public_key));assert(mesh_radio_remote_read(peer));status.reconnecting=true;management_poll(fake_time);assert(remote.state==MESH_MANAGE_INTERRUPTED&&!remote_op.session_size);
 setup();read_remote();writes=write_count;assert(mesh_radio_remote_change(MESH_REMOTE_HOPS,NULL,NULL,2,remote.revision));response(3,remote_op.id,true,peer,local_node_number,"",4);assert(remote.state==MESH_MANAGE_FAILED&&write_count==writes+1); // changed during review: no mutation
 puts("PASS: contact URL/UTF-8/public-only codec, durable verified-key baselines and mismatch guards, isolated authenticated/correlated remote replies, PKI-only destination, session scoping, unknown-field preservation, single writes, fresh readback, reboot ACK distinction, NAK/timeouts/reconnect/stale key and revision guards");
 return 0;
}
