#define main message_tests
#include "test_direct_messages.c"
#undef main

static const uint32_t target=0xaabbccdd, other=0xabcdef01;
static void node_snapshot(uint32_t number, bool favorite, bool explicit_false) {
 uint8_t node[80],frame[100];size_t n=0,f=0;
 assert(append_varint(node,sizeof(node),&n,8)&&append_varint(node,sizeof(node),&n,number));
 if(favorite||explicit_false){assert(append_varint(node,sizeof(node),&n,80)&&append_varint(node,sizeof(node),&n,favorite));}
 assert(append_bytes_field(frame,sizeof(frame),&f,4,node,n));decode_from_radio(frame,f);
}
static void complete(uint32_t nonce){
 uint8_t frame[16]={0x38};size_t n=1;assert(append_varint(frame,sizeof(frame),&n,nonce));decode_from_radio(frame,n);
}
static protobuf_field_t field(const uint8_t *data,size_t size,uint32_t number){
 size_t p=0;protobuf_field_t f;while(p<size){assert(read_field(data,size,&p,&f));if(f.field==number)return f;}
 assert(!"Missing field");return (protobuf_field_t){0};
}
static void ready(void){
 reset();fake_time+=1000000;lora_config.loaded=true;
 session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;
 node_snapshot(target,false,false);node_snapshot(other,true,false);
}
static bool favored(uint32_t number){mesh_node_t node;assert(mesh_radio_get_node_by_number(number,&node)&&node.favorite_known);return node.is_favorite;}
static void begin_verification(void){
 fake_time+=1500001;mesh_radio_poll();assert(favorite_verify_nonce&&favorite_verify_nonce==config_nonce&&config_request_pending);
 // Verification is a local ToRadio.want_config_id, not a mesh transmission.
 assert(sent_bytes[4]==0x18);assert(favorite_status.state==MESH_FAVORITE_SAVING);
}
static void check_command(bool favorite){
 size_t size=((size_t)sent_bytes[2]<<8)|sent_bytes[3];assert(size+4==sent_size);
 protobuf_field_t packet=field(sent_bytes+4,size,1),to=field(packet.value,packet.length,2);
 uint32_t actual;memcpy(&actual,to.value,4);assert(actual==local_node_number&&actual!=target);
 protobuf_field_t data=field(packet.value,packet.length,4);assert(field(data.value,data.length,1).integer==6);
 protobuf_field_t admin=field(data.value,data.length,2);assert(field(admin.value,admin.length,favorite?39:40).integer==target);
 protobuf_field_t key=field(admin.value,admin.length,101);assert(key.length==4&&!memcmp(key.value,"test",4));
 protobuf_field_t id=field(packet.value,packet.length,6);memcpy(&actual,id.value,4);assert(actual==favorite_request_id&&actual);
 assert(field(packet.value,packet.length,10).integer==1); // Reliable ACK requested.
}
int main(void){
 ready();assert(!favored(target)&&favored(other));
 mesh_node_t before;assert(mesh_radio_get_node_by_number(target,&before));
 incoming(target,local_node_number,500,3);assert(!favored(target)); // Mesh traffic cannot set local favorites.
 size_t writes=write_count;assert(mesh_radio_set_favorite(0,true)==MESH_FAVORITE_START_INVALID);
 assert(mesh_radio_set_favorite(local_node_number,true)==MESH_FAVORITE_START_INVALID);
 assert(mesh_radio_set_favorite(MESH_BROADCAST_NODE,true)==MESH_FAVORITE_START_INVALID);
 assert(mesh_radio_set_favorite(42,true)==MESH_FAVORITE_START_INVALID&&write_count==writes);
 assert(mesh_radio_set_favorite(target,true)==MESH_FAVORITE_START_OK);check_command(true);
 assert(!favored(target)&&favorite_status.state==MESH_FAVORITE_SAVING); // No optimistic success.
 writes=write_count;assert(mesh_radio_set_favorite(other,false)==MESH_FAVORITE_START_BUSY&&write_count==writes);
 ack(target,favorite_request_id,33);ack(local_node_number,favorite_request_id,0);
 assert(favorite_status.state==MESH_FAVORITE_SAVING); // Remote NAK/local ACK do not verify the DB.
 node_snapshot(target,true,false);assert(favorite_status.state==MESH_FAVORITE_SAVING); // Unsolicited update insufficient.
 begin_verification();complete(config_nonce-1);assert(favorite_status.state==MESH_FAVORITE_SAVING);
 node_snapshot(other,true,false);node_snapshot(target,true,false);
 assert(favorite_status.state==MESH_FAVORITE_SAVING);complete(config_nonce);
 assert(favorite_status.state==MESH_FAVORITE_SAVED&&favored(target)&&favored(other));
 assert(mesh_radio_set_favorite(target,false)==MESH_FAVORITE_START_OK);check_command(false);
 assert(favored(target));begin_verification();node_snapshot(target,false,false); // Proto3 false is omitted.
 complete(config_nonce);assert(favorite_status.state==MESH_FAVORITE_SAVED&&!favored(target));
 // Radio owns persistence: a fresh app cache restores favorites from NodeInfo.
 node_count=0;node_snapshot(target,true,false);assert(favored(target));node_snapshot(target,false,true);assert(!favored(target));
 // A matched NAK retains the confirmed flag and reports failure; never retry the mutation.
 assert(mesh_radio_set_favorite(target,true)==MESH_FAVORITE_START_OK);ack(local_node_number,favorite_request_id,36);
 assert(favorite_status.state==MESH_FAVORITE_FAILED&&favorite_status.error_reason==36&&!favored(target));
 ready();assert(mesh_radio_set_favorite(target,true)==MESH_FAVORITE_START_OK);begin_verification();node_snapshot(target,false,false);
 complete(config_nonce);assert(favorite_status.state==MESH_FAVORITE_FAILED&&!favored(target));
 ready();assert(mesh_radio_set_favorite(target,true)==MESH_FAVORITE_START_OK);begin_verification();complete(config_nonce);
 assert(favorite_status.state==MESH_FAVORITE_FAILED); // Missing target cannot confirm a successful no-op.
 ready();short_write=true;assert(mesh_radio_set_favorite(target,true)==MESH_FAVORITE_START_WRITE_FAILED);
 assert(favorite_status.state==MESH_FAVORITE_IDLE&&!favored(target));short_write=false;
 status.reconnecting=true;writes=write_count;assert(mesh_radio_set_favorite(target,true)==MESH_FAVORITE_START_OFFLINE&&write_count==writes);
 ready();fake_time+=240000001;writes=write_count;
 assert(mesh_radio_set_favorite(target,true)==MESH_FAVORITE_START_NOT_READY&&write_count==writes+1);
 assert(favorite_status.state==MESH_FAVORITE_IDLE); // Only an owner read is sent, never a deferred set.
 // Foreign admin replies cannot seed local access or overwrite owner/config.
 session_passkey_size=0;uint8_t admin[]={0xaa,6,4,'e','v','i','l'},data[40];size_t n=0;
 assert(append_varint(data,sizeof(data),&n,8)&&append_varint(data,sizeof(data),&n,6));
 assert(append_bytes_field(data,sizeof(data),&n,2,admin,sizeof(admin)));
 decode_data_message(data,n,target,0,local_node_number,700,0);assert(!session_passkey_size);
 ready();assert(mesh_radio_set_favorite(target,true)==MESH_FAVORITE_START_OK);begin_radio_reconnect();
 assert(favorite_status.state==MESH_FAVORITE_INTERRUPTED&&!favored(target));
 ready();assert(mesh_radio_set_favorite(target,true)==MESH_FAVORITE_START_OK);fake_time+=20000001;mesh_radio_poll();
 assert(favorite_status.state==MESH_FAVORITE_TIMEOUT&&!favored(target));
 assert(message_count==0&&reaction_count==0&&mesh_radio_unread_total(false)==0&&mesh_radio_unread_total(true)==0);
 puts("PASS: radio-owned favorite flag, exact local admin set/remove/key encoding, no optimistic save, nonce-matched complete readback, omitted false, stale/foreign replies, missing/mismatched target, NAK, expiry, write failure, reconnect and timeout");
}
