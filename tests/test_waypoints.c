#define WAYPOINT_PATH "waypoints.bin"
#define main previous_tests
#include "test_direct_messages.c"
#undef main
static const uint32_t alice=0x11223344,bob=0xaabbccdd;
static void receive(uint32_t sender,uint32_t to,uint8_t channel,uint32_t packet,uint32_t timestamp,const mesh_waypoint_t *w){uint8_t payload[192],data[208];size_t n=0,p=0;assert(encode_waypoint(w,payload,sizeof(payload),&n));assert(append_varint(data,sizeof(data),&p,8)&&append_varint(data,sizeof(data),&p,8)&&append_bytes_field(data,sizeof(data),&p,2,payload,n));decode_data_message(data,p,sender,timestamp,to,packet,channel);}
static mesh_waypoint_t sample(uint32_t id){mesh_waypoint_t w={.id=id,.latitude_i=551700000,.longitude_i=-1188000000,.expire=INT32_MAX,.locked_to=alice,.icon=0x1f4cd};strcpy(w.name,"Rally point");strcpy(w.description,"Meet here");return w;}
static void clean(void){remove(WAYPOINT_PATH);remove(WAYPOINT_PATH ".tmp");remove(WAYPOINT_PATH ".bak");}
int main(void){
    reset();clean();mesh_waypoint_t w=sample(500),got;uint32_t now=current_message_timestamp();
    receive(alice,MESH_BROADCAST_NODE,1,100,now,&w);assert(mesh_radio_waypoint_count()==1&&mesh_radio_get_waypoint(0,&got)&&got.locked_to==alice&&got.channel==1&&got.creator==alice);
    assert(!message_count&&!reaction_count&&!message_alerts&&!mesh_radio_unread_total(false));
    strcpy(w.name,"Forged change");receive(bob,MESH_BROADCAST_NODE,1,101,now+1,&w);mesh_radio_get_waypoint(0,&got);assert(!strcmp(got.name,"Rally point"));
    w.expire=1;receive(bob,MESH_BROADCAST_NODE,1,102,now+2,&w);assert(mesh_radio_waypoint_count()==1);
    w.expire=INT32_MAX;strcpy(w.name,"Owner update");receive(alice,MESH_BROADCAST_NODE,1,103,now+3,&w);mesh_radio_get_waypoint(0,&got);assert(!strcmp(got.name,"Owner update")&&got.updated_by==alice);
    receive(alice,MESH_BROADCAST_NODE,1,104,now,&w);assert(waypoints[0].packet_id==103); // delayed update cannot replace a newer report
    w.expire=1;receive(alice,MESH_BROADCAST_NODE,1,105,now+4,&w);assert(!mesh_radio_waypoint_count()&&waypoint_count==1);
    w=sample(501);w.locked_to=0;w.latitude_i=0;w.longitude_i=0;receive(alice,MESH_BROADCAST_NODE,0,106,now,&w);assert(mesh_radio_waypoint_count()==1&&mesh_radio_get_waypoint(0,&got)&&got.latitude_i==0&&got.longitude_i==0);
    strcpy(w.name,"Open edited");receive(bob,MESH_BROADCAST_NODE,0,107,now+1,&w);assert(mesh_radio_get_waypoint(0,&got)&&!strcmp(got.name,"Open edited")&&got.creator==alice);
    receive(alice,bob,0,108,now,&w);assert(mesh_radio_waypoint_count()==1); // another recipient
    receive(alice,local_node_number,0,109,now,&w);assert(mesh_radio_waypoint_count()==2); // private scope independent of channel
    w.latitude_i=910000000;receive(alice,MESH_BROADCAST_NODE,0,110,now,&w);assert(mesh_radio_waypoint_count()==2);
    w=sample(502);w.expire=now-1;receive(alice,MESH_BROADCAST_NODE,0,111,now,&w);assert(mesh_radio_waypoint_count()==2);
    // Missing coordinate presence is invalid even if the implicit value is zero.
    const uint8_t invalid[]={8,9,0x32,1,'x'};decode_waypoint(alice,MESH_BROADCAST_NODE,112,now,0,invalid,sizeof(invalid));assert(mesh_radio_waypoint_count()==2);
    // Locally shared markers use fixed32 coordinates/icon and reliable port8 on the chosen channel.
    uint8_t channel[]={8,1,24,2};decode_channel(channel,sizeof(channel));w=sample(600);w.locked_to=local_node_number;w.channel=1;
    assert(mesh_radio_send_waypoint(&w,false));assert(mesh_radio_find_waypoint(600,0,1,&got)&&got.delivery==MESH_DELIVERY_PENDING&&got.creator==local_node_number);
    uint32_t packet=got.packet_id;size_t writes=write_count;assert(!mesh_radio_send_waypoint(&got,false)&&write_count==writes);
    ack(local_node_number,packet,0);assert(mesh_radio_find_waypoint(600,0,1,&got)&&got.delivery==MESH_DELIVERY_ACKNOWLEDGED);
    assert(mesh_radio_send_waypoint(&got,true));assert(mesh_radio_find_waypoint(600,0,1,&got)&&got.expire==1&&got.deleted);
    ack(local_node_number,got.packet_id,9);assert(mesh_radio_find_waypoint(600,0,1,&got)&&got.delivery==MESH_DELIVERY_FAILED&&got.error_reason==9);
    w=sample(601);w.locked_to=local_node_number;w.peer_node=alice;assert(mesh_radio_send_waypoint(&w,false));mesh_radio_find_waypoint(601,alice,0,&got);packet=got.packet_id;
    ack(local_node_number,packet,0);mesh_radio_find_waypoint(601,alice,0,&got);assert(got.delivery==MESH_DELIVERY_PENDING);ack(bob,packet,0);mesh_radio_find_waypoint(601,alice,0,&got);assert(got.delivery==MESH_DELIVERY_PENDING);ack(alice,packet,0);mesh_radio_find_waypoint(601,alice,0,&got);assert(got.delivery==MESH_DELIVERY_ACKNOWLEDGED);
    uint8_t primary[]={24,1};decode_channel(primary,sizeof(primary));w=sample(602);w.locked_to=local_node_number;assert(mesh_radio_send_waypoint(&w,false));fake_time+=90000001;mesh_radio_poll();mesh_radio_find_waypoint(602,0,0,&got);assert(got.failure==MESH_FAILURE_TIMEOUT);
    w=sample(603);w.locked_to=local_node_number;assert(mesh_radio_send_waypoint(&w,false));history_fs_ready=true;assert(mesh_radio_flush_waypoints());size_t before=waypoint_count;
    waypoint_count=0;restore_waypoints();assert(waypoint_count==before&&mesh_radio_find_waypoint(603,0,0,&got)&&got.failure==MESH_FAILURE_RESTARTED);assert(!message_count&&!message_alerts);
    assert(mesh_radio_flush_waypoints());FILE *f=fopen(WAYPOINT_PATH,"r+b");assert(f);fseek(f,20,SEEK_SET);fputc(255,f);fclose(f);waypoint_count=0;restore_waypoints();assert(waypoint_count==before); // backup
    reset();history_fs_ready=false;
    for(unsigned i=0;i<MESH_WAYPOINT_LIMIT+20;i++){w=sample(i+1000);receive(alice,MESH_BROADCAST_NODE,0,1000+i,now+i,&w);}assert(waypoint_count==MESH_WAYPOINT_LIMIT&&mesh_radio_waypoint_count()==MESH_WAYPOINT_LIMIT);
    // Prefer an expired slot over active markers, while protecting in-flight sends.
    waypoints[20].deleted=true;w=sample(3000);receive(alice,MESH_BROADCAST_NODE,0,9000,now+1000,&w);assert(waypoints[20].id==3000);
    for(size_t i=0;i<waypoint_count;i++)waypoints[i].delivery=MESH_DELIVERY_PENDING;
    w=sample(3001);w.locked_to=local_node_number;size_t pending_writes=write_count;assert(!mesh_radio_send_waypoint(&w,false)&&write_count==pending_writes);
    receive(alice,MESH_BROADCAST_NODE,0,9001,now+1001,&w);assert(waypoint_count==MESH_WAYPOINT_LIMIT);
    // Future waypoint extensions survive edit/re-share, and cannot override lock fields.
    reset();w=sample(4000);w.locked_to=0;w.extra[0]=0x48;w.extra[1]=123;w.extra_size=2;
    receive(alice,MESH_BROADCAST_NODE,0,9002,now,&w);assert(mesh_radio_get_waypoint(0,&got)&&got.extra_size==2&&!memcmp(got.extra,w.extra,2));
    uint8_t encoded[233];size_t encoded_size=0;assert(encode_waypoint(&got,encoded,sizeof(encoded),&encoded_size)&&encoded[encoded_size-2]==0x48&&encoded[encoded_size-1]==123);
    got.extra[0]=0x28;assert(!encode_waypoint(&got,encoded,sizeof(encoded),&encoded_size));
    // Invalid and arbitrary packet bytes never cause out-of-bounds reads.
    unsigned seed=77;for(unsigned i=0;i<1500;i++){uint8_t bytes[234];size_t count=i%sizeof(bytes);for(size_t j=0;j<count;j++){seed=seed*1664525u+1013904223u;bytes[j]=seed>>24;}decode_waypoint(alice,MESH_BROADCAST_NODE,10000+i,now,0,bytes,count);}
    clean();puts("PASS: shared/private waypoint scope and exact port8 format, lock owner updates/deletion, delayed/duplicate/invalid reports, zero coordinates, expiration/tombstones, ACK semantics, timeout/restart, durable checksummed backup recovery and bounded storage independent of chat/unread alerts");
}
