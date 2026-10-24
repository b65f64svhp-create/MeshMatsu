#define main old_message_tests
#include "test_direct_messages.c"
#undef main
#include "message_navigation.c"
static void add(uint32_t id,uint8_t channel,bool unread){assert(store_message("!11223344",(const uint8_t*)"text",4,1800000000+id,0,false,channel));messages[message_count-1].packet_id=id;message_unread[message_count-1]=unread;}
int main(void){
 reset();add(1,1,false);add(2,0,true);add(3,1,true);add(4,1,true);add(5,1,true);assert(store_message("!55667788",(const uint8_t*)"dm",2,1800000000,42,false,0));
 size_t index=99;assert(mesh_radio_first_unread(0,1,&index)&&index==1);mesh_message_anchor_t anchor={0};size_t offset=mesh_message_open_offset(0,1,&anchor);assert(offset==2&&anchor.valid&&anchor.message.packet_id==3);
 size_t writes=write_count;mesh_radio_mark_read_range(0,1,0,2);assert(mesh_radio_unread_channel(1)==2&&mesh_radio_unread_channel(0)==1&&mesh_radio_unread_direct(42)==1&&write_count==writes);
 add(6,1,true);offset=mesh_message_resolve_offset(0,1,offset,&anchor);assert(offset==3&&anchor.message.packet_id==3); // arrivals do not shift the viewed message
 offset--;mesh_message_capture_anchor(0,1,offset,&anchor);assert(anchor.message.packet_id==4);mesh_radio_mark_read_range(0,1,2,3);assert(mesh_radio_unread_channel(1)==2);
 assert(mesh_radio_first_unread(0,1,&index)&&index==3);offset=mesh_message_open_offset(0,1,&anchor);assert(offset==1&&anchor.message.packet_id==5);
 // Removing the anchor keeps the viewport at retained history rather than skipping to the tail.
 assert(mesh_radio_delete_message(&anchor.message)==false); // filesystem is not mounted in this fixture
 remove_history_message(4);offset=mesh_message_resolve_offset(0,1,offset,&anchor);assert(offset==3&&anchor.message.packet_id==1);
 offset=mesh_message_resolve_offset(0,1,0,&anchor);assert(!offset&&!anchor.valid);
 mesh_radio_mark_read_range(0,1,0,SIZE_MAX);assert(!mesh_radio_unread_channel(1)&&mesh_radio_unread_channel(0)==1&&mesh_radio_unread_direct(42)==1);
 assert(!mesh_message_open_offset(0,1,&anchor));assert(!mesh_radio_first_unread(UINT32_MAX,0,&index)&&!mesh_radio_first_unread(0,8,&index));
 mesh_radio_mark_read_range(0,0,2,1);assert(mesh_radio_unread_channel(0)==1);
 // Read-range changes survive a normal history save/restart without changing other chats.
 reset();history_fs_ready=true;remove(HISTORY_PATH);remove(HISTORY_BACKUP_PATH);remove(HISTORY_TEMP_PATH);
 add(101,1,true);add(102,1,true);add(103,0,true);uint32_t revision=mesh_radio_history_revision();
 mesh_radio_mark_read_range(0,1,0,1);assert(history_dirty&&mesh_radio_history_revision()!=revision&&mesh_radio_flush_history());
 restore_message_history();assert(mesh_radio_unread_channel(1)==1&&mesh_radio_unread_channel(0)==1&&mesh_radio_first_unread(0,1,&index)&&index==1);
 puts("PASS: first-unread channel/DM scope, visible-range reads, persisted unread revision, arrival-stable anchors, scroll capture, deletion fallback and no radio transmissions");
}
