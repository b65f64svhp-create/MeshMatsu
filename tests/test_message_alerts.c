#define main message_tests
#include "test_direct_messages.c"
#undef main
static void text(uint32_t sender,uint32_t dest,uint32_t id,unsigned channel) {
    const uint8_t data[]={8,1,18,2,'h','i'};
    decode_data_message(data,sizeof(data),sender,0,dest,id,channel);
}
int main(void) {
    reset();assert(!mesh_radio_take_message_alerts());
    assert(mesh_radio_send_text("outgoing"));assert(!mesh_radio_take_message_alerts());
    text(0x11111111,MESH_BROADCAST_NODE,101,2);assert(mesh_radio_take_message_alerts()==4);
    assert(!mesh_radio_take_message_alerts());
    text(0x11111111,MESH_BROADCAST_NODE,101,2);assert(!mesh_radio_take_message_alerts()); // duplicate
    text(0x22222222,local_node_number,102,0);text(0x11111111,MESH_BROADCAST_NODE,103,7);
    assert(mesh_radio_take_message_alerts()==((1u<<8)|(1u<<7)));assert(!mesh_radio_take_message_alerts());
    const uint8_t reaction[]={8,1,18,1,'!',0x3d,101,0,0,0,0x45,1,0,0,0};
    decode_data_message(reaction,sizeof(reaction),0x11111111,0,MESH_BROADCAST_NODE,104,2);
    assert(reaction_count==1&&!mesh_radio_take_message_alerts());
    text(0x11111111,0x33333333,105,0);text(0x11111111,MESH_BROADCAST_NODE,106,8);
    assert(!mesh_radio_take_message_alerts());
    text(local_node_number,MESH_BROADCAST_NODE,107,0);assert(!mesh_radio_take_message_alerts());
    // Storage/restore is separate from decoder events; marking read never sets events.
    assert(store_message("!11111111",(const uint8_t*)"saved",5,0,0,false,1));
    assert(!mesh_radio_take_message_alerts());mesh_radio_mark_read(0,2);assert(!mesh_radio_take_message_alerts());
    text(0x11111111,MESH_BROADCAST_NODE,108,0);text(0x11111111,MESH_BROADCAST_NODE,109,0);
    assert(mesh_radio_take_message_alerts()==1); // burst coalesces
    text(0x11111111,MESH_BROADCAST_NODE,110,1);assert(message_alerts==2);
    mesh_radio_start();assert(!mesh_radio_take_message_alerts());
    puts("PASS: live-message event flags coalesce and drain, per-channel/DM routing, duplicate/outgoing/reaction/history isolation, invalid destinations and startup reset");
}
