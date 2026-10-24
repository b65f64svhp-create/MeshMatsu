#define main original_message_tests
#include "test_direct_messages.c"
#undef main
#include <sys/stat.h>
static void clean(void){remove(HISTORY_PATH);remove(HISTORY_BACKUP_PATH);remove(HISTORY_TEMP_PATH);reset();history_fs_ready=true;}
static void add(uint32_t id,uint8_t channel,bool unread){assert(store_message("Peer",(const uint8_t*)"saved text",10,1800000000,0,false,channel));messages[message_count-1].packet_id=id;message_unread[message_count-1]=unread;}
#include "message_reuse.h"
int main(void){
 clean();add(11,1,true);add(11,0,true);add(12,1,true);messages[2].peer_node=42;
 reactions[0]=(mesh_reaction_t){.reply_id=11,.channel=1};strcpy(reactions[0].emoji,"ok");reactions[1]=(mesh_reaction_t){.reply_id=11,.channel=0};strcpy(reactions[1].emoji,"yes");reaction_count=2;
 assert(mesh_radio_send_direct(42,"pending"));int64_t sent=message_sent_us[3];uint32_t pending=messages[3].packet_id;
 mesh_message_t target=messages[0];size_t writes=write_count;
 assert(mkdir(HISTORY_TEMP_PATH,0700)==0);assert(!mesh_radio_delete_message(&target)&&message_count==4&&reaction_count==2);rmdir(HISTORY_TEMP_PATH);
 assert(mesh_radio_delete_message(&target)&&message_count==3&&reaction_count==1&&reactions[0].channel==0&&mesh_radio_unread_channel(1)==0&&mesh_radio_unread_channel(0)==1&&mesh_radio_unread_direct(42)==1);
 assert(messages[2].packet_id==pending&&message_sent_us[2]==sent&&write_count==writes&&!mesh_radio_delete_message(&target));
 assert(!mesh_radio_delete_message(&messages[2]));restore_message_history();assert(message_count==3&&reaction_count==1); // Pending is marked interrupted after reload.
 // Identical retained snapshots are ambiguous and cannot remove the wrong row.
 clean();add(0,1,true);add(0,1,true);target=messages[0];assert(!mesh_radio_delete_message(&target)&&message_count==2);
 messages[1].timestamp++;assert(mesh_radio_delete_message(&target)&&message_count==1);
 // Same packet ID from another sender is not selected by row position.
 clean();add(88,1,true);target=messages[0];strcpy(messages[0].from,"Other");assert(!mesh_radio_delete_message(&target));
 // A clear removes orphan reactions only in its scope, preserving pending parents.
 clean();add(1,0,true);assert(mesh_radio_send_direct(42,"pending"));
 reactions[0]=(mesh_reaction_t){.reply_id=999,.channel=0};reactions[1]=(mesh_reaction_t){.reply_id=999,.channel=1};reactions[2]=(mesh_reaction_t){.reply_id=messages[1].packet_id,.peer_node=42};reaction_count=3;
 size_t removed=0;assert(mesh_radio_clear_thread(0,0,&removed)&&removed==1&&reaction_count==2);assert(mesh_radio_clear_thread(42,0,&removed)&&!removed&&reaction_count==2);
 // Reuse truncates only at complete Unicode characters, including 4-byte emoji.
 char copied[201],text[240];memset(text,'a',198);strcpy(text+198,"🙂more");assert(mesh_message_reuse(copied,sizeof(copied),text)==198&&strlen(copied)==198);
 memset(text,'a',196);strcpy(text+196,"🙂more");assert(mesh_message_reuse(copied,sizeof(copied),text)==200&&!strcmp(copied+196,"🙂"));
 strcpy(text,"it's working ❤️");assert(mesh_message_reuse(copied,sizeof(copied),text)==strlen(text)&&!strcmp(copied,text));
 assert(mesh_message_reuse(copied,sizeof(copied),"bad\xed\xa0\x80text")==3&&!strcmp(copied,"bad"));
 clean();puts("PASS: transactional scoped deletion, unread/reaction/timing compaction, persistence, pending protection, stale/ambiguous targets, orphan cleanup and UTF-8 draft reuse");
}
