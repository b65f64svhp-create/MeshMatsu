#define EMOJI_TEST_MAIN old_emoji_tests
#include "test_emoji_reactions.c"
static protobuf_field_t reply_field(const uint8_t *data, size_t size, uint32_t tag) {
    size_t pos=0;protobuf_field_t f;
    while(pos<size){assert(read_field(data,size,&pos,&f));if(f.field==tag)return f;}
    assert(!"Missing field");return (protobuf_field_t){0};
}
static void sent_reply(uint32_t destination, uint8_t channel, uint32_t reply_id) {
    size_t n=((size_t)sent_bytes[2]<<8)|sent_bytes[3];
    protobuf_field_t packet=reply_field(sent_bytes+4,n,1), to=reply_field(packet.value,packet.length,2);
    uint32_t value;memcpy(&value,to.value,4);assert(value==destination);
    if(channel)assert(reply_field(packet.value,packet.length,3).integer==channel);
    protobuf_field_t data=reply_field(packet.value,packet.length,4),reply=reply_field(data.value,data.length,7);
    assert(reply.wire==5&&reply.length==4);memcpy(&value,reply.value,4);assert(value==reply_id);
    // A text reply must remain TEXT_MESSAGE_APP, without the reaction/emoji flag.
    assert(reply_field(data.value,data.length,1).integer==1);
    size_t offset=0;protobuf_field_t f;while(offset<data.length){assert(read_field(data.value,data.length,&offset,&f));assert(f.field!=8);}
    assert(reply_field(packet.value,packet.length,10).integer==1);
}
static void clean_history(void){remove(HISTORY_PATH);remove(HISTORY_BACKUP_PATH);remove(HISTORY_TEMP_PATH);}
int main(void) {
    reset();clean_history();const uint32_t alice=0x11223344;
    uint8_t channel[]={8,1,24,2};decode_channel(channel,sizeof(channel));
    channel_packet(1,alice,500);mesh_message_t parent=messages[0];
    reaction_packet(alice,UINT32_MAX,1,501,500,"quoted text",false,1);
    assert(messages[1].reply_id==500&&reaction_count==0&&mesh_radio_unread_channel(1)==2);
    mesh_message_t original;assert(mesh_radio_get_reply_parent(&messages[1],&original)&&original.packet_id==500);
    mesh_message_t wrong=messages[1];wrong.channel=0;assert(!mesh_radio_get_reply_parent(&wrong,&original));
    size_t writes=write_count;
    assert(!mesh_radio_send_reply(0,1,"",500)&&!mesh_radio_send_reply(0,1,"x",0)&&!mesh_radio_send_reply(local_node_number,0,"x",500));
    assert(write_count==writes);
    assert(mesh_radio_send_reply(0,1,"My reply",500));sent_reply(UINT32_MAX,1,500);
    assert(messages[2].reply_id==500&&messages[2].channel==1&&messages[2].delivery==MESH_DELIVERY_PENDING);
    ack(local_node_number,messages[2].packet_id,9);
    assert(messages[2].failure==MESH_FAILURE_RADIO&&messages[2].error_reason==9&&strstr(mesh_radio_delivery_text(&messages[2]),"duty-cycle"));
    mesh_message_t retry=messages[2];assert(mesh_radio_retry_message(&retry));sent_reply(UINT32_MAX,1,500);
    assert(messages[3].reply_id==500&&messages[3].error_reason==0&&messages[3].failure==MESH_FAILURE_UNKNOWN);
    assert(mesh_radio_send_reply(alice,0,"Private reply",700));sent_reply(alice,0,700);
    uint32_t id=messages[4].packet_id;ack(local_node_number,id,0);assert(messages[4].delivery==MESH_DELIVERY_PENDING);
    ack(alice,id,35);assert(strstr(mesh_radio_delivery_text(&messages[4]),"public key unknown"));
    // A locally missing ACK is not a fabricated Meshtastic routing error.
    fake_time+=90000001;mesh_radio_poll();assert(messages[3].delivery==MESH_DELIVERY_FAILED&&messages[3].failure==MESH_FAILURE_TIMEOUT&&messages[3].error_reason==0);
    assert(strstr(mesh_radio_delivery_text(&messages[3]),"90 seconds"));
    assert(mesh_radio_send_reply(alice,0,"Restart pending",700));
    history_fs_ready=true;assert(mesh_radio_flush_history());message_count=0;restore_message_history();
    assert(message_count==6&&messages[1].reply_id==500&&messages[2].error_reason==9&&messages[2].failure==MESH_FAILURE_RADIO);
    assert(messages[3].failure==MESH_FAILURE_TIMEOUT&&messages[5].failure==MESH_FAILURE_RESTARTED&&messages[5].delivery==MESH_DELIVERY_FAILED);
    assert(mesh_radio_get_reply_parent(&messages[1],&original)&&original.packet_id==parent.packet_id);
    // Ambiguous IDs and missing parents use the unavailable fallback.
    messages[0].packet_id=501;messages[1].reply_id=500;assert(!mesh_radio_get_reply_parent(&messages[1],&original));
    messages[0].packet_id=500;messages[2].packet_id=500;assert(!mesh_radio_get_reply_parent(&messages[1],&original));
    clean_history();reset();history_fs_ready=true;
    // A real v8 file uses its frozen 268-byte rows, including old padding.
    v8_mesh_message_t old={.packet_id=77,.channel=1,.delivery=MESH_DELIVERY_PENDING,.outgoing=true};
    strcpy(old.from,"You");strcpy(old.text,"Old message");uint8_t unread=0;
    history_header_t h={.magic=MESSAGE_HISTORY_MAGIC,.version=8,.count=1,.capacity=250};
    h.checksum=history_checksum(2166136261U,&h,offsetof(history_header_t,checksum));
    h.checksum=history_checksum(h.checksum,&old,sizeof(old));h.checksum=history_checksum(h.checksum,&unread,1);
    FILE *file=fopen(HISTORY_PATH,"wb");assert(file);assert(fwrite(&h,sizeof(h),1,file)==1&&fwrite(&old,sizeof(old),1,file)==1&&fwrite(&unread,1,1,file)==1);fclose(file);
    restore_message_history();assert(message_count==1&&history_limit==250&&!strcmp(messages[0].text,"Old message")&&messages[0].reply_id==0);
    assert(messages[0].delivery==MESH_DELIVERY_FAILED&&messages[0].failure==MESH_FAILURE_RESTARTED&&history_dirty);
    assert(mesh_radio_flush_history());file=fopen(HISTORY_PATH,"rb");assert(file&&fread(&h,sizeof(h),1,file)==1&&h.version==9);fclose(file);
    // Unknown future radio errors stay identifiable rather than being mislabeled.
    messages[0].failure=MESH_FAILURE_RADIO;messages[0].error_reason=999;
    assert(!strcmp(mesh_radio_delivery_text(&messages[0]),"Radio error 999"));
    for(unsigned reason=1;reason<=39;reason++) {messages[0].error_reason=reason;assert(mesh_radio_delivery_text(&messages[0])[0]);}
    reset();char boundary[202];memset(boundary,'x',200);boundary[200]=0;
    assert(mesh_radio_send_reply(alice,0,boundary,700));sent_reply(alice,0,700);
    boundary[200]='x';boundary[201]=0;writes=write_count;
    assert(!mesh_radio_send_reply(alice,0,boundary,700)&&write_count==writes);
    clean_history();puts("PASS: wire-compatible text replies in both chat types, quoted retry, scoped/ambiguous/missing parents, exact radio errors versus local timeouts/restart, v9 roundtrip and frozen v8 migration");
}
