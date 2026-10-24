#define main previous_tests
#include "test_direct_messages.c"
#undef main
static uint8_t original[160];static size_t original_size;
static void snapshot(uint8_t index,uint8_t role,const uint8_t *settings,size_t size,bool dump){
    uint8_t ch[192],frame[208];size_t n=0,f=0;
    assert(append_varint(ch,sizeof(ch),&n,8)&&append_varint(ch,sizeof(ch),&n,index));
    assert(append_bytes_field(ch,sizeof(ch),&n,2,settings,size));assert(append_varint(ch,sizeof(ch),&n,24)&&append_varint(ch,sizeof(ch),&n,role));
    if(dump){assert(append_bytes_field(frame,sizeof(frame),&f,10,ch,n));decode_from_radio(frame,f);}else decode_channel(ch,n);
}
static void ready(void){reset();fake_time+=1000000;session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;
    original_size=0;const uint8_t key[]={1};
    assert(append_bytes_field(original,sizeof(original),&original_size,2,key,1));assert(append_bytes_field(original,sizeof(original),&original_size,3,(const uint8_t*)"BUL",3));
    for(unsigned i=0;i<8;i++)snapshot(i,i==0?1:i==1?2:0,original,original_size,false);
}
static void complete(uint32_t nonce){uint8_t data[16]={0x38};size_t n=1;assert(append_varint(data,sizeof(data),&n,nonce));decode_from_radio(data,n);}
static void verify(void){fake_time+=1500001;mesh_radio_poll();assert(config_request_pending&&channel_operation_data.nonce==config_nonce&&channel_operation.state==MESH_FAVORITE_SAVING);}
static protobuf_field_t field(const uint8_t *data,size_t n,unsigned tag){size_t p=0;protobuf_field_t f;while(p<n){assert(read_field(data,n,&p,&f));if(f.field==tag)return f;}assert(!"Missing field");return (protobuf_field_t){0};}
int main(void){
    ready();char url[MESH_CHANNEL_URL_MAX];size_t writes=write_count;assert(mesh_radio_channel_url(1,url,sizeof(url))&&strstr(url,"?add=true#"));assert(write_count==writes);
    mesh_channel_share_t share;assert(mesh_radio_parse_channel_url(url,&share)&&share.count==1&&!share.has_lora&&share.sizes[0]==original_size&&!memcmp(share.settings[0],original,original_size));
    mesh_channel_t info;assert(mesh_radio_share_channel_info(&share,0,&info)&&!strcmp(info.name,"BUL")&&info.default_key);
    assert(!mesh_radio_parse_channel_url("https://meshtastic.org.evil/e/#AQ",&share));assert(!mesh_radio_parse_channel_url("http://meshtastic.org/e/#AQ",&share));
    assert(!mesh_radio_parse_channel_url("https://meshtastic.org/e/#A",&share));assert(!mesh_radio_parse_channel_url("https://meshtastic.org/e/#a?",&share));
    uint8_t set[100];size_t n=0;assert(append_bytes_field(set,sizeof(set),&n,1,original,original_size)&&append_bytes_field(set,sizeof(set),&n,1,original,original_size));uint8_t lora[]={8,1};assert(append_bytes_field(set,sizeof(set),&n,2,lora,sizeof(lora)));
    char encoded[200];assert(channel_base64(set,n,encoded,sizeof(encoded)));snprintf(url,sizeof(url),"https://meshtastic.org/e/#%s?add=true",encoded);assert(mesh_radio_parse_channel_url(url,&share)&&share.count==2&&share.has_lora);
    assert(!mesh_radio_share_channel_info(&share,2,&info));
    // 256-bit generated keys and replace preserve all non-key settings.
    char generated[67];assert(mesh_radio_generate_channel_key(generated,sizeof(generated))&&strlen(generated)==43&&mesh_radio_channel_key_valid(generated));
    uint32_t revision=channels[1].info.revision;assert(mesh_radio_replace_channel_key(1,generated,revision));assert(channel_operation.state==MESH_FAVORITE_SAVING&&channels[1].info.default_key);
    protobuf_field_t packet=field(sent_bytes+4,((size_t)sent_bytes[2]<<8)|sent_bytes[3],1),to=field(packet.value,packet.length,2);uint32_t destination;memcpy(&destination,to.value,4);assert(destination==local_node_number);
    protobuf_field_t data=field(packet.value,packet.length,4),admin=field(data.value,data.length,2),change=field(admin.value,admin.length,33),settings=field(change.value,change.length,2);
    assert(field(admin.value,admin.length,101).length==4&&field(settings.value,settings.length,2).length==32&&field(settings.value,settings.length,3).length==3);
    uint8_t expected[160];size_t expected_size=channel_operation_data.size;memcpy(expected,channel_operation_data.settings,expected_size);
    writes=write_count;assert(!mesh_radio_replace_channel_key(1,generated,revision)&&write_count==writes);ack(local_node_number,channel_operation_data.write_id,0);assert(channel_operation.state==MESH_FAVORITE_SAVING);
    snapshot(1,2,expected,expected_size,false);assert(channel_operation.state==MESH_FAVORITE_SAVING);verify();complete(config_nonce-1);assert(channel_operation.state==MESH_FAVORITE_SAVING);
    snapshot(1,2,expected,expected_size,true);complete(config_nonce);assert(channel_operation.state==MESH_FAVORITE_SAVED&&!channels[1].info.default_key);
    // A stale revision, gaps and removal of a non-last or primary channel are rejected.
    writes=write_count;assert(!mesh_radio_set_channel_role(1,0,false,revision));assert(!mesh_radio_set_channel_role(0,0,true,channels[0].info.revision));assert(!mesh_radio_manage_channel(4,2,original,original_size,channels[4].info.revision)&&write_count==writes);
    assert(mesh_radio_set_channel_role(1,0,false,channels[1].info.revision));verify();snapshot(1,0,expected,expected_size,true);complete(config_nonce);assert(channel_operation.state==MESH_FAVORITE_SAVED);
    // Re-enable keeps the key. Promotion must also verify old-primary demotion.
    assert(mesh_radio_set_channel_role(1,2,false,channels[1].info.revision));verify();snapshot(1,2,expected,expected_size,true);complete(config_nonce);assert(channel_operation.state==MESH_FAVORITE_SAVED);
    assert(mesh_radio_set_channel_role(1,1,false,channels[1].info.revision));verify();snapshot(1,1,expected,expected_size,true);complete(config_nonce);assert(channel_operation.state==MESH_FAVORITE_FAILED); // old primary not observed
    ready();assert(mesh_radio_set_channel_role(1,1,false,channels[1].info.revision));verify();snapshot(1,1,original,original_size,true);snapshot(0,2,original,original_size,true);complete(config_nonce);assert(channel_operation.state==MESH_FAVORITE_SAVED);
    ready();assert(mesh_radio_set_channel_role(1,0,true,channels[1].info.revision));verify();snapshot(1,0,NULL,0,true);complete(config_nonce);assert(channel_operation.state==MESH_FAVORITE_SAVED);size_t length=sizeof(expected);assert(mesh_radio_export_channel_settings(1,expected,&length)&&length==0);
    ready();assert(mesh_radio_set_channel_role(1,0,false,channels[1].info.revision));ack(local_node_number,channel_operation_data.write_id,36);assert(channel_operation.state==MESH_FAVORITE_FAILED&&channel_operation.error_reason==36);
    ready();assert(mesh_radio_set_channel_role(1,0,false,channels[1].info.revision));begin_radio_reconnect();assert(channel_operation.state==MESH_FAVORITE_SAVING);status.synchronized=true;status.reconnecting=false;verify();snapshot(1,0,original,original_size,true);complete(config_nonce);assert(channel_operation.state==MESH_FAVORITE_SAVED); // reads only, no replay
    ready();assert(mesh_radio_set_channel_role(1,0,false,channels[1].info.revision));fake_time+=60000001;mesh_radio_poll();assert(channel_operation.state==MESH_FAVORITE_TIMEOUT);
    ready();session_passkey_received_us=fake_time-240000001;writes=write_count;assert(!mesh_radio_set_channel_role(1,0,false,channels[1].info.revision)&&write_count==writes);
    // Sharing a secondary that inherits its key resolves the primary key first.
    ready();const uint8_t inherited[]={0x1a,3,'B','U','L'};snapshot(1,2,inherited,sizeof(inherited),false);
    assert(mesh_radio_channel_url(1,url,sizeof(url))&&mesh_radio_parse_channel_url(url,&share));
    assert(mesh_radio_share_channel_info(&share,0,&info)&&info.default_key);
    assert(channel_field_equal(share.settings[0],share.sizes[0],original,original_size,2,false));
    char upper[MESH_CHANNEL_URL_MAX];strcpy(upper,url);for(char *c=upper;c<strchr(upper,'#');c++)if(*c>='a'&&*c<='z')*c-=32;
    assert(mesh_radio_parse_channel_url(upper,&share));
    // Malformed/truncated input stays bounded and never partially replaces the caller's result.
    unsigned seed=17;for(unsigned i=0;i<1500;i++){
        uint8_t bytes[192];size_t count=i%sizeof(bytes);for(size_t j=0;j<count;j++){seed=seed*1664525u+1013904223u;bytes[j]=seed>>24;}
        char payload[300];channel_base64(bytes,count,payload,sizeof(payload));snprintf(url,sizeof(url),"https://meshtastic.org/e/#%s",payload);
        mesh_radio_parse_channel_url(url,&share);
    }
    puts("PASS: standard add/legacy/multi channel links, key generation and exact local admin encoding, preservation, one-write nonce readback, primary demotion, contiguous slots, disable/erase/re-enable, stale/NAK/reconnect/timeout/session guards");
}
