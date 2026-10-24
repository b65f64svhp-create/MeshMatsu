#define main backend_tests
#include "test_direct_messages.c"
#undef main
static size_t metric_payload(uint8_t *out,unsigned mask,uint32_t battery,float voltage,float channel,float airtime,uint32_t uptime) {
    size_t n=0;
    if(mask&1){assert(append_varint(out,100,&n,8));assert(append_varint(out,100,&n,battery));}
    float values[]={voltage,channel,airtime};
    for(unsigned i=0;i<3;i++)if(mask&(2u<<i)){out[n++]=(uint8_t)(((i+2)<<3)|5);memcpy(out+n,&values[i],4);n+=4;}
    if(mask&16){assert(append_varint(out,100,&n,40));assert(append_varint(out,100,&n,uptime));}
    return n;
}
static void telemetry_packet(uint32_t sender,uint32_t destination,uint32_t id,uint32_t stamp,
                             unsigned variant,const uint8_t *metrics,size_t size) {
    uint8_t tel[140],data[170],packet[230];size_t t=0,d=0,p=0;
    tel[t++]=0x0d;memcpy(tel+t,&stamp,4);t+=4;
    assert(append_bytes_field(tel,sizeof(tel),&t,variant,metrics,size));
    assert(append_varint(data,sizeof(data),&d,8)&&append_varint(data,sizeof(data),&d,67));
    assert(append_bytes_field(data,sizeof(data),&d,2,tel,t));
    packet[p++]=0x0d;memcpy(packet+p,&sender,4);p+=4;
    packet[p++]=0x15;memcpy(packet+p,&destination,4);p+=4;
    packet[p++]=0x35;memcpy(packet+p,&id,4);p+=4;
    assert(append_bytes_field(packet,sizeof(packet),&p,4,data,d));decode_mesh_packet(packet,p);
}
static void cached_node(uint32_t number,const uint8_t *metrics,size_t size,uint32_t last_heard) {
    uint8_t node[150];size_t n=0;
    assert(append_varint(node,sizeof(node),&n,8)&&append_varint(node,sizeof(node),&n,number));
    assert(append_bytes_field(node,sizeof(node),&n,6,metrics,size));
    node[n++]=0x2d;memcpy(node+n,&last_heard,4);n+=4;decode_node(node,n);
}
#ifndef TELEMETRY_HELPERS_ONLY
int main(void) {
    reset();uint8_t raw[100];size_t n=metric_payload(raw,31,84,3.82f,12.5f,1.25f,3600);
    cached_node(42,raw,n,1800000000);mesh_device_metrics_t m;
    assert(mesh_radio_get_node_metrics(42,&m)&&m.cached&&!m.sample_time&&!m.received_us);
    assert(m.has_battery&&m.battery==84&&m.has_voltage&&fabsf(m.voltage-3.82f)<.001f&&m.uptime==3600);
    // last-heard is not the metrics' independent measurement timestamp.
    incoming(42,MESH_BROADCAST_NODE,44,3);assert(mesh_radio_get_node_metrics(42,&m)&&m.cached&&!m.sample_time);
    telemetry_packet(42,MESH_BROADCAST_NODE,101,1800000100,2,raw,n);
    assert(mesh_radio_get_node_metrics(42,&m)&&!m.cached&&m.sample_time==1800000100&&m.packet_id==101);
    int64_t received=m.received_us;fake_time+=60000000;
    telemetry_packet(42,MESH_BROADCAST_NODE,101,1800000100,2,raw,n);assert(nodes[0].metrics.received_us==received);
    // Old live reports and later cached dumps cannot overwrite the newer report.
    n=metric_payload(raw,31,1,3.1f,90,20,10);
    telemetry_packet(42,MESH_BROADCAST_NODE,102,1800000000,2,raw,n);
    cached_node(42,raw,n,1800000200);assert(mesh_radio_get_node_metrics(42,&m)&&m.battery==84&&!m.cached&&m.sample_time==1800000100);
    // A partial new report clears absent readings instead of giving them its new age.
    n=metric_payload(raw,2,0,4.1f,0,0,0);telemetry_packet(42,MESH_BROADCAST_NODE,103,1800000200,2,raw,n);
    assert(mesh_radio_get_node_metrics(42,&m)&&!m.has_battery&&m.has_voltage&&!m.has_channel_use&&!m.has_airtime&&!m.has_uptime);
    n=metric_payload(raw,31,0,0,0,0,0);telemetry_packet(42,MESH_BROADCAST_NODE,104,1800000300,2,raw,n);
    assert(mesh_radio_get_node_metrics(42,&m)&&m.has_battery&&!m.battery&&m.has_voltage&&!m.voltage&&m.has_channel_use&&!m.channel_use&&m.has_uptime&&!m.uptime);
    n=metric_payload(raw,1,101,0,0,0,0);telemetry_packet(42,MESH_BROADCAST_NODE,105,0,2,raw,n);
    assert(mesh_radio_get_node_metrics(42,&m)&&m.battery==101&&!m.sample_time&&!m.cached);
    // Float validity, field presence, unknown future fields and malformed payloads.
    n=metric_payload(raw,15,75,NAN,INFINITY,-1,0);telemetry_packet(42,MESH_BROADCAST_NODE,106,0,2,raw,n);
    assert(mesh_radio_get_node_metrics(42,&m)&&m.has_battery&&m.battery==75&&!m.has_voltage&&!m.has_channel_use&&!m.has_airtime);
    const uint8_t bad[]={0x15,0,0};telemetry_packet(42,MESH_BROADCAST_NODE,107,0,2,bad,sizeof(bad));
    assert(nodes[0].metrics.packet_id==106);
    n=metric_payload(raw,12,0,0,100.1f,100.1f,0);telemetry_packet(42,MESH_BROADCAST_NODE,108,0,2,raw,n);assert(nodes[0].metrics.packet_id==106);
    n=metric_payload(raw,1,50,0,0,0,0);append_varint(raw,sizeof(raw),&n,0x320);append_varint(raw,sizeof(raw),&n,123);
    telemetry_packet(42,MESH_BROADCAST_NODE,109,0,2,raw,n);assert(nodes[0].metrics.battery==50);
    telemetry_packet(42,MESH_BROADCAST_NODE,110,0,3,raw,n);assert(nodes[0].metrics.packet_id==109); // Environment variant is not DeviceMetrics.
    telemetry_packet(43,0xabcdef01,111,0,2,raw,n);assert(!mesh_radio_get_node_metrics(43,&m));
    telemetry_packet(0,MESH_BROADCAST_NODE,112,0,2,raw,n);assert(!mesh_radio_get_node_metrics(0,&m));
    // Local telemetry's real nonzero packet IDs must bypass own-text echo suppression.
    telemetry_packet(local_node_number,MESH_BROADCAST_NODE,201,1800000400,2,raw,n);
    assert(mesh_radio_get_node_metrics(local_node_number,&m)&&m.battery==50);
    assert(mesh_radio_get_local_activity(&m)&&m.packet_id==201);
    // LocalStats uses different tags; defaults are genuine proto3 zero readings.
    uint8_t stats[]={8,90,0x15,0,0,0x48,0x41,0x1d,0,0,0xa0,0x3f};
    telemetry_packet(42,MESH_BROADCAST_NODE,202,1800000500,6,stats,sizeof(stats));
    assert(mesh_radio_get_local_activity(&m)&&m.packet_id==201);
    telemetry_packet(local_node_number,MESH_BROADCAST_NODE,203,1800000500,6,stats,sizeof(stats));
    assert(mesh_radio_get_local_activity(&m)&&m.uptime==90&&m.channel_use==12.5f&&m.airtime==1.25f&&!m.has_battery);
    telemetry_packet(local_node_number,MESH_BROADCAST_NODE,204,1800000600,6,raw,0);
    assert(mesh_radio_get_local_activity(&m)&&m.has_channel_use&&!m.channel_use&&m.has_uptime&&!m.uptime);
    assert(mesh_radio_get_node_metrics(local_node_number,&m)&&m.battery==50); // LocalStats doesn't erase battery report.
    local_node_number=1234;assert(!mesh_radio_get_local_activity(&m));
    // NodeInfo before MyNodeInfo is still usable for own cached activity later.
    reset();n=metric_payload(raw,31,101,0,5,0,10);cached_node(5432,raw,n,1800000000);local_node_number=5432;
    assert(mesh_radio_get_local_activity(&m)&&m.cached&&m.channel_use==5&&!m.sample_time);
    cached_node(42,raw,0,1800000000);mesh_node_t node;assert(mesh_radio_get_node_by_number(42,&node)&&!node.metrics.available);
    // Reconnect retains ages/old reports, but app start/reset clears volatile metrics.
    begin_radio_reconnect();assert(mesh_radio_get_node_metrics(5432,&m)&&m.cached);
    assert(!write_count&&!message_count&&!reaction_count&&!mesh_radio_unread_total(true)&&!mesh_radio_unread_total(false));
    puts("PASS: NodeInfo/port67/local echo, optional zero/missing fields, report ages, stale/cache preservation, partial reports, floats, malformed data, external power, LocalStats tags/defaults, sender scope and zero telemetry transmissions/unread changes");
}

#endif
