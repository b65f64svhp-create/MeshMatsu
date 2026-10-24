#define EMOJI_TEST_MAIN received_reaction_tests
#include "test_emoji_reactions.c"
#include "../main/emoji_picker.c"

static protobuf_field_t wire_field(const uint8_t *bytes, size_t size, uint32_t number) {
    size_t pos = 0; protobuf_field_t field;
    while(pos < size) { assert(read_field(bytes,size,&pos,&field)); if(field.field==number) return field; }
    assert(!"Missing field"); return (protobuf_field_t){0};
}
static void check_reaction_wire(const mesh_message_t *target, const char *emoji) {
    size_t length = ((size_t)sent_bytes[2]<<8)|sent_bytes[3];
    assert(sent_size == length+4);
    protobuf_field_t packet=wire_field(sent_bytes+4,length,1);
    protobuf_field_t to=wire_field(packet.value,packet.length,2);
    uint32_t destination; memcpy(&destination,to.value,4);
    assert(destination==(target->peer_node ? target->peer_node : UINT32_MAX));
    if(!target->peer_node && target->channel) assert(wire_field(packet.value,packet.length,3).integer==target->channel);
    protobuf_field_t data=wire_field(packet.value,packet.length,4);
    assert(wire_field(data.value,data.length,1).integer==1);
    protobuf_field_t payload=wire_field(data.value,data.length,2);
    assert(payload.length==strlen(emoji) && !memcmp(payload.value,emoji,payload.length));
    protobuf_field_t reply=wire_field(data.value,data.length,7), flag=wire_field(data.value,data.length,8);
    assert(reply.wire==5 && flag.wire==5 && reply.length==4 && flag.length==4);
    uint32_t id, is_emoji; memcpy(&id,reply.value,4); memcpy(&is_emoji,flag.value,4);
    assert(id==target->packet_id && is_emoji==1);
    assert(wire_field(packet.value,packet.length,9).integer==5);
    assert(wire_field(packet.value,packet.length,10).integer==1);
    assert(wire_field(packet.value,packet.length,11).integer==70);
}
int main(void) {
    received_reaction_tests(); reset();
    lora_config.loaded=true; lora_config.hop_limit=5;
    const uint8_t secondary[]={8,1,24,2}; decode_channel(secondary,sizeof(secondary));
    channel_packet(1,0x11223344,300);
    mesh_message_t target=messages[0];
    size_t unread=mesh_radio_unread_total(false), texts=message_count;
    assert(mesh_radio_send_reaction(&target,"👍")); check_reaction_wire(&target,"👍");
    assert(message_count==texts && mesh_radio_unread_total(false)==unread && reaction_count==1);
    assert(reactions[0].sender==local_node_number && reactions[0].reply_id==300 && reactions[0].channel==1);
    uint32_t old_id=reactions[0].packet_id;
    // Same local reaction can be requeued, but does not duplicate its badge.
    assert(mesh_radio_send_reaction(&target,"👍") && reaction_count==1);
    assert(mesh_radio_send_reaction(&target,"❤️") && reaction_count==2);
    assert(mesh_radio_send_direct(0xaabbccdd,"DM target"));
    mesh_message_t dm=messages[message_count-1];
    assert(mesh_radio_send_reaction(&dm,"😂")); check_reaction_wire(&dm,"😂");
    assert(reactions[2].peer_node==0xaabbccdd && reactions[2].channel==0);
    assert(reactions[2].packet_id!=old_id && message_count==texts+1);
    mesh_message_t forged=target; forged.channel=0;
    assert(!mesh_radio_send_reaction(&forged,"👍"));
    forged=target; forged.peer_node=0x55555555;
    assert(!mesh_radio_send_reaction(&forged,"👍"));
    forged=target; forged.packet_id=0;
    assert(!mesh_radio_send_reaction(&forged,"👍"));
    assert(!mesh_radio_send_reaction(NULL,"👍") && !mesh_radio_send_reaction(&target,""));
    char large[65]; memset(large,'x',64); large[64]=0;
    assert(!mesh_radio_send_reaction(&target,large));
    size_t before=reaction_count;
    short_write=true; assert(!mesh_radio_send_reaction(&target,"🔥")); short_write=false;
    status.reconnecting=true; assert(!mesh_radio_send_reaction(&target,"🔥")); status.reconnecting=false;
    assert(reaction_count==before);
    history_fs_ready=true; persist_message_history(); if (history_fs_ready) assert(mesh_radio_flush_history());
    memset(reactions,0,sizeof(reactions)); reaction_count=0; restore_message_history();
    assert(reaction_count==before && mesh_radio_reaction_count(&target)==2 && mesh_radio_reaction_count(&dm)==1);
    remove(HISTORY_PATH); history_fs_ready=false;
    for(unsigned i=0;i<510;i++) store_message("Other",(const uint8_t*)"new",3,0,0,false,0);
    assert(!mesh_radio_send_reaction(&target,"👍"));
    // Picker inserts complete UTF-8 into the draft without sending anything.
    for(size_t i=0;i<mesh_picker_count();i++) {
        const mesh_picker_emoji_t *choice=mesh_picker_get(i);
        mesh_text_token_t token=mesh_text_next(choice->text,10);
        assert(token.glyph>=0 && token.bytes==strlen(choice->text));
        char draft[201]="Hello "; size_t writes=write_count;
        assert(mesh_picker_insert(draft,sizeof(draft),i) && !strcmp(draft+6,choice->text));
        assert(write_count==writes);
    }
    assert(!mesh_picker_get(mesh_picker_count()));
    char draft[201]; memset(draft,'x',196); draft[196]=0;
    assert(mesh_picker_insert(draft,sizeof(draft),0) && strlen(draft)==200);
    assert(!mesh_picker_insert(draft,sizeof(draft),0) && strlen(draft)==200);
    memset(draft,'x',198); draft[198]=0;
    assert(!mesh_picker_insert(draft,sizeof(draft),0) && strlen(draft)==198);
    char unterminated[3]={'a','b','c'};
    assert(!mesh_picker_insert(unterminated,sizeof(unterminated),0));
    puts("PASS: exact channel/DM reaction wire fields, configured hops, local badges, dedup/persistence, stale-target/failure rejection, all picker glyphs and UTF-8 draft boundaries");
}
