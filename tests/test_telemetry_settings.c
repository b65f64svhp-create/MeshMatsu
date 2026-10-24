#define main message_tests
#include "test_direct_messages.c"
#undef main

static protobuf_field_t field(const uint8_t *data, size_t size, uint32_t tag) {
    size_t n = 0; protobuf_field_t f;
    while (n < size) { assert(read_field(data, size, &n, &f)); if (f.field == tag) return f; }
    assert(false); return (protobuf_field_t){0};
}
static bool has(const uint8_t *data, size_t size, uint32_t tag) {
    size_t n=0; protobuf_field_t f;
    while(n<size) { assert(read_field(data,size,&n,&f)); if(f.field==tag)return true; }
    return false;
}
static size_t module(uint8_t *out, bool enabled, uint32_t interval, bool include_switch) {
    uint8_t raw[100]; size_t n=0,m=0;
    append_varint(raw,sizeof(raw),&n,8);append_varint(raw,sizeof(raw),&n,interval);
    // Environment/power/health settings and future fields must survive edits.
    const uint8_t unknown[]={16,0x88,0x0e,24,1,40,1,64,1,96,0x90,0x1c,0x98,6,77,0xa2,6,3,'n','e','w'};
    memcpy(raw+n,unknown,sizeof(unknown));n+=sizeof(unknown);
    if(include_switch){append_varint(raw,sizeof(raw),&n,112);append_varint(raw,sizeof(raw),&n,enabled);}
    append_varint(out,160,&m,800);append_varint(out,160,&m,42);
    append_bytes_field(out,160,&m,6,raw,n);
    return m;
}
static void receive(bool enabled, uint32_t interval, bool include_switch, uint32_t id, uint32_t sender, uint32_t dest) {
    uint8_t m[160],a[180],d[220];size_t n=module(m,enabled,interval,include_switch),an=0,dn=0;
    append_bytes_field(a,sizeof(a),&an,8,m,n);
    append_varint(d,sizeof(d),&dn,8);append_varint(d,sizeof(d),&dn,6);
    append_bytes_field(d,sizeof(d),&dn,2,a,an);d[dn++]=0x35;memcpy(d+dn,&id,4);dn+=4;
    decode_data_message(d,dn,sender,0,dest,123,0);
}
static void ready(void) {
    reset();fake_time=1000000;last_radio_activity_us=last_rx_byte_us=last_heartbeat_us=fake_time;
    lora_config.loaded=true;session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;
    radio_metadata.loaded=true;strcpy(radio_metadata.firmware_version,"2.7.18");
    uint8_t m[160],r[180];size_t n=module(m,false,1800,false),rn=0;
    append_bytes_field(r,sizeof(r),&rn,9,m,n);assert(valid_radio_payload(r,rn));decode_from_radio(r,rn);
    assert(telemetry_config.loaded&&telemetry_config.enable_supported&&!telemetry_config.enabled);
}
static protobuf_field_t sent_admin(void) {
    protobuf_field_t p=field(sent_bytes+4,sent_size-4,1),d=field(p.value,p.length,4);
    assert(field(d.value,d.length,1).integer==6);
    protobuf_field_t to=field(p.value,p.length,2);uint32_t dest;memcpy(&dest,to.value,4);assert(dest==local_node_number);
    assert(field(d.value,d.length,3).integer==1);
    return field(d.value,d.length,2);
}
static uint32_t readback(void) {
    fake_time+=1600000;mesh_radio_poll();assert(telemetry_config_op.read_id);
    protobuf_field_t a=sent_admin();assert(field(a.value,a.length,7).integer==5&&!has(a.value,a.length,35));
    return telemetry_config_op.read_id;
}
int main(void) {
    ready();mesh_telemetry_config_t draft;assert(mesh_radio_get_telemetry_config(&draft));
    draft.enabled=true;draft.interval_secs=3600;uint32_t original_rev=draft.revision;
    assert(mesh_radio_set_telemetry_config(&draft));assert(!mesh_radio_set_telemetry_config(&draft));
    assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING&&!telemetry_config.enabled&&telemetry_config.interval_secs==1800);
    protobuf_field_t a=sent_admin(),m=field(a.value,a.length,35),raw=field(m.value,m.length,6);
    assert(field(raw.value,raw.length,1).integer==3600&&field(raw.value,raw.length,14).integer==1);
    assert(field(raw.value,raw.length,2).integer==1800&&field(raw.value,raw.length,3).integer==1);
    assert(field(raw.value,raw.length,5).integer==1&&field(raw.value,raw.length,8).integer==1&&field(raw.value,raw.length,12).integer==3600);
    assert(field(raw.value,raw.length,99).integer==77&&field(raw.value,raw.length,100).length==3);
    assert(field(m.value,m.length,100).integer==42&&field(a.value,a.length,101).length==4);
    ack(local_node_number,telemetry_config_op.write_id,0);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING);
    // Neither unsolicited data nor a cached config is sufficient confirmation.
    receive(true,3600,true,0,local_node_number,local_node_number);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING);
    uint32_t id=readback();
    receive(true,3600,true,id+1,local_node_number,local_node_number);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING);
    receive(true,3600,true,id,0xaabbccdd,local_node_number);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING);
    receive(true,3600,true,id,local_node_number,MESH_BROADCAST_NODE);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING);
    const uint8_t malformed[]={0x42,4,0x32,2,8,0x80};
    parse_admin_response(malformed,sizeof(malformed),id);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING);
    const uint8_t wrong_module[]={0x42,2,0x0a,0};parse_admin_response(wrong_module,sizeof(wrong_module),id);
    assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING);
    uint8_t valid_m[160],bad_envelope[190];size_t vn=module(valid_m,true,3600,true),bn=0;
    append_bytes_field(bad_envelope,sizeof(bad_envelope),&bn,8,valid_m,vn);bad_envelope[bn++]=0;
    parse_admin_response(bad_envelope,bn,id);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING);
    receive(true,3600,true,id,local_node_number,local_node_number);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVED);
    assert(telemetry_config.revision>original_rev&&!mesh_radio_set_telemetry_config(&draft));

    // Expected radio reboot keeps verification pending; no write is replayed.
    ready();draft=telemetry_config;draft.enabled=true;assert(mesh_radio_set_telemetry_config(&draft));
    uint32_t write_id=telemetry_config_op.write_id;begin_radio_reconnect();assert(!telemetry_config.loaded);
    assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING&&!telemetry_config_op.read_id);
    status.synchronized=true;status.reconnecting=false;owner_config_received=true;lora_config.loaded=true;
    id=readback();assert(telemetry_config_op.write_id==write_id);
    receive(true,1800,true,id,local_node_number,local_node_number);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVED);

    ready();draft=telemetry_config;draft.enabled=true;assert(mesh_radio_set_telemetry_config(&draft));id=readback();
    receive(false,1800,false,id,local_node_number,local_node_number);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_FAILED);
    ready();draft=telemetry_config;draft.interval_secs=0;assert(mesh_radio_set_telemetry_config(&draft));id=readback();
    receive(false,1800,false,id,local_node_number,local_node_number);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVED);
    ready();draft=telemetry_config;assert(mesh_radio_set_telemetry_config(&draft));
    ack(local_node_number,telemetry_config_op.write_id,8);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING);
    ack(0x11111111,telemetry_config_op.write_id,33);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING);
    ack(local_node_number,telemetry_config_op.write_id,33);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_FAILED&&telemetry_save.error_reason==33);
    ready();draft=telemetry_config;assert(mesh_radio_set_telemetry_config(&draft));id=readback();
    ack(local_node_number,id,36);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_FAILED&&telemetry_save.error_reason==36);
    ready();draft=telemetry_config;assert(mesh_radio_set_telemetry_config(&draft));fake_time+=60000001;telemetry_config_poll(fake_time);
    assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_TIMEOUT);
    ready();draft=telemetry_config;assert(mesh_radio_set_telemetry_config(&draft));local_node_number++;telemetry_config_poll(fake_time);
    assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_INTERRUPTED);

    ready();draft=telemetry_config;short_write=true;assert(!mesh_radio_set_telemetry_config(&draft));
    assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_IDLE);short_write=false;
    ready();draft=telemetry_config;session_passkey_received_us=fake_time-240000001;assert(!mesh_radio_set_telemetry_config(&draft));
    ready();draft=telemetry_config;status.synchronized=false;assert(!mesh_radio_set_telemetry_config(&draft)&&!mesh_radio_request_telemetry_config());
    ready();draft=telemetry_config;draft.revision++;assert(!mesh_radio_set_telemetry_config(&draft));
    ready();draft=telemetry_config;assert(mesh_radio_set_telemetry_config(&draft));uint32_t first=readback(),once=telemetry_config_op.write_id;
    fake_time+=3000001;telemetry_config_poll(fake_time);assert(telemetry_config_op.read_id!=first&&telemetry_config_op.write_id==once);
    a=sent_admin();assert(field(a.value,a.length,7).integer==5&&!has(a.value,a.length,35));
    receive(false,1800,false,first,local_node_number,local_node_number);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING);
    receive(false,1800,false,telemetry_config_op.read_id,local_node_number,local_node_number);assert(telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVED);
    ready();draft=telemetry_config;location_save.state=MESH_LOCATION_SAVING;assert(!mesh_radio_set_telemetry_config(&draft));

    // Older firmware may edit only interval; never pretend it supports a switch.
    ready();strcpy(radio_metadata.firmware_version,"2.6.7");telemetry_enable_seen=false;
    assert(mesh_radio_get_telemetry_config(&draft)&&!draft.enable_supported);draft.enabled=true;assert(!mesh_radio_set_telemetry_config(&draft));
    draft.enabled=false;draft.interval_secs=7200;assert(mesh_radio_set_telemetry_config(&draft));
    a=sent_admin();m=field(a.value,a.length,35);raw=field(m.value,m.length,6);assert(!has(raw.value,raw.length,14));
    // An explicit tag establishes support even if firmware version is unknown.
    ready();radio_metadata.loaded=false;receive(true,1800,true,0,local_node_number,local_node_number);
    receive(false,1800,false,0,local_node_number,local_node_number);assert(mesh_radio_get_telemetry_config(&draft)&&draft.enable_supported);
    assert(!message_count&&!reaction_count&&!mesh_radio_take_message_alerts());
    puts("PASS: telemetry module wire tags, switch compatibility, unknown preservation, explicit writes, matched local readback, reboot recovery, no write replay, failures, cooldown, stale revision and no message effects");
}
