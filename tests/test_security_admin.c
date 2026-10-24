#define MANAGEMENT_TEST_MAIN earlier_management
#include "test_management.c"
#undef main
static uint8_t local_sec[224];static size_t local_sec_size;
static void sec_fixture(unsigned admins){local_sec_size=0;uint8_t key[32];memset(key,5,32);assert(append_bytes_field(local_sec,sizeof(local_sec),&local_sec_size,1,key,32));memset(key,6,32);assert(append_bytes_field(local_sec,sizeof(local_sec),&local_sec_size,2,key,32));
 for(unsigned i=0;i<admins;i++){memset(key,7+i,32);assert(append_bytes_field(local_sec,sizeof(local_sec),&local_sec_size,3,key,32));}
 assert(append_varint(local_sec,sizeof(local_sec),&local_sec_size,40)&&append_varint(local_sec,sizeof(local_sec),&local_sec_size,1));assert(append_varint(local_sec,sizeof(local_sec),&local_sec_size,800)&&append_varint(local_sec,sizeof(local_sec),&local_sec_size,7));}
static void sec_response(uint32_t id){uint8_t cfg[240],adm[256];size_t c=0,a=0;assert(append_bytes_field(cfg,sizeof(cfg),&c,8,local_sec,local_sec_size));assert(append_bytes_field(adm,sizeof(adm),&a,6,cfg,c));assert(append_bytes_field(adm,sizeof(adm),&a,101,(const uint8_t*)"LOCL",4));parse_admin_response(adm,a,id);}

#ifndef SECURITY_TEST_MAIN
#define SECURITY_TEST_MAIN main
#endif
int SECURITY_TEST_MAIN(void){
 setup();sec_fixture(1);assert(mesh_radio_security_read());size_t writes=write_count;uint32_t id=security_op.id;sec_response(id+1);assert(security_state.state==MESH_MANAGE_READING);sec_response(id);assert(security_state.state==MESH_MANAGE_READY&&security_state.admin_count==1&&security_state.serial_enabled);
 uint8_t key[32];memset(key,9,32);assert(!mesh_radio_security_set_admin(1,key,false,security_state.revision+1)&&write_count==writes);
 assert(mesh_radio_security_set_admin(1,key,false,security_state.revision)&&write_count==writes+1);assert(!mesh_radio_admin_ready());assert(security_state.state==MESH_MANAGE_CHECKING);sec_response(security_op.id);assert(security_state.state==MESH_MANAGE_SAVING&&write_count==writes+2);
 protobuf_field_t packet=get(sent_bytes+4,sent_size-4,1),data=get(packet.value,packet.length,4),adm=get(data.value,data.length,2),cfg=get(adm.value,adm.length,34),raw=get(cfg.value,cfg.length,8);
 assert(get(raw.value,raw.length,1).value[0]==5&&get(raw.value,raw.length,2).value[0]==6&&get(raw.value,raw.length,5).integer==1&&get(raw.value,raw.length,100).integer==7);
 unsigned count=0;size_t p=0;while(p<raw.length){protobuf_field_t f;assert(read_field(raw.value,raw.length,&p,&f));if(f.field==3){assert(f.value[0]==(count?9:7));count++;}}assert(count==2);
 ack(local_node_number,security_op.id,0);assert(security_state.state==MESH_MANAGE_SAVING);fake_time+=2000001;management_poll(fake_time);assert(security_state.state==MESH_MANAGE_VERIFYING&&write_count==writes+3);
 assert(append_bytes_field(local_sec,sizeof(local_sec),&local_sec_size,3,key,32));sec_response(security_op.id);assert(security_state.state==MESH_MANAGE_SAVED&&security_state.admin_count==2&&mesh_radio_admin_ready());
 // Duplicate and zero keys, serial lockout, managed mode and stale snapshots are refused.
 assert(mesh_radio_security_read());sec_response(security_op.id);writes=write_count;assert(!mesh_radio_security_set_admin(2,key,false,security_state.revision)&&write_count==writes);memset(key,0,32);assert(!mesh_radio_security_set_admin(2,key,false,security_state.revision));
 security_state.managed=true;memset(key,10,32);assert(!mesh_radio_security_set_admin(2,key,false,security_state.revision));security_state.managed=false;security_state.serial_enabled=false;assert(!mesh_radio_security_set_admin(2,key,false,security_state.revision));security_state.serial_enabled=true;
 fake_time+=60000001;assert(!mesh_radio_security_set_admin(2,key,false,security_state.revision));management_poll(fake_time);assert(!security_size&&security_state.state==MESH_MANAGE_IDLE);
 setup();sec_fixture(1);assert(mesh_radio_security_read());sec_response(security_op.id);assert(mesh_radio_security_set_admin(0,key,true,security_state.revision));sec_response(security_op.id);fake_time+=2000001;management_poll(fake_time);sec_response(security_op.id);assert(security_state.state==MESH_MANAGE_FAILED); // old snapshot != removal
 setup();sec_fixture(1);assert(mesh_radio_security_read());sec_response(security_op.id);assert(mesh_radio_security_set_admin(0,key,true,security_state.revision));sec_response(security_op.id);ack(local_node_number,security_op.id,33);assert(security_state.state==MESH_MANAGE_FAILED&&security_state.error==33);
 setup();mesh_contact_t c;assert(mesh_radio_contact(peer,&c));writes=write_count;assert(mesh_radio_import_contact(&c)&&write_count==writes+1);packet=get(sent_bytes+4,sent_size-4,1);data=get(packet.value,packet.length,4);adm=get(data.value,data.length,2);raw=get(adm.value,adm.length,66);assert(get(raw.value,raw.length,1).integer==peer);assert(!manage_field(raw.value,raw.length,4,&cfg));
 fake_time+=2000001;management_poll(fake_time);assert(security_op.importing&&security_state.state==MESH_MANAGE_VERIFYING&&config_request_pending);
 management_complete(security_op.nonce+1);assert(security_state.state==MESH_MANAGE_VERIFYING);management_observe_contact(&nodes[0]);management_complete(security_op.nonce);assert(security_state.state==MESH_MANAGE_SAVED);
 setup();assert(mesh_radio_contact(peer,&c));assert(mesh_radio_import_contact(&c));fake_time+=2000001;management_poll(fake_time);management_complete(security_op.nonce);assert(security_state.state==MESH_MANAGE_FAILED); // cached NodeDB is insufficient
 setup();assert(mesh_radio_security_read());fake_time+=120000001;management_poll(fake_time);assert(security_state.state==MESH_MANAGE_TIMEOUT&&!security_size);
 setup();sec_fixture(1);assert(mesh_radio_security_read());sec_response(security_op.id);writes=write_count;assert(mesh_radio_security_set_admin(0,key,true,security_state.revision));sec_fixture(2);sec_response(security_op.id);assert(security_state.state==MESH_MANAGE_FAILED&&write_count==writes+1); // stale review never mutates
 puts("PASS: fresh local security_state reads and expiry, add/remove admin keys, identity/serial/unknown preservation, no ACK-only success, duplicate/zero/managed/lockout/stale guards, contact wire import without inherited verification and nonce-matched fresh NodeDB verification");
 return 0;
}
