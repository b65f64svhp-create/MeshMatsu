#define main earlier_tests
#include "test_direct_messages.c"
#undef main
static uint8_t security[224];static size_t security_size;
static void ready(void){
 reset();keys_state=MESH_KEYS_IDLE;mesh_radio_keys_clear();keys_nonce=0;keys_revision=0;keys_write_id=0;
 session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;
 for(uint8_t i=0;i<8;i++){uint8_t ch[]={8,i,18,7,18,1,1,26,2,'B','L',24,i==0?1:i==1?2:0};decode_channel(ch,sizeof(ch));}
 uint8_t pub[32],priv[32],admin_key[32];memset(pub,3,32);memset(priv,4,32);memset(admin_key,9,32);security_size=0;
 assert(append_bytes_field(security,sizeof(security),&security_size,1,pub,32));assert(append_bytes_field(security,sizeof(security),&security_size,2,priv,32));
 assert(append_bytes_field(security,sizeof(security),&security_size,3,admin_key,32));
 assert(append_varint(security,sizeof(security),&security_size,40)&&append_varint(security,sizeof(security),&security_size,1)); // serial enabled
 assert(append_varint(security,sizeof(security),&security_size,800)&&append_varint(security,sizeof(security),&security_size,7)); // future field
 strcpy(local_long_name,"BUL-TEST");
}
static void dump(uint32_t nonce,bool complete,bool sec,bool all){
 uint8_t config[240],frame[300];size_t c=0,n=0;
 if(sec){assert(append_bytes_field(config,sizeof(config),&c,8,security,security_size));assert(append_bytes_field(frame,sizeof(frame),&n,5,config,c));decode_from_radio(frame,n);}
 for(uint8_t i=0;i<(all?8:7);i++){uint8_t ch[]={8,i,18,7,18,1,1,26,2,'B','L',24,i==0?1:i==1?2:0};n=0;assert(append_bytes_field(frame,sizeof(frame),&n,10,ch,sizeof(ch)));decode_from_radio(frame,n);}
 if(complete){n=0;assert(append_varint(frame,sizeof(frame),&n,56)&&append_varint(frame,sizeof(frame),&n,nonce));decode_from_radio(frame,n);}
}
static protobuf_field_t field(const uint8_t *data,size_t size,uint32_t tag){size_t p=0;protobuf_field_t f;while(p<size){assert(read_field(data,size,&p,&f));if(f.field==tag)return f;}assert(false);return (protobuf_field_t){0};}
int main(void){
 ready();size_t writes=write_count;assert(mesh_radio_keys_request()&&write_count==writes+1&&keys_state==MESH_KEYS_READING);uint32_t nonce=keys_nonce;
 mesh_key_backup_t b;assert(!mesh_radio_keys_snapshot(&b,true,true));dump(nonce+1,true,true,true);assert(keys_state==MESH_KEYS_READING);
 dump(nonce,true,true,true);assert(mesh_radio_keys_snapshot(&b,true,true)&&b.identity&&b.channel_mask==3&&b.private_key[0]==4&&b.channel[1].length==1&&b.channel[1].key[0]==1);
 uint32_t revision=keys_revision;uint8_t pub[32],priv[32];memset(pub,5,32);memset(priv,6,32);
 assert(!mesh_radio_keys_restore_identity(pub,priv,revision+1));writes=write_count;assert(mesh_radio_keys_restore_identity(pub,priv,revision)&&write_count==writes+1&&keys_state==MESH_KEYS_SAVING);
 protobuf_field_t packet=field(sent_bytes+4,sent_size-4,1),data=field(packet.value,packet.length,4),admin=field(data.value,data.length,2),cfg=field(admin.value,admin.length,34),raw=field(cfg.value,cfg.length,8);
 assert(field(data.value,data.length,1).integer==6&&field(raw.value,raw.length,1).value[0]==5&&field(raw.value,raw.length,2).value[0]==6);
 assert(field(raw.value,raw.length,3).value[0]==9&&field(raw.value,raw.length,5).integer==1&&field(raw.value,raw.length,100).integer==7);
 assert(field(admin.value,admin.length,101).length==4);assert(!mesh_radio_admin_ready()&&!mesh_radio_set_owner("x","y"));
 writes=write_count;assert(!mesh_radio_keys_restore_identity(pub,priv,revision)&&write_count==writes);ack(local_node_number,keys_write_id,0);assert(keys_state==MESH_KEYS_SAVING);
 // Only fresh complete readback after the single mutation can confirm it.
 fake_time+=2000000;keys_poll(fake_time);assert(keys_nonce);assert(write_count==writes+1);nonce=keys_nonce;
 size_t p=0;while(p<security_size){protobuf_field_t f;assert(read_field(security,security_size,&p,&f));if(f.field==1)memset((void*)f.value,5,32);if(f.field==2)memset((void*)f.value,6,32);}
 dump(nonce,true,true,true);assert(keys_state==MESH_KEYS_SAVED&&!keys_loaded&&!keys_raw_size);for(unsigned i=0;i<32;i++)assert(!keys_private[i]&&!keys_desired_private[i]);
 ready();assert(mesh_radio_keys_request());dump(keys_nonce,true,true,false);assert(keys_state==MESH_KEYS_FAILED&&!keys_loaded);
 ready();assert(mesh_radio_keys_request());dump(keys_nonce,true,false,true);assert(keys_state==MESH_KEYS_FAILED);
 ready();assert(mesh_radio_keys_request());dump(keys_nonce,true,true,true);revision=keys_revision;assert(mesh_radio_keys_restore_identity(pub,priv,revision));begin_radio_reconnect();assert(keys_state==MESH_KEYS_SAVING&&!keys_nonce);status.reconnecting=false;status.synchronized=true;fake_time+=2000000;writes=write_count;keys_poll(fake_time);assert(write_count==writes+1&&keys_nonce);dump(keys_nonce,true,true,true);assert(keys_state==MESH_KEYS_FAILED); // Old key is not success.
 ready();assert(mesh_radio_keys_request());dump(keys_nonce,true,true,true);assert(mesh_radio_keys_restore_identity(pub,priv,keys_revision));ack(local_node_number,keys_write_id,33);assert(keys_state==MESH_KEYS_FAILED);
 ready();assert(mesh_radio_keys_request());fake_time+=60000001;keys_poll(fake_time);assert(keys_state==MESH_KEYS_TIMEOUT&&!keys_loaded);
 ready();assert(mesh_radio_keys_request());dump(keys_nonce,true,true,true);assert(mesh_radio_keys_restore_identity(pub,priv,keys_revision));local_node_number++;keys_poll(fake_time);assert(keys_state==MESH_KEYS_INTERRUPTED);
 ready();assert(mesh_radio_keys_request());dump(keys_nonce,true,true,true);fake_time+=240000001;assert(!mesh_radio_keys_restore_identity(pub,priv,keys_revision));
 ready();assert(mesh_radio_keys_request());dump(keys_nonce,true,true,true);short_write=true;assert(!mesh_radio_keys_restore_identity(pub,priv,keys_revision));short_write=false;
 ready();assert(mesh_radio_keys_request());dump(keys_nonce,true,true,true);mesh_radio_keys_clear();assert(!keys_loaded&&keys_state==MESH_KEYS_IDLE);
 mesh_backup_channel_t saved={.name="other name",.role=1,.length=1,.key={0}};assert(mesh_radio_keys_restore_channel(1,&saved,channels[1].info.revision));assert(channel_operation_data.roles[1]==2);
 assert(!mesh_radio_keys_restore_channel(2,&saved,channels[2].info.revision));
 puts("PASS: local nonce-matched complete snapshots, identity/channel encoding, admin/unknown preservation, no optimistic ACK success/write replay, expiry/session/busy/reconnect/NAK/timeout guards and private-key clearing");
}
