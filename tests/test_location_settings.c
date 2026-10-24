#define main message_tests
#include "test_direct_messages.c"
#undef main
static protobuf_field_t field(const uint8_t *data,size_t size,uint32_t tag){
 size_t n=0;protobuf_field_t f;while(n<size){assert(read_field(data,size,&n,&f));if(f.field==tag)return f;}assert(false);return (protobuf_field_t){0};
}
static void config(uint32_t role){
 uint8_t raw[]={8,0,0x20,7,0x5a,4,'U','T','C','0',0xa0,6,123},cfg[80];size_t n=0;raw[1]=role;
 assert(append_bytes_field(cfg,sizeof(cfg),&n,1,raw,sizeof(raw)));decode_config(cfg,n);
}
static void ready(void){
 reset();lora_config.loaded=true;session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;
 config(0);radio_metadata.loaded=true;strcpy(radio_metadata.firmware_version,"2.7.18");
}
static void reply(uint32_t role,uint32_t id,uint32_t sender){
 uint8_t device[]={8,role},cfg[40],admin[60],data[90];size_t c=0,a=0,d=0;
 assert(append_bytes_field(cfg,sizeof(cfg),&c,1,device,sizeof(device)));
 assert(append_bytes_field(admin,sizeof(admin),&a,6,cfg,c));
 assert(append_varint(data,sizeof(data),&d,8)&&append_varint(data,sizeof(data),&d,6));
 assert(append_bytes_field(data,sizeof(data),&d,2,admin,a));data[d++]=0x35;memcpy(data+d,&id,4);d+=4;
 decode_data_message(data,d,sender,0,local_node_number,400,0);
}
static const uint8_t raw_position[]={8,0,16,1,24,0,40,120,56,7,104,2,0x98,6,77};
static void location_ready(void) {
    ready();decode_position_config(raw_position,sizeof(raw_position));
}
static void start_read(void) {fake_time+=1600000;mesh_radio_poll();assert(location_op.nonce&&config_request_pending);}
static void complete(uint32_t nonce) {uint8_t r[16];size_t n=0;append_varint(r,sizeof(r),&n,56);append_varint(r,sizeof(r),&n,nonce);decode_from_radio(r,n);}
static void position_reply(bool fixed,bool smart,unsigned interval,bool node,int32_t lat,int32_t lon) {
    uint8_t p[80],cfg[100],radio[180];size_t n=0,c=0,r=0;
    append_varint(p,sizeof(p),&n,8);append_varint(p,sizeof(p),&n,interval);
    append_varint(p,sizeof(p),&n,16);append_varint(p,sizeof(p),&n,smart);
    append_varint(p,sizeof(p),&n,24);append_varint(p,sizeof(p),&n,fixed);
    append_bytes_field(cfg,sizeof(cfg),&c,2,p,n);append_bytes_field(radio,sizeof(radio),&r,5,cfg,c);
    if(node) {
        uint8_t pos[10],ni[40];size_t z=0;pos[0]=0x0d;memcpy(pos+1,&lat,4);pos[5]=0x15;memcpy(pos+6,&lon,4);
        append_varint(ni,sizeof(ni),&z,8);append_varint(ni,sizeof(ni),&z,local_node_number);
        append_bytes_field(ni,sizeof(ni),&z,3,pos,sizeof(pos));append_bytes_field(radio,sizeof(radio),&r,4,ni,z);
    }
    decode_from_radio(radio,r);
}
static protobuf_field_t sent_admin(void) {
    protobuf_field_t p=field(sent_bytes+4,sent_size-4,1),d=field(p.value,p.length,4);
    assert(field(d.value,d.length,1).integer==6);
    return field(d.value,d.length,2);
}
static void channel_fixture(bool public_key,bool module) {
    uint8_t settings[160],channel[192],key[16];memset(key,0xaa,sizeof(key));size_t n=0,c=0;
    append_bytes_field(settings,sizeof(settings),&n,2,key,public_key?1:16);
    append_bytes_field(settings,sizeof(settings),&n,3,(const uint8_t*)"Private",7);
    const uint8_t m[]={8,13,16,1,0x98,6,99};
    if(module)append_bytes_field(settings,sizeof(settings),&n,7,m,sizeof(m));
    append_varint(settings,sizeof(settings),&n,0x320);append_varint(settings,sizeof(settings),&n,77);
    append_varint(channel,sizeof(channel),&c,8);append_varint(channel,sizeof(channel),&c,1);
    append_bytes_field(channel,sizeof(channel),&c,2,settings,n);
    append_varint(channel,sizeof(channel),&c,24);append_varint(channel,sizeof(channel),&c,2);decode_channel(channel,c);
}
int main(void) {
    location_ready();mesh_position_config_t draft=position_config;draft.broadcast_secs=1800;draft.smart=false;
    assert(mesh_radio_set_position_config(&draft));assert(!mesh_radio_set_position_config(&draft));
    assert(position_config.broadcast_secs==0&&location_save.state==MESH_LOCATION_SAVING);
    protobuf_field_t a=sent_admin(),cfg=field(a.value,a.length,34),p=field(cfg.value,cfg.length,2);
    assert(field(p.value,p.length,1).integer==1800&&!field(p.value,p.length,2).integer);
    assert(field(p.value,p.length,5).integer==120&&field(p.value,p.length,7).integer==7&&field(p.value,p.length,13).integer==2&&field(p.value,p.length,99).integer==77);
    ack(local_node_number,location_op.write_id,0);assert(location_save.state==MESH_LOCATION_SAVING);
    start_read();uint32_t nonce=location_op.nonce;
    position_reply(false,false,1800,false,0,0);complete(nonce+1);assert(location_save.state==MESH_LOCATION_SAVING);
    complete(nonce);assert(location_save.state==MESH_LOCATION_SAVED&&!message_count);
    // A cached matching config, unrelated packets, or a missing config cannot verify a save.
    location_ready();draft=position_config;assert(mesh_radio_set_position_config(&draft));start_read();complete(location_op.nonce);
    assert(location_save.state==MESH_LOCATION_FAILED);
    location_ready();draft=position_config;draft.revision++;assert(!mesh_radio_set_position_config(&draft));
    draft=position_config;draft.fixed=true;assert(!mesh_radio_set_position_config(&draft)); // no own fix
    // Fixed command uses signed optional sfixed32 coordinates; zero is valid.
    location_ready();assert(!mesh_radio_set_fixed_position(900000001,0,position_config.revision));
    assert(!mesh_radio_set_fixed_position(0,-1800000001,position_config.revision));
    assert(mesh_radio_set_fixed_position(0,-1188000000,position_config.revision));a=sent_admin();p=field(a.value,a.length,41);
    int32_t lon;protobuf_field_t f=field(p.value,p.length,2);memcpy(&lon,f.value,4);assert(lon==-1188000000&&field(p.value,p.length,1).wire==5);
    start_read();position_reply(true,true,0,true,0,-1188000000);complete(location_op.nonce);assert(location_save.state==MESH_LOCATION_SAVED);
    location_ready();assert(mesh_radio_set_fixed_position(0,0,position_config.revision));start_read();position_reply(true,true,0,false,0,0);complete(location_op.nonce);
    assert(location_save.state==MESH_LOCATION_FAILED); // Old cached coordinates never count.
    location_ready();assert(mesh_radio_set_fixed_position(0,0,position_config.revision));begin_radio_reconnect();
    fake_time+=2000000;mesh_radio_request_config();position_reply(true,true,0,true,0,0);complete(location_op.nonce);
    assert(location_save.state==MESH_LOCATION_SAVED); // Reboot recovery reads; no mutation is resent.
    location_ready();assert(mesh_radio_set_fixed_position(0,0,position_config.revision));ack(local_node_number,location_op.write_id,33);assert(location_save.state==MESH_LOCATION_FAILED);
    location_ready();short_write=true;assert(!mesh_radio_set_fixed_position(0,0,position_config.revision));assert(location_save.state==MESH_LOCATION_IDLE);short_write=false;
    location_ready();session_passkey_received_us=fake_time-240000001;assert(!mesh_radio_set_fixed_position(0,0,position_config.revision));
    location_ready();assert(mesh_radio_set_fixed_position(0,0,position_config.revision));fake_time+=60000001;mesh_radio_poll();assert(location_save.state==MESH_LOCATION_TIMEOUT);
    // Nested channel module edits retain key, name, mute and unknown fields.
    location_ready();channel_fixture(false,true);uint32_t revision=channels[1].info.revision;
    assert(!mesh_radio_set_channel_location(1,"Private",false,false,18,revision+1));
    assert(mesh_radio_set_channel_location(1,"Private",false,false,18,revision));a=sent_admin();protobuf_field_t ch=field(a.value,a.length,33),cs=field(ch.value,ch.length,2),m=field(cs.value,cs.length,7);
    assert(field(m.value,m.length,1).integer==18&&field(m.value,m.length,2).integer==1&&field(m.value,m.length,99).integer==99);
    assert(field(cs.value,cs.length,2).length==16&&field(cs.value,cs.length,100).integer==77);
    uint8_t radio[256];size_t r=0;append_bytes_field(radio,sizeof(radio),&r,10,ch.value,ch.length);
    start_read();decode_from_radio(radio,r);complete(location_op.nonce);assert(location_save.state==MESH_LOCATION_SAVED);
    location_ready();channel_fixture(true,false);assert(!channels[1].info.has_position_precision);
    assert(!mesh_radio_set_channel_location(1,"Private",false,false,32,channels[1].info.revision));
    assert(mesh_radio_set_channel_location(1,"Private",false,false,0,channels[1].info.revision));a=sent_admin();ch=field(a.value,a.length,33);cs=field(ch.value,ch.length,2);m=field(cs.value,cs.length,7);assert(field(m.value,m.length,1).integer==0);
    location_ready();channel_fixture(false,false);assert(mesh_radio_set_channel(1,"Private",false,false));a=sent_admin();ch=field(a.value,a.length,33);cs=field(ch.value,ch.length,2);
    size_t off=0;while(off<cs.length){assert(read_field(cs.value,cs.length,&off,&f));assert(f.field!=7);} // Keep absence unchanged.
    assert(!message_count&&!reaction_count);
    puts("PASS: location encoding/preservation, coordinates, stale revisions, nonce verification, no optimistic success, reboot, NAK, timeout, channel privacy and nested unknown fields");
}
