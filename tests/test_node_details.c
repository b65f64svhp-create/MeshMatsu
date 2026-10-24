#define NODE_SEEN_PATH "node-seen.bin"
#define NODE_SEEN_TEMP_PATH "node-seen.tmp"
#define main message_tests
#include "test_direct_messages.c"
#undef main
#include "../main/node_details.c"

static size_t make_user(uint8_t *user, bool role, size_t key_size) {
 size_t n=0;const uint8_t id[]="!aabbccdd",name[]="BUL-TEST",short_name[]="TANM";
 assert(append_bytes_field(user,200,&n,1,id,sizeof(id)-1));
 assert(append_bytes_field(user,200,&n,2,name,sizeof(name)-1));
 assert(append_bytes_field(user,200,&n,3,short_name,sizeof(short_name)-1));
 if(role){assert(append_varint(user,200,&n,56)&&append_varint(user,200,&n,12));}
 uint8_t key[40];for(unsigned i=0;i<40;i++)key[i]=i;
 if(key_size)assert(append_bytes_field(user,200,&n,8,key,key_size));
 return n;
}
static void snapshot(const uint8_t *user,size_t size,bool snr){
 uint8_t node[300];size_t n=0;
 assert(append_varint(node,sizeof(node),&n,8)&&append_varint(node,sizeof(node),&n,0xaabbccdd));
 if(user)assert(append_bytes_field(node,sizeof(node),&n,2,user,size));
 if(snr){float value=0;node[n++]=0x25;memcpy(node+n,&value,4);n+=4;}
 decode_node(node,n);
}
static void receive_signal(bool mqtt){
 uint8_t packet[80];size_t n=0;uint32_t sender=0xaabbccdd,time=1800000000;
 packet[n++]=0x0d;memcpy(packet+n,&sender,4);n+=4;
 packet[n++]=0x3d;memcpy(packet+n,&time,4);n+=4;
 float snr=-7.5f;packet[n++]=0x45;memcpy(packet+n,&snr,4);n+=4;
 assert(append_varint(packet,sizeof(packet),&n,96)&&append_varint(packet,sizeof(packet),&n,(uint64_t)(int64_t)-178));
 if(mqtt){assert(append_varint(packet,sizeof(packet),&n,112)&&append_varint(packet,sizeof(packet),&n,1));}
 decode_mesh_packet(packet,n);
}
int main(void){
 reset();uint8_t user[200];size_t n=make_user(user,true,32);snapshot(user,n,true);
 mesh_node_t node;assert(mesh_radio_get_node_by_number(0xaabbccdd,&node));
 assert(!strcmp(node.name,"BUL-TEST")&&!strcmp(node.short_name,"TANM")&&!strcmp(node.user_id,"!aabbccdd"));
 assert(node.role==12&&node.user_info_available&&!strcmp(mesh_node_role_name(&node),"Client Base"));
 assert(node.snr_available&&node.snr==0&&!node.rssi_available&&node.first_seen>=VALID_UNIX_TIME);
 uint32_t first=node.first_seen;char key[45];mesh_node_public_key_text(&node,key);
 assert(!strcmp(key,"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="));
 snapshot(NULL,0,false);assert(mesh_radio_get_node_by_number(0xaabbccdd,&node));
 assert(node.public_key_available&&node.role==12&&node.first_seen==first&&!strcmp(node.short_name,"TANM"));
 receive_signal(false);assert(mesh_radio_get_node_by_number(0xaabbccdd,&node));
 assert(node.rssi_available&&node.rssi==-178&&node.snr==-7.5f&&node.last_heard==1800000000&&node.first_seen==first);
 // MQTT metadata must not manufacture an RF measurement.
 nodes[0].snr_available=nodes[0].rssi_available=false;receive_signal(true);
 assert(!nodes[0].snr_available&&!nodes[0].rssi_available);
 n=make_user(user,false,0);update_node_user(0xaabbccdd,user,n);
 assert(nodes[0].role==0&&nodes[0].user_info_available&&!nodes[0].public_key_available);
 assert(!strcmp(mesh_node_role_name(&nodes[0]),"Client"));mesh_node_public_key_text(&nodes[0],key);assert(!strcmp(key,"Unavailable"));
 n=make_user(user,true,31);snapshot(user,n,false);assert(!nodes[0].public_key_available);
 node=nodes[0];node.role=999;assert(!strcmp(mesh_node_role_name(&node),"Unknown role"));
 node.user_info_available=false;assert(!strcmp(mesh_node_role_name(&node),"Unavailable"));
 const uint8_t malformed[]={0x12,0x20,'x'};snapshot(malformed,sizeof(malformed),false);assert(!strcmp(nodes[0].name,"BUL-TEST"));
 // First-seen storage survives app restart without changing the radio or chat history.
 history_fs_ready=true;size_t writes=write_count;mesh_radio_flush_node_cache();assert(!node_seen_dirty&&write_count==writes);
 memset(&node_seen,0,sizeof(node_seen));restore_node_seen();assert(node_seen.count==1&&note_node_seen(0xaabbccdd)==first);
 FILE *file=fopen(NODE_SEEN_PATH,"wb");assert(file);fwrite("broken",6,1,file);fclose(file);
 restore_node_seen();assert(!node_seen.count);
 note_node_seen(42);mesh_radio_flush_node_cache();assert(node_seen.count==1);
 file=fopen(NODE_SEEN_PATH,"ab");fputc(0,file);fclose(file);restore_node_seen();assert(!node_seen.count);
 remove(NODE_SEEN_PATH);remove(NODE_SEEN_TEMP_PATH);
 assert(!message_count&&!reaction_count);
 puts("PASS: node identity/key/role decoding, proto3 Client default, malformed and partial snapshots, zero SNR, signed RSSI, MQTT isolation, first-seen persistence/corruption, Base64 and no radio writes");
}
