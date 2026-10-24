#define main prior_direct_tests
#include "test_direct_messages.c"
#undef main
static size_t archived;static uint32_t archived_id;static size_t archived_reactions;
static void accept(const mesh_message_t *m,const mesh_reaction_t *r,size_t n,const char *sender) {
 assert(m->delivery!=MESH_DELIVERY_PENDING&&sender[0]);archived++;archived_id=m->packet_id;
 for(size_t i=0;i<n;i++)if(reaction_targets_message(&r[i],m))archived_reactions++;
}
static void add(unsigned id){assert(store_message("!11223344",(const uint8_t *)"older text",10,1800000000,0,false,1));messages[message_count-1].packet_id=id;}
int main(void) {
 reset();history_fs_ready=true;mesh_radio_set_history_archive(accept);assert(mesh_radio_set_history_limit(100));
 for(unsigned i=0;i<100;i++)add(i+1);
 assert(!archived);reactions[0]=(mesh_reaction_t){.reply_id=1,.channel=1};strcpy(reactions[0].emoji,"👍");reaction_count=1;
 add(101);assert(archived==1&&archived_id==1&&archived_reactions==1&&message_count==100&&mesh_radio_unread_channel(1)==100);
 // Read-first eviction still applies. A pending send is never archived/removed.
 messages[0].delivery=MESH_DELIVERY_PENDING;message_unread[50]=false;add(102);assert(archived==2&&archived_id==52&&messages[0].packet_id==2);
 mesh_message_t target=messages[1];assert(mesh_radio_delete_message(&target));assert(archived==2);
 size_t removed=0;assert(mesh_radio_clear_thread(0,1,&removed)&&removed==98&&message_count==1&&archived==2);
 // Capacity reductions capture only committed victims, before reactions compact.
 assert(mesh_radio_set_history_limit(500));for(unsigned i=0;i<249;i++)add(i+200);
 assert(!mkdir(HISTORY_TEMP_PATH,0700));assert(!mesh_radio_set_history_limit(100)&&archived==2);assert(!rmdir(HISTORY_TEMP_PATH));
 assert(mesh_radio_set_history_limit(100)&&archived==152&&message_count==100);
 mesh_radio_set_history_archive(NULL);add(999);assert(archived==152);
 puts("PASS: automatic eviction and successful cap reduction archive before compaction; pending/read-first/unread rules unchanged, delete/clear and failed saves do not archive");
}
