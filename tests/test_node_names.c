#define main earlier_tests
#include "test_direct_messages.c"
#undef main
static void node_user(uint32_t number,const uint8_t *user,size_t size){uint8_t frame[240];size_t n=0;assert(append_varint(frame,sizeof(frame),&n,8)&&append_varint(frame,sizeof(frame),&n,number));assert(append_bytes_field(frame,sizeof(frame),&n,2,user,size));decode_node(frame,n);}
static protobuf_field_t field(const uint8_t *data,size_t size,uint32_t tag){size_t p=0;protobuf_field_t f;while(p<size){assert(read_field(data,size,&p,&f));if(f.field==tag)return f;}assert(false);return (protobuf_field_t){0};}
int main(void){
 reset();owner_user_size=0;node_info_requested=false;uint32_t peer=0xabcdef12;
 uint8_t user[192];size_t n=0;assert(append_bytes_field(user,sizeof(user),&n,2,(uint8_t*)"Greenview Base",14));assert(append_bytes_field(user,sizeof(user),&n,3,(uint8_t*)"GV",2));
 update_node_user(peer,user,n);assert(node_count==1&&!strcmp(nodes[0].name,"Greenview Base")&&!strcmp(nodes[0].short_name,"GV"));
 uint8_t partial[]={26,2,'X','Y'};node_user(peer,partial,sizeof(partial));assert(!strcmp(nodes[0].name,"Greenview Base")&&!strcmp(nodes[0].short_name,"XY"));
 uint8_t empty_long[]={18,0,26,2,'A','B'};node_user(0x123,empty_long,sizeof(empty_long));assert(node_count==2&&!strcmp(nodes[1].name,"AB"));
 uint8_t role_only[]={56,0};node_user(peer,role_only,sizeof(role_only));assert(!strcmp(nodes[0].name,"Greenview Base")&&!strcmp(nodes[0].short_name,"XY"));
 uint8_t renamed[]={18,7,'N','e','w',' ','B','U','L'};update_node_user(peer,renamed,sizeof(renamed));assert(!strcmp(nodes[0].name,"New BUL")&&!strcmp(nodes[0].short_name,"XY"));
 uint8_t malformed[]={18,8,'x'};update_node_user(peer,malformed,sizeof(malformed));assert(!strcmp(nodes[0].name,"New BUL"));
 uint8_t def[]={18,15,'M','e','s','h','t','a','s','t','i','c',' ','0','0','4','2'};node_user(0x42,def,sizeof(def));assert(!strcmp(nodes[node_count-1].name,"Meshtastic 0042"));
 update_node_user(0x42,partial,sizeof(partial));assert(!strcmp(nodes[node_count-1].name,"XY"));
 node_user(0x42,def,sizeof(def));assert(!strcmp(nodes[node_count-1].name,"XY")); // A stale default snapshot cannot undo the learned name.
 size_t writes=write_count;assert(!mesh_radio_request_node_info(peer)&&write_count==writes+1); // Only requests missing local User; no fabricated identity on mesh.
 node_user(local_node_number,user,n);assert(owner_user_size==n); // Preserve raw public User, including license/hardware/future values.
 assert(append_varint(user,sizeof(user),&n,48)&&append_varint(user,sizeof(user),&n,1));assert(append_varint(user,sizeof(user),&n,800)&&append_varint(user,sizeof(user),&n,19));cache_owner_user(user,n);
 writes=write_count;size_t messages_before=message_count;assert(mesh_radio_request_node_info(peer)&&write_count==writes+1&&message_count==messages_before);
 protobuf_field_t packet=field(sent_bytes+4,sent_size-4,1),to=field(packet.value,packet.length,2),data=field(packet.value,packet.length,4);uint32_t target;memcpy(&target,to.value,4);assert(target==peer);
 assert(field(data.value,data.length,1).integer==4&&field(data.value,data.length,3).integer==1);protobuf_field_t payload=field(data.value,data.length,2);assert(payload.length==n&&!memcmp(payload.value,user,n));
 writes=write_count;assert(!mesh_radio_request_node_info(peer)&&write_count==writes);fake_time+=60000001;assert(mesh_radio_request_node_info(peer));
 assert(!mesh_radio_request_node_info(MESH_BROADCAST_NODE)&&!mesh_radio_request_node_info(local_node_number));
 status.reconnecting=true;assert(!mesh_radio_request_node_info(peer));status.reconnecting=false;
 mesh_node_t before=nodes[0];update_node_user(peer,role_only,sizeof(role_only));assert(!strcmp(nodes[0].name,before.name)&&nodes[0].names_received);
 puts("PASS: early name announcements, long-name retention across partial/empty User snapshots, explicit rename, malformed data, actual local identity preservation, unicast NODEINFO request, throttling and no chat/unread side effects");
}
