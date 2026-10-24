#define main old_message_tests
#include "test_direct_messages.c"
#undef main
#include <sys/stat.h>
static void clean(void) {
    remove(HISTORY_PATH); remove(HISTORY_BACKUP_PATH); remove(HISTORY_TEMP_PATH); remove(LEGACY_HISTORY_PATH);
    reset(); history_fs_ready = true;
}
static void add(uint32_t id, uint8_t channel, bool unread) {
    assert(store_message("Peer", (const uint8_t *)"saved text", 10, 1800000000, 0, false, channel));
    messages[message_count - 1].packet_id = id;
    message_unread[message_count - 1] = unread;
}
int main(void) {
    clean(); assert(mesh_radio_history_limit() == 500);
    add(10, 1, true); persist_message_history();
    FILE *file = fopen(HISTORY_PATH, "rb"); assert(!file); // Deferred saves do not rewrite per change.
    fake_time += 1999999; mesh_radio_poll_history(false);assert(access(HISTORY_PATH,F_OK)!=0);
    fake_time++;mesh_radio_poll_history(true);assert(access(HISTORY_PATH,F_OK)!=0);
    fake_time += 8000000;mesh_radio_poll_history(true);assert(access(HISTORY_PATH,F_OK)==0&&!history_dirty);
    assert(mesh_radio_set_history_limit(1000));
    for (unsigned i = 1; i < 1000; i++) add(10 + i, i % 2, true);
    assert(message_count == 1000 && mesh_radio_unread_total(false) == 1000);
    reactions[0] = (mesh_reaction_t){.reply_id=1009, .channel=1, .sender=12}; strcpy(reactions[0].emoji,"ok");reaction_count=1;
    assert(mesh_radio_flush_history());
    memset(messages, 0, sizeof(messages)); memset(message_unread,0,sizeof(message_unread));message_count=0;
    restore_message_history(); assert(history_limit==1000&&message_count==1000&&message_unread[999]&&messages[999].packet_id==1009&&reaction_count==1);
    // Limit reduction is transactional; remove read entries before unread ones.
    message_unread[800]=false;assert(mesh_radio_set_history_limit(500));assert(message_count==500&&mesh_radio_unread_total(false)==500);
    for(size_t i=0;i<message_count;i++)assert(messages[i].packet_id!=810);
    assert(reaction_count==1&&mesh_radio_reaction_count(&messages[499])==1);
    size_t before=message_count;assert(!mesh_radio_set_history_limit(51)&&message_count==before);
    // A failed file write cannot activate a smaller limit or clear a thread.
    assert(mkdir(HISTORY_TEMP_PATH,0700)==0);
    assert(!mesh_radio_set_history_limit(100)&&history_limit==500&&message_count==before);
    size_t removed=999;assert(!mesh_radio_clear_thread(0,1,&removed)&&message_count==before&&removed==999);
    rmdir(HISTORY_TEMP_PATH);
    // Clearing is scoped and preserves pending sends, timing data, other chat reactions.
    clean();add(1,0,true);add(2,1,true);
    assert(mesh_radio_send_direct(0xaabbccdd,"pending dm"));uint32_t pending=messages[2].packet_id;int64_t started=message_sent_us[2];
    add(3,0,false);messages[3].peer_node=0xaabbccdd;
    reactions[0]=(mesh_reaction_t){.reply_id=1,.channel=0};strcpy(reactions[0].emoji,"a");
    reactions[1]=(mesh_reaction_t){.reply_id=2,.channel=1};strcpy(reactions[1].emoji,"b");reaction_count=2;
    assert(mesh_radio_clear_thread(0,0,&removed)&&removed==1&&message_count==3&&reaction_count==1&&reactions[0].reply_id==2);
    assert(mesh_radio_clear_thread(0xaabbccdd,0,&removed)&&removed==1&&message_count==2);
    assert(messages[1].packet_id==pending&&message_sent_us[1]==started&&messages[1].delivery==MESH_DELIVERY_PENDING);
    assert(mesh_radio_unread_channel(1)==1);
    // All slots pending: never evict an in-flight send or transmit without retaining it.
    clean();assert(mesh_radio_set_history_limit(100));
    for(unsigned i=0;i<100;i++)assert(mesh_radio_send_direct(0xaabbccdd,"waiting"));
    size_t writes=write_count;assert(!mesh_radio_send_direct(0xaabbccdd,"overflow")&&write_count==writes&&message_count==100);
    incoming(0xaabbccdd,MESH_BROADCAST_NODE,999,3);assert(message_count==100&&messages[99].outgoing);
    // Retry is exactly one normal new send, with frozen destination/channel/text.
    clean();add(51,1,false);messages[0].outgoing=true;messages[0].delivery=MESH_DELIVERY_FAILED;
    mesh_message_t target=messages[0];writes=write_count;
    const uint8_t ch[]={8,1,24,2};decode_channel(ch,sizeof(ch));
    assert(mesh_radio_retry_message(&target)&&write_count==writes+1&&message_count==2&&messages[1].channel==1&&messages[1].packet_id!=target.packet_id&&messages[1].delivery==MESH_DELIVERY_PENDING);
    target.channel=2;assert(!mesh_radio_retry_message(&target));target=messages[0];messages[0].delivery=MESH_DELIVERY_ACKNOWLEDGED;assert(!mesh_radio_retry_message(&target));
    // Migrate v7's exact fixed 50-record layout and the bit at index 49.
    clean();stored_message_history_t old={.magic=MESSAGE_HISTORY_MAGIC,.version=7,.count=50,.unread_mask=UINT64_C(1)<<49,.reaction_count=1};
    for(unsigned i=0;i<50;i++){old.messages[i]=(v8_mesh_message_t){.packet_id=i+1,.channel=1};strcpy(old.messages[i].from,"Peer");strcpy(old.messages[i].text,"legacy");}
    old.reactions[0]=(mesh_reaction_t){.reply_id=50,.channel=1};strcpy(old.reactions[0].emoji,"ok");
    file=fopen(HISTORY_PATH,"wb");assert(file&&fwrite(&old,sizeof(old),1,file)==1);fclose(file);
    restore_message_history();assert(message_count==50&&message_unread[49]&&!message_unread[48]&&reaction_count==1&&history_limit==500);
    history_header_t header;file=fopen(HISTORY_PATH,"rb");assert(file&&fread(&header,sizeof(header),1,file)==1&&header.version==CURRENT_HISTORY_VERSION);fclose(file);
    // Corrupt/truncated current snapshot recovers the previous complete snapshot.
    add(999,1,true);assert(mesh_radio_flush_history());
    file=fopen(HISTORY_PATH,"r+b");assert(file);fseek(file,20,SEEK_SET);fputc(0xff,file);fclose(file);
    restore_message_history();assert(message_count==50&&message_unread[49]&&history_dirty);
    assert(mesh_radio_flush_history());restore_message_history();assert(message_count==50&&reaction_count==1);
    // A reduction cannot discard pending records even when they exceed the new limit.
    clean();for(unsigned i=0;i<150;i++)assert(mesh_radio_send_direct(0xaabbccdd,"pending"));
    assert(!mesh_radio_set_history_limit(100)&&message_count==150&&history_limit==500);
    clean();
    puts("PASS: v8 1000-record unread/reaction roundtrip, v7 migration, transactional limits/clear, read-first eviction, pending protection, failed storage, backup recovery and one-cycle scoped retry");
}
