#define NODE_PREFS_PATH "node-prefs.bin"
#define main message_tests
#include "test_direct_messages.c"
#undef main
static const uint32_t target=0xaabbccdd, other=0x11223344;
static void snapshot(uint32_t number, bool ignored) {
    uint8_t node[64],frame[80];size_t n=0,f=0;
    assert(append_varint(node,sizeof(node),&n,8)&&append_varint(node,sizeof(node),&n,number));
    if(ignored){assert(append_varint(node,sizeof(node),&n,88)&&append_varint(node,sizeof(node),&n,1));}
    assert(append_bytes_field(frame,sizeof(frame),&f,4,node,n));decode_from_radio(frame,f);
}
static void done(uint32_t nonce){uint8_t frame[16]={0x38};size_t n=1;assert(append_varint(frame,sizeof(frame),&n,nonce));decode_from_radio(frame,n);}
static void ready(void){reset();fake_time+=1000000;lora_config.loaded=true;session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;snapshot(target,false);snapshot(other,false);}
static bool ignored(void){mesh_node_t node;assert(mesh_radio_get_node_by_number(target,&node));return node.is_ignored;}
static protobuf_field_t field(const uint8_t *data,size_t n,uint32_t tag){size_t p=0;protobuf_field_t f;while(p<n){assert(read_field(data,n,&p,&f));if(f.field==tag)return f;}assert(!"Missing field");return (protobuf_field_t){0};}
static void check_command(unsigned tag){size_t n=((size_t)sent_bytes[2]<<8)|sent_bytes[3];protobuf_field_t packet=field(sent_bytes+4,n,1),to=field(packet.value,packet.length,2);uint32_t number;memcpy(&number,to.value,4);assert(number==local_node_number);protobuf_field_t data=field(packet.value,packet.length,4),admin=field(data.value,data.length,2);assert(field(data.value,data.length,1).integer==6);assert(field(admin.value,admin.length,tag).integer==target);assert(field(admin.value,admin.length,101).length==4);}
static void verify(void){fake_time+=1500001;mesh_radio_poll();assert(node_control_nonce==config_nonce&&config_request_pending&&node_control.state==MESH_FAVORITE_SAVING);}
int main(void){
    ready();size_t writes=write_count;
    assert(mesh_radio_control_node(local_node_number,MESH_NODE_REMOVE)==MESH_FAVORITE_START_INVALID);
    assert(mesh_radio_control_node(0,MESH_NODE_IGNORE)==MESH_FAVORITE_START_INVALID&&write_count==writes);
    assert(mesh_radio_control_node(target,MESH_NODE_IGNORE)==MESH_FAVORITE_START_OK);check_command(47);assert(!ignored());
    writes=write_count;assert(mesh_radio_control_node(other,MESH_NODE_REMOVE)==MESH_FAVORITE_START_BUSY&&mesh_radio_set_favorite(other,true)==MESH_FAVORITE_START_BUSY&&write_count==writes);
    ack(target,node_control_id,33);ack(local_node_number,node_control_id,0);assert(node_control.state==MESH_FAVORITE_SAVING);
    snapshot(target,true);assert(node_control.state==MESH_FAVORITE_SAVING);verify();done(config_nonce-1);assert(node_control.state==MESH_FAVORITE_SAVING);
    snapshot(target,true);done(config_nonce);assert(node_control.state==MESH_FAVORITE_SAVED&&ignored());
    writes=write_count;incoming(target,local_node_number,10,3);assert(message_count==0&&message_alerts==0&&write_count==writes);
    assert(mesh_radio_control_node(target,MESH_NODE_UNIGNORE)==MESH_FAVORITE_START_OK);check_command(48);assert(ignored());verify();snapshot(target,false);done(config_nonce);assert(node_control.state==MESH_FAVORITE_SAVED&&!ignored());
    incoming(target,local_node_number,11,3);assert(message_count==1&&mesh_radio_unread_direct(target)==1);
    assert(mesh_trails_add(target,0,0,current_message_timestamp(),0,current_message_timestamp()));
    assert(mesh_radio_control_node(target,MESH_NODE_REMOVE)==MESH_FAVORITE_START_OK);check_command(38);assert(node_count==2);verify();snapshot(other,false);done(config_nonce);
    mesh_node_t node;assert(node_control.state==MESH_FAVORITE_SAVED&&node_count==1&&mesh_radio_status().node_count==1&&!mesh_radio_get_node_by_number(target,&node)&&message_count==1&&mesh_radio_unread_direct(target)==1);
    assert(!mesh_trails_count());
    snapshot(target,false);assert(node_count==2); // Removing is not blocking; it may reappear.
    ready();assert(mesh_radio_control_node(target,MESH_NODE_REMOVE)==MESH_FAVORITE_START_OK);verify();snapshot(target,false);done(config_nonce);assert(node_control.state==MESH_FAVORITE_FAILED&&node_count==2);
    ready();assert(mesh_radio_control_node(target,MESH_NODE_IGNORE)==MESH_FAVORITE_START_OK);verify();done(config_nonce);assert(node_control.state==MESH_FAVORITE_FAILED&&!ignored());
    ready();assert(mesh_radio_control_node(target,MESH_NODE_IGNORE)==MESH_FAVORITE_START_OK);ack(local_node_number,node_control_id,36);assert(node_control.state==MESH_FAVORITE_FAILED&&node_control.error_reason==36&&!ignored());
    ready();short_write=true;assert(mesh_radio_control_node(target,MESH_NODE_IGNORE)==MESH_FAVORITE_START_WRITE_FAILED);assert(node_control.state==MESH_FAVORITE_IDLE);short_write=false;
    ready();assert(mesh_radio_control_node(target,MESH_NODE_REMOVE)==MESH_FAVORITE_START_OK);fake_time+=20000001;mesh_radio_poll();assert(node_control.state==MESH_FAVORITE_TIMEOUT&&node_count==2);
    ready();assert(mesh_radio_control_node(target,MESH_NODE_IGNORE)==MESH_FAVORITE_START_OK);begin_radio_reconnect();assert(node_control.state==MESH_FAVORITE_INTERRUPTED&&!ignored());
    ready();fake_time+=240000001;assert(mesh_radio_control_node(target,MESH_NODE_IGNORE)==MESH_FAVORITE_START_NOT_READY&&node_control.state==MESH_FAVORITE_IDLE);
    // Notification mute is local, durable and does not hide messages or unread counts.
    ready();history_fs_ready=true;remove(NODE_PREFS_PATH);remove(NODE_PREFS_TEMP_PATH);remove(NODE_PREFS_PATH ".bak");
    writes=write_count;assert(mesh_radio_set_node_muted(target,true)&&mesh_radio_node_muted(target)&&write_count==writes&&!ignored());
    incoming(target,local_node_number,30,3);incoming(target,MESH_BROADCAST_NODE,31,3);
    assert(message_count==2&&mesh_radio_unread_direct(target)==1&&mesh_radio_unread_channel(0)==1&&message_alerts==0);
    assert(!mesh_radio_alert_unread(true,0)&&!mesh_radio_alert_unread(false,0));
    memset(&node_preferences,0,sizeof(node_preferences));load_node_preferences();assert(mesh_radio_node_muted(target));
    incoming(other,MESH_BROADCAST_NODE,32,3);assert(mesh_radio_alert_unread(false,0)&&message_alerts==1);
    assert(mesh_radio_set_node_muted(target,false)&&mesh_radio_alert_unread(true,0));
    assert(mkdir(NODE_PREFS_TEMP_PATH,0700)==0);assert(!mesh_radio_set_node_muted(target,true)&&!mesh_radio_node_muted(target));rmdir(NODE_PREFS_TEMP_PATH);
    assert(!mesh_radio_set_node_muted(local_node_number,true));
    // A complete backup recovers a power loss between the two FAT rename steps.
    assert(mesh_radio_set_node_muted(target,true));
    remove(NODE_PREFS_PATH ".bak");assert(rename(NODE_PREFS_PATH,NODE_PREFS_PATH ".bak")==0);
    memset(&node_preferences,0,sizeof(node_preferences));load_node_preferences();assert(mesh_radio_node_muted(target));
    remove(NODE_PREFS_PATH);remove(NODE_PREFS_PATH ".bak");
    puts("PASS: exact local admin ignore/unignore/remove with nonce readback, no optimistic success or retries, failure/reconnect handling, retained chats, durable per-node mute, unchanged unread counts and awake/asleep alert policy");
}
