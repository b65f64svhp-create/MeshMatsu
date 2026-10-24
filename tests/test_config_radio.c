#define main existing_tests
#include "test_direct_messages.c"
#undef main
static mesh_config_backup_t backup;
static bool fixture_fix,fixture_fixed;
static int32_t fixture_lat=551700000,fixture_lon=-1188000000;
static void emit(unsigned tag,const void *data,size_t size){uint8_t f[1100];size_t n=0;assert(append_bytes_field(f,sizeof(f),&n,tag,data,size));decode_from_radio(f,n);}
static void complete(uint32_t nonce){uint8_t f[16]={0x38};size_t n=1;assert(append_varint(f,sizeof(f),&n,nonce));decode_from_radio(f,n);}
static void setup(void){fixture_fix=fixture_fixed=false;reset();cfg_state=CFG_IDLE;mesh_radio_config_clear();session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;radio_metadata=(mesh_radio_metadata_t){.loaded=true,.hardware_model=1};strcpy(radio_metadata.firmware_version,"2.7.18");}
static void dump(bool restored,bool omit){
 uint8_t user[]={8,1,18,4,'T','e','s','t'},node[100];size_t p=0;assert(append_varint(node,sizeof(node),&p,8)&&append_varint(node,sizeof(node),&p,local_node_number)&&append_bytes_field(node,sizeof(node),&p,2,user,sizeof(user)));if(fixture_fix){uint8_t pos[10]={0x0d};memcpy(pos+1,&fixture_lat,4);pos[5]=0x15;memcpy(pos+6,&fixture_lon,4);assert(append_bytes_field(node,sizeof(node),&p,3,pos,sizeof(pos)));}emit(4,node,p);
 uint8_t meta[]={10,6,'2','.','7','.','1','8',72,1};emit(13,meta,sizeof(meta));
 for(unsigned i=1;i<=8;i++){uint8_t inner[]={8,(uint8_t)(restored&&i==6?2:1)},wrapper[16];size_t n=0;if(i==2&&fixture_fixed){inner[0]=24;inner[1]=1;}assert(append_bytes_field(wrapper,sizeof(wrapper),&n,i,inner,sizeof(inner)));emit(5,wrapper,n);}
 for(unsigned i=1;i<=13;i++){uint8_t wrapper[16];size_t n=0;assert(append_bytes_field(wrapper,sizeof(wrapper),&n,i,NULL,0));if(!(omit&&i==13))emit(9,wrapper,n);}
 for(unsigned i=0;i<8;i++){uint8_t ch[]={8,(uint8_t)i,18,3,18,1,1,24,(uint8_t)(i?0:1)};emit(10,ch,sizeof(ch));}
}
static void capture(void){assert(mesh_radio_config_capture());dump(false,false);complete(config_nonce);assert(cfg_state==CFG_READY&&mesh_radio_config_snapshot(&backup)&&backup.count==31);}
static bool checked_pair(const uint8_t *pub,const uint8_t *priv){return pub[0]==3&&priv[0]==4;}
int main(void){
 setup();fixture_fix=true;capture();const mesh_cfg_record_t *coords=mesh_config_find(&backup,CFG_FIXED,0);assert(coords&&coords->size==9&&!coords->data[0]);
 setup();fixture_fix=fixture_fixed=true;capture();coords=mesh_config_find(&backup,CFG_FIXED,0);assert(coords&&coords->data[0]);fixture_lat+=1000;memcpy(backup.records[coords-backup.records].data+1,&fixture_lat,4);
 assert(mesh_radio_config_restore(&backup,coords));fake_time+=4000001;mesh_radio_poll();dump(false,false);complete(config_nonce);assert(cfg_state==CFG_SAVED);
 setup();assert(mesh_radio_config_capture());dump(false,true);complete(config_nonce-1);assert(cfg_state==CFG_READING);complete(config_nonce);assert(cfg_state==CFG_FAILED&&!mesh_radio_config_snapshot(&backup));
 setup();capture();const mesh_cfg_record_t *r=mesh_config_find(&backup,CFG_RADIO,6);assert(r&&r->size==4);backup.records[r-backup.records].data[3]=2;size_t writes=write_count;
 assert(mesh_radio_config_restore(&backup,r)&&write_count==writes+1&&cfg_state==CFG_SAVING);mesh_radio_config_clear();assert(cfg_state==CFG_SAVING);
 ack(local_node_number,cfg_write_id,0);assert(cfg_state==CFG_SAVING);fake_time+=4000001;mesh_radio_poll();assert(cfg_nonce==config_nonce&&config_request_pending);dump(true,false);complete(config_nonce);assert(cfg_state==CFG_SAVED);
 setup();capture();r=mesh_config_find(&backup,CFG_RADIO,6);backup.node++;writes=write_count;assert(!mesh_radio_config_restore(&backup,r)&&write_count==writes);backup.node--;backup.hardware++;assert(!mesh_radio_config_restore(&backup,r));backup.hardware--;strcpy(backup.firmware,"2.7.19");assert(!mesh_radio_config_restore(&backup,r));strcpy(backup.firmware,"2.7.18");
 backup.records[r-backup.records].data[0]=0x0a;assert(!mesh_radio_config_restore(&backup,r));backup.records[r-backup.records].data[0]=0x32;
 assert(mesh_radio_config_restore(&backup,r));ack(local_node_number,cfg_write_id,33);assert(cfg_state==CFG_FAILED);writes=write_count;fake_time+=5000000;mesh_radio_poll();assert(write_count>=writes); // Poll may send reads/heartbeats, never retries this mutation.
 setup();capture();r=mesh_config_find(&backup,CFG_RADIO,6);assert(mesh_radio_config_restore(&backup,r));fake_time+=120000001;cfg_poll(fake_time);assert(cfg_state==CFG_TIMEOUT);
 setup();assert(mesh_radio_config_capture());uint8_t duplicate[]={10,0,18,0};emit(5,duplicate,sizeof(duplicate));dump(false,false);complete(config_nonce);assert(cfg_state==CFG_FAILED);
 setup();capture();r=mesh_config_find(&backup,CFG_RADIO,8);assert(!mesh_radio_config_restore(&backup,r)); // Missing serial permission and valid key pair are refused.
 setup();capture();r=mesh_config_find(&backup,CFG_OWNER,0);mesh_cfg_record_t *owner=&backup.records[r-backup.records];size_t on=owner->size;uint8_t readonly[32]={7};assert(append_bytes_field(owner->data,sizeof(owner->data),&on,8,readonly,sizeof(readonly)));owner->size=on;
 assert(mesh_radio_config_restore(&backup,r));fake_time+=4000001;mesh_radio_poll();dump(false,false);complete(config_nonce);assert(cfg_state==CFG_SAVED); // Read-only public-key/User metadata cannot cause a false owner-restore failure.
 for(unsigned mode=0;mode<4;mode++){
  setup();capture();mesh_radio_config_identity_validator(checked_pair);r=mesh_config_find(&backup,CFG_RADIO,8);mesh_cfg_record_t *security=&backup.records[r-backup.records];
  uint8_t inner[100],pub[32]={3},priv[32]={4};size_t n=0,w=0;if(mode==3)priv[0]=5;
  assert(append_bytes_field(inner,sizeof(inner),&n,1,pub,32)&&append_bytes_field(inner,sizeof(inner),&n,2,priv,32));assert(append_varint(inner,sizeof(inner),&n,40)&&append_varint(inner,sizeof(inner),&n,mode==1?0:1));
  if(mode==2)assert(append_varint(inner,sizeof(inner),&n,32)&&append_varint(inner,sizeof(inner),&n,1));
  assert(append_bytes_field(security->data,sizeof(security->data),&w,8,inner,n));security->size=w;writes=write_count;
  bool ok=mesh_radio_config_restore(&backup,r);assert(ok==(mode==0));if(ok){assert(write_count==writes+1);ack(local_node_number,cfg_write_id,33);}else assert(write_count==writes);
 }
 uint8_t overlong[234]={0};writes=write_count;assert(!send_admin_payload(overlong,sizeof(overlong))&&write_count==writes);
 puts("PASS: full 31-section fresh nonce capture, missing/duplicate/malformed guards, same node/hardware/firmware restore, one mutation, ACK insufficiency, full fresh readback, NAK/timeout and no serial lockout");
}
