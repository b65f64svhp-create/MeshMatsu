#define main backend_tests
#include "test_direct_messages.c"
#undef main
static protobuf_field_t tf(const uint8_t *raw,size_t size,unsigned tag) {
    size_t o=0;protobuf_field_t f;while(o<size){assert(read_field(raw,size,&o,&f));if(f.field==tag)return f;}assert(false);return (protobuf_field_t){0};
}
static void ready_request(void) {
    reset();lora_config.loaded=true;lora_config.hop_limit=5;
    for(unsigned i=0;i<10;i++)nodes[i]=(mesh_node_t){.number=i+1};node_count=10;
}
static void telemetry_reply(uint32_t sender,uint32_t destination,uint32_t request_id,bool malformed,unsigned variant) {
    uint8_t tel[12],data[64],packet[96],radio[110];size_t t=0,d=0,p=0,r=0;
    const uint8_t metrics[]={8,84};assert(append_bytes_field(tel,sizeof(tel),&t,variant,metrics,malformed?1:2));
    assert(append_varint(data,sizeof(data),&d,8)&&append_varint(data,sizeof(data),&d,67));
    assert(append_bytes_field(data,sizeof(data),&d,2,tel,t));
    data[d++]=0x35;memcpy(data+d,&request_id,4);d+=4;
    packet[p++]=0x0d;memcpy(packet+p,&sender,4);p+=4;
    packet[p++]=0x15;memcpy(packet+p,&destination,4);p+=4;
    assert(append_bytes_field(packet,sizeof(packet),&p,4,data,d));
    assert(append_bytes_field(radio,sizeof(radio),&r,2,packet,p));decode_from_radio(radio,r);
}
static mesh_telemetry_request_t request_result(unsigned number) {
    mesh_telemetry_request_t r;assert(mesh_radio_get_telemetry_request(number,&r));return r;
}
static void poll_request(void) {
    last_radio_activity_us=last_heartbeat_us=fake_time;mesh_radio_poll();
}
#ifndef TELEMETRY_REQUEST_HELPERS_ONLY
int main(void) {
    ready_request();size_t writes=write_count;mesh_telemetry_request_t r;
    assert(mesh_radio_request_telemetry(0)==MESH_TELEMETRY_START_INVALID);
    assert(mesh_radio_request_telemetry(MESH_BROADCAST_NODE)==MESH_TELEMETRY_START_INVALID);
    assert(mesh_radio_request_telemetry(local_node_number)==MESH_TELEMETRY_START_INVALID);
    assert(mesh_radio_request_telemetry(999)==MESH_TELEMETRY_START_INVALID&&write_count==writes);
    assert(mesh_radio_request_telemetry(1)==MESH_TELEMETRY_START_OK);r=request_result(1);assert(r.state==MESH_TELEMETRY_WAITING);
    protobuf_field_t packet=tf(sent_bytes+4,sent_size-4,1),to=tf(packet.value,packet.length,2),data=tf(packet.value,packet.length,4);
    uint32_t dest;memcpy(&dest,to.value,4);assert(dest==1);
    assert(tf(data.value,data.length,1).integer==67&&tf(data.value,data.length,3).integer==1);
    protobuf_field_t payload=tf(data.value,data.length,2);assert(payload.length==2&&payload.value[0]==0x12&&payload.value[1]==0);
    assert(tf(packet.value,packet.length,9).integer==5&&tf(packet.value,packet.length,10).integer==1);
    assert(mesh_radio_request_telemetry(2)==MESH_TELEMETRY_START_BUSY&&write_count==writes+1);
    ack(local_node_number,r.request_id,0);ack(1,r.request_id,0);assert(request_result(1).state==MESH_TELEMETRY_WAITING);
    telemetry_reply(2,local_node_number,r.request_id,false,2);telemetry_reply(1,local_node_number,r.request_id+1,false,2);
    telemetry_reply(1,MESH_BROADCAST_NODE,r.request_id,false,2);telemetry_reply(1,local_node_number,r.request_id,true,2);
    telemetry_reply(1,local_node_number,r.request_id,false,3);assert(request_result(1).state==MESH_TELEMETRY_WAITING);
    telemetry_reply(1,local_node_number,r.request_id,false,2);assert(request_result(1).state==MESH_TELEMETRY_REPLY);
    mesh_device_metrics_t m;assert(mesh_radio_get_node_metrics(1,&m)&&m.battery==84);
    ack(local_node_number,r.request_id,1);assert(request_result(1).state==MESH_TELEMETRY_REPLY);
    assert(mesh_radio_request_telemetry(2)==MESH_TELEMETRY_START_COOLDOWN&&mesh_radio_telemetry_cooldown_seconds()==60);
    fake_time+=60000000;assert(!mesh_radio_telemetry_cooldown_seconds());
    assert(mesh_radio_request_telemetry(2)==MESH_TELEMETRY_START_OK);r=request_result(2);
    ack(3,r.request_id,1);assert(request_result(2).state==MESH_TELEMETRY_WAITING);
    ack(local_node_number,r.request_id,1);assert(request_result(2).state==MESH_TELEMETRY_FAILED);
    fake_time+=60000000;assert(mesh_radio_request_telemetry(3)==MESH_TELEMETRY_START_OK);r=request_result(3);
    writes=write_count;fake_time+=120000000;poll_request();assert(request_result(3).state==MESH_TELEMETRY_TIMEOUT&&write_count==writes);
    telemetry_reply(3,local_node_number,r.request_id,false,2);assert(request_result(3).state==MESH_TELEMETRY_REPLY);
    assert(mesh_radio_request_telemetry(4)==MESH_TELEMETRY_START_OK);r=request_result(4);
    begin_radio_reconnect();assert(request_result(4).state==MESH_TELEMETRY_INTERRUPTED);
    telemetry_reply(4,local_node_number,r.request_id,false,2);assert(request_result(4).state==MESH_TELEMETRY_INTERRUPTED);
    assert(mesh_radio_request_telemetry(5)==MESH_TELEMETRY_START_OFFLINE);
    ready_request();short_write=true;assert(mesh_radio_request_telemetry(1)==MESH_TELEMETRY_START_WRITE_FAILED&&!mesh_radio_get_telemetry_request(1,&r));
    assert(!mesh_radio_telemetry_cooldown_seconds());short_write=false;
    ready_request();nodes[node_count++]=(mesh_node_t){.number=local_node_number,.metrics={.available=true,.has_battery=true,.battery=88,.has_voltage=true,.voltage=3.9f,.sample_time=1800000000}};
    assert(mesh_radio_request_telemetry(1)==MESH_TELEMETRY_START_OK);
    packet=tf(sent_bytes+4,sent_size-4,1);data=tf(packet.value,packet.length,4);payload=tf(data.value,data.length,2);
    protobuf_field_t dm=tf(payload.value,payload.length,2);assert(tf(dm.value,dm.length,1).integer==88);
    protobuf_field_t stamp=tf(payload.value,payload.length,1);uint32_t sent_stamp;memcpy(&sent_stamp,stamp.value,4);assert(sent_stamp==1800000000);
    assert(tf(dm.value,dm.length,2).wire==5); // Own readings retain their real age and wire types.
    ready_request();
    // Bounded status cache, frozen destinations and getter/passive refresh do not send.
    for(unsigned i=1;i<=9;i++) {
        fake_time+=60000000;assert(mesh_radio_request_telemetry(i)==MESH_TELEMETRY_START_OK);r=request_result(i);
        telemetry_reply(i,local_node_number,r.request_id,false,2);
    }
    assert(!mesh_radio_get_telemetry_request(1,&r));writes=write_count;
    for(unsigned i=2;i<=9;i++)assert(mesh_radio_get_telemetry_request(i,&r)&&r.state==MESH_TELEMETRY_REPLY&&r.node==i);
    poll_request();assert(write_count==writes&&!message_count&&!reaction_count&&!mesh_radio_unread_total(false)&&!mesh_radio_unread_total(true));
    puts("PASS: explicit unicast telemetry encoding/oneof, hop limit, matched device reply, ACK isolation, wrong sender/ID/destination/variant, malformed reports, 60s cooldown, busy/timeout/late reply, NAK, reboot, short writes, bounded cache and no automatic resend/chat/unread effects");
}

#endif
