#define CHANNEL_TEST_MAIN channel_tests
#include "test_channel_messaging.c"

static void check_name(const mesh_message_t *message, const char *expected) {
    char name[40]; mesh_radio_message_sender_name(message,name,sizeof(name));assert(!strcmp(name,expected));
}
static void announce(uint32_t number, const char *name) {
    uint8_t user[80];size_t n=0;
    assert(append_bytes_field(user,sizeof(user),&n,2,(const uint8_t *)name,strlen(name)));
    update_node_user(number,user,n);
}
int main(void) {
    reset();
    channel_packet(0,0xaabbccdd,123);channel_packet(1,0xaabbccdd,124);
    assert(message_count==2);check_name(&messages[0],"!aabbccdd");
    nodes[node_count++]=(mesh_node_t){.number=0xaabbccdd};
    announce(0xaabbccdd,"Greenview Base Radio");
    check_name(&messages[0],"Greenview Base Radio");check_name(&messages[1],"Greenview Base Radio");
    assert(!strcmp(messages[0].from,"!aabbccdd"));
    size_t writes=write_count;uint32_t revision=history_revision;bool dirty=history_dirty;
    size_t unread=mesh_radio_unread_total(false);
    check_name(&messages[0],"Greenview Base Radio");
    assert(write_count==writes&&history_revision==revision&&history_dirty==dirty&&mesh_radio_unread_total(false)==unread);
    channel_packet(0,0xaabbccdd,123);assert(message_count==2); // Resolution must not break deduplication.
    announce(0xaabbccdd,"Renamed Radio");check_name(&messages[0],"Renamed Radio");
    history_fs_ready=true;persist_message_history();assert(mesh_radio_flush_history());
    memset(messages,0,sizeof(messages));message_count=0;restore_message_history();
    assert(message_count==2);check_name(&messages[0],"Renamed Radio");assert(!strcmp(messages[0].from,"!aabbccdd"));
    mesh_message_t message={.peer_node=0xaabbccdd};strcpy(message.from,"legacy");check_name(&message,"Renamed Radio");
    message.outgoing=true;check_name(&message,"You");
    message=(mesh_message_t){0};strcpy(message.from,"!AABBCCDD");check_name(&message,"Renamed Radio");
    strcpy(message.from,"!aabbccdg");check_name(&message,"!aabbccdg");
    strcpy(message.from,"legacy");check_name(&message,"legacy");
    nodes[0].name[0]=0;strcpy(nodes[0].short_name,"BUL");strcpy(message.from,"!aabbccdd");check_name(&message,"BUL");
    nodes[0].short_name[0]=0;check_name(&message,"!aabbccdd");
    node_count=0;check_name(&message,"!aabbccdd");
    char tiny[2]={'x','x'};mesh_radio_message_sender_name(&message,tiny,1);assert(tiny[0]==0&&tiny[1]=='x');
    mesh_radio_message_sender_name(&message,NULL,5);mesh_radio_message_sender_name(&message,tiny,0);
    mesh_radio_message_sender_name(NULL,tiny,sizeof(tiny));assert(tiny[1]==0);
    remove(HISTORY_PATH);
    puts("PASS: live and renamed sender names across channels/DMs/history; IDs, deduplication and unread preserved; fallbacks and output bounds");
}
