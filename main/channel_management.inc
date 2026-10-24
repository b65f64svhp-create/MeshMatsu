// Standard ChannelSet URL codec and single-write, full-dump verified mutations.
static bool channel_settings_info(const uint8_t *data, size_t size, mesh_channel_t *info) {
    if (!data || size > 160 || !info) return false;
    memset(info,0,sizeof(*info)); info->loaded=true;
    uint32_t seen=0; size_t offset=0;
    while(offset<size) {
        protobuf_field_t f;if(!read_field(data,size,&offset,&f))return false;
        if(f.field<=7) {if(seen&(1u<<f.field))return false;seen|=1u<<f.field;}
        if(f.field==2) {
            if(f.wire!=2 || (f.length!=0&&f.length!=1&&f.length!=16&&f.length!=32))return false;
            info->has_key=f.length && !(f.length==1&&!f.value[0]);
            info->default_key=f.length==1&&f.value[0];
            static const uint8_t default_prefix[]={0xd4,0xf1,0xbb,0x3a,0x20,0x29,0x07,0x59,0xf0,0xbc,0xff,0xab,0xcf,0x4e,0x69};
            if(f.length==16&&!memcmp(f.value,default_prefix,15))info->default_key=true;
        } else if(f.field==3) {
            if(f.wire!=2||f.length>=sizeof(info->name)||memchr(f.value,0,f.length))return false;
            for(size_t i=0;i<f.length;i++)if(f.value[i]<32||f.value[i]==127)return false;
            memcpy(info->name,f.value,f.length);
        } else if(f.field==5||f.field==6) {
            if(f.wire!=0||f.integer>1)return false;
            if(f.field==5)info->uplink_enabled=f.integer;else info->downlink_enabled=f.integer;
        } else if(f.field==1||f.field==4) {if(f.wire!=0||f.integer>UINT32_MAX)return false;}
        else if(f.field==7) {
            if(f.wire!=2)return false;
            size_t p=0;uint8_t nested=0;
            while(p<f.length){protobuf_field_t m;if(!read_field(f.value,f.length,&p,&m))return false;
                if(m.field<=2){if(nested&(1u<<m.field))return false;nested|=1u<<m.field;}
                if(m.field==1){if(m.wire!=0||m.integer>32)return false;info->has_position_precision=true;info->position_precision=m.integer;}
                else if(m.field==2&&(m.wire!=0||m.integer>1))return false;
            }
        }
    }
    return true;
}
static bool channel_extract_settings(const uint8_t *channel,size_t size,const uint8_t **out,size_t *length) {
    size_t p=0;bool found=false;
    while(p<size){protobuf_field_t f;if(!read_field(channel,size,&p,&f))return false;
        if(f.field==2){if(f.wire!=2||found)return false;*out=f.value;*length=f.length;found=true;}}
    return found;
}
bool mesh_radio_export_channel_settings(uint8_t index,uint8_t *out,size_t *size) {
    if(index>=8||!out||!size||!channels[index].info.loaded)return false;
    const uint8_t *settings=NULL;size_t n=0;
    if(!channel_extract_settings(channels[index].raw,channels[index].size,&settings,&n)||n>*size)return false;
    memcpy(out,settings,n);*size=n;return true;
}
static size_t channel_base64(const uint8_t *data,size_t size,char *out,size_t capacity) {
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    size_t n=0;uint32_t bits=0;unsigned count=0;
    for(size_t i=0;i<size;i++){bits=(bits<<8)|data[i];count+=8;while(count>=6){count-=6;if(n+1>=capacity)return 0;out[n++]=alphabet[(bits>>count)&63];}}
    if(count){if(n+1>=capacity)return 0;out[n++]=alphabet[(bits<<(6-count))&63];}out[n]=0;return n;
}
static bool channel_key_field(const uint8_t *settings,size_t size,const uint8_t **key,size_t *length) {
    size_t p=0;*key=NULL;*length=0;
    while(p<size){protobuf_field_t f;if(!read_field(settings,size,&p,&f))return false;if(f.field==2&&f.wire==2){*key=f.value;*length=f.length;}}
    return true;
}
static bool channel_url_prefix(const char *url,const char *prefix){
    for(size_t i=0;prefix[i];i++){unsigned char a=url[i],b=prefix[i];if(a>='A'&&a<='Z')a+=32;if(b>='A'&&b<='Z')b+=32;if(a!=b)return false;}
    return true;
}
bool mesh_radio_channel_url(uint8_t index,char *out,size_t capacity) {
    if(index>=8||!out||!capacity||!channels[index].info.role)return false;
    out[0]=0;uint8_t settings[160],set[384];size_t n=sizeof(settings),p=0;
    if(!mesh_radio_export_channel_settings(index,settings,&n))return false;
    const uint8_t *key=NULL;size_t key_size=0;if(!channel_key_field(settings,n,&key,&key_size))return false;
    // Empty PSKs on a secondary inherit the primary key. Materialize that key
    // when sharing so the invitation cannot silently inherit a different mesh.
    if(channels[index].info.role==2&&!key_size){
        uint8_t primary[160];size_t primary_size=sizeof(primary);bool found=false;
        for(unsigned i=0;i<8;i++)if(channels[i].info.loaded&&channels[i].info.role==1){found=mesh_radio_export_channel_settings(i,primary,&primary_size);break;}
        if(!found||!channel_key_field(primary,primary_size,&key,&key_size))return false;
        uint8_t merged[160];size_t used=0,q=0;
        while(q<n){size_t start=q;protobuf_field_t f;if(!read_field(settings,n,&q,&f))return false;if(f.field==2)continue;
            if(q-start>sizeof(merged)-used)return false;
            memcpy(merged+used,settings+start,q-start);used+=q-start;}
        static const uint8_t no_encryption=0;if(!key_size){key=&no_encryption;key_size=1;}
        if(!append_bytes_field(merged,sizeof(merged),&used,2,key,key_size))return false;
        memcpy(settings,merged,used);n=used;
    }
    if(!append_bytes_field(set,sizeof(set),&p,1,settings,n))return false;
    // Add-mode intentionally excludes LoRa: joining this secondary must not retune the receiver.
    const char prefix[]="https://meshtastic.org/e/?add=true#";size_t prefix_size=sizeof(prefix)-1;
    if(capacity<=prefix_size)return false;
    memcpy(out,prefix,prefix_size);
    if(!channel_base64(set,p,out+prefix_size,capacity-prefix_size)){out[0]=0;return false;}
    return true;
}
bool mesh_radio_parse_channel_url(const char *url,mesh_channel_share_t *out) {
    if(!url||!out||strlen(url)>=MESH_CHANNEL_URL_MAX)return false;
    const char *path=NULL;
    const char *prefixes[]={"https://meshtastic.org/e/","https://www.meshtastic.org/e/","https://meshtastic.org/e","https://www.meshtastic.org/e"};
    for(unsigned i=0;i<4;i++){size_t n=strlen(prefixes[i]);if(channel_url_prefix(url,prefixes[i])&&(url[n]=='#'||url[n]=='?')){path=url+n;break;}}
    if(!path)return false;
    const char *fragment=strchr(path,'#');if(!fragment)return false;fragment++;
    if(*fragment=='?')fragment++; // Legacy shares sometimes prefix the Base64 with '?'.
    const char *end=strchr(fragment,'?');if(!end)end=fragment+strlen(fragment);
    size_t length=end-fragment,padding=0;while(length&&fragment[length-1]=='='){length--;padding++;}
    if(!length||padding>2||length%4==1||(padding&&(length+padding)%4))return false;
    uint8_t data[1760];size_t n=0;uint32_t bits=0;unsigned count=0;
    for(size_t i=0;i<length;i++){int digit=base64_digit(fragment[i]);if(digit<0)return false;bits=(bits<<6)|(unsigned)digit;count+=6;
        if(count>=8){count-=8;if(n==sizeof(data))return false;data[n++]=(uint8_t)(bits>>count);}}
    if(count&&(bits&((1u<<count)-1)))return false;
    mesh_channel_share_t result={0};size_t p=0;
    while(p<n){protobuf_field_t f;if(!read_field(data,n,&p,&f))return false;
        if(f.field==1){mesh_channel_t info;if(f.wire!=2||result.count==8||!channel_settings_info(f.value,f.length,&info))return false;
            result.sizes[result.count]=f.length;memcpy(result.settings[result.count++],f.value,f.length);
        }else if(f.field==2){if(f.wire!=2||result.has_lora||f.length>192)return false;size_t q=0;
            while(q<f.length){protobuf_field_t l;if(!read_field(f.value,f.length,&q,&l))return false;}result.has_lora=true;}
    }
    if(!result.count)return false;
    *out=result;return true;
}
bool mesh_radio_share_channel_info(const mesh_channel_share_t *share,uint8_t entry,mesh_channel_t *out) {
    return share&&entry<share->count&&entry<8&&channel_settings_info(share->settings[entry],share->sizes[entry],out);
}
static bool channel_field_equal(const uint8_t *a,size_t an,const uint8_t *b,size_t bn,uint32_t tag,bool nested) {
    protobuf_field_t af={0},bf={0};size_t p=0;bool ah=false,bh=false;
    while(p<an){protobuf_field_t f;if(!read_field(a,an,&p,&f))return false;if(f.field==tag){af=f;ah=true;}}
    p=0;while(p<bn){protobuf_field_t f;if(!read_field(b,bn,&p,&f))return false;if(f.field==tag){bf=f;bh=true;}}
    if(nested){for(unsigned t=1;t<=2;t++)if(!channel_field_equal(af.value,af.length,bf.value,bf.length,t,false))return false;return true;}
    if(!ah&&!bh)return true;
    if((ah?af.wire:bf.wire)==0)return (!ah||af.wire==0)&&(!bh||bf.wire==0)&&af.integer==bf.integer;
    if(ah&&bh&&af.wire!=bf.wire)return false;
    return af.length==bf.length&&(!af.length||!memcmp(af.value,bf.value,af.length));
}
static bool channel_settings_equal(const uint8_t *a,size_t an,const uint8_t *b,size_t bn) {
    for(unsigned t=1;t<=7;t++)if(!channel_field_equal(a,an,b,bn,t,t==7))return false;
    return true;
}
mesh_channel_operation_t mesh_radio_channel_operation(void){return channel_operation;}
static bool channel_management_ready(void) {
    return status.started&&status.synchronized&&!status.reconnecting&&!config_request_pending&&mesh_radio_admin_ready()&&
        esp_timer_get_time()-session_passkey_received_us<240000000&&channel_operation.state!=MESH_FAVORITE_SAVING&&
        node_control.state!=MESH_FAVORITE_SAVING&&favorite_status.state!=MESH_FAVORITE_SAVING&&role_save.state!=MESH_ROLE_SAVING&&
        location_save.state!=MESH_LOCATION_SAVING&&telemetry_save.state!=MESH_TELEMETRY_CONFIG_SAVING&&
        !owner_verify_at_us&&!lora_verify_at_us&&channel_verify_index<0;
}
bool mesh_radio_manage_channel(uint8_t index,uint8_t role,const uint8_t *settings,size_t size,uint32_t revision) {
    mesh_channel_t info;
    if(index>=8||role>2||!channel_settings_info(settings,size,&info)||!channel_management_ready()||
        !channels[index].info.loaded||channels[index].info.revision!=revision)return false;
    unsigned primaries=0;
    for(unsigned i=0;i<8;i++){
        if(!channels[i].info.loaded||channels[i].info.role>2)return false;
        uint8_t proposed=i==index?role:role==1&&channels[i].info.role==1?2:channels[i].info.role;
        if(proposed==1)primaries++;
    }
    if(primaries!=1)return false;
    // Keep enabled slots consecutive; never leave a radio without its primary.
    if(role){for(unsigned i=0;i<index;i++)if(!channels[i].info.role)return false;}
    else {if(channels[index].info.role==1)return false;for(unsigned i=index+1;i<8;i++)if(channels[i].info.role)return false;}
    if(channels[index].info.role==1&&role!=1)return false; // Promote another channel first (radio demotes this one).
    if(!info.has_key||info.default_key)if(info.has_position_precision&&info.position_precision>15)return false;
    uint8_t channel[192],admin[240];size_t n=0,p=0;
    if(!append_varint(channel,sizeof(channel),&n,8)||!append_varint(channel,sizeof(channel),&n,index)||
        !append_bytes_field(channel,sizeof(channel),&n,2,settings,size)||!append_varint(channel,sizeof(channel),&n,24)||
        !append_varint(channel,sizeof(channel),&n,role)||!append_bytes_field(admin,sizeof(admin),&p,33,channel,n)||
        !append_bytes_field(admin,sizeof(admin),&p,101,session_passkey,session_passkey_size))return false;
    uint32_t id;if(!send_admin_payload_with_id(admin,p,&id))return false;
    channel_operation=(mesh_channel_operation_t){.state=MESH_FAVORITE_SAVING,.index=index};
    memset(&channel_operation_data,0,sizeof(channel_operation_data));channel_operation_data.origin=local_node_number;
    channel_operation_data.write_id=id;channel_operation_data.started=esp_timer_get_time();channel_operation_data.read_at=channel_operation_data.started+1500000;
    memcpy(channel_operation_data.settings,settings,size);channel_operation_data.size=size;
    channel_operation_data.required=1u<<index;channel_operation_data.matched=true;
    for(unsigned i=0;i<8;i++){
        channel_operation_data.roles[i]=channels[i].info.role;
        if(role==1&&i!=index&&channels[i].info.role==1){channel_operation_data.roles[i]=2;channel_operation_data.required|=1u<<i;}}
    channel_operation_data.roles[index]=role;history_revision++;return true;
}
static void channel_operation_observe(uint8_t index) {
    if(channel_operation.state!=MESH_FAVORITE_SAVING||!location_in_dump||!channel_operation_data.nonce||
        channel_operation_data.nonce!=config_nonce||!config_request_pending)return;
    if(!(channel_operation_data.required&(1u<<index)))return;
    channel_operation_data.seen|=1u<<index;
    if(channels[index].info.role!=channel_operation_data.roles[index])channel_operation_data.matched=false;
    if(index==channel_operation.index){const uint8_t *raw=NULL;size_t n=0;
        if(!channel_extract_settings(channels[index].raw,channels[index].size,&raw,&n)||
            !channel_settings_equal(raw,n,channel_operation_data.settings,channel_operation_data.size))channel_operation_data.matched=false;}
}
static void channel_operation_complete(uint32_t nonce) {
    if(channel_operation.state!=MESH_FAVORITE_SAVING||!channel_operation_data.nonce||nonce!=channel_operation_data.nonce)return;
    channel_operation.state=channel_operation_data.origin==local_node_number&&channel_operation_data.matched&&
        channel_operation_data.seen==channel_operation_data.required?MESH_FAVORITE_SAVED:MESH_FAVORITE_FAILED;
    history_revision++;
}
static void channel_operation_poll(int64_t now) {
    if(channel_operation.state!=MESH_FAVORITE_SAVING)return;
    if(local_node_number&&local_node_number!=channel_operation_data.origin){channel_operation.state=MESH_FAVORITE_INTERRUPTED;history_revision++;}
    else if(now-channel_operation_data.started>=60000000){channel_operation.state=MESH_FAVORITE_TIMEOUT;history_revision++;}
    else if(now>=channel_operation_data.read_at&&status.synchronized&&!status.reconnecting&&!config_request_pending){
        mesh_radio_request_config();channel_operation_data.read_at=now+5000000;}
}
bool mesh_radio_generate_channel_key(char *out,size_t capacity) {
    if(!out||capacity<44)return false;
    uint8_t key[32];for(unsigned i=0;i<8;i++){uint32_t value=esp_random();memcpy(key+i*4,&value,4);}
    bool ok=channel_base64(key,sizeof(key),out,capacity)>0;memset(key,0,sizeof(key));return ok;
}
bool mesh_radio_replace_channel_key(uint8_t index,const char *key_text,uint32_t revision) {
    uint8_t key[32],settings[160],old[160];size_t kn=0,on=sizeof(old),n=0,p=0;
    if(!decode_channel_key(key_text,key,&kn)||!mesh_radio_export_channel_settings(index,old,&on)){memset(key,0,sizeof(key));return false;}
    while(p<on){size_t begin=p;protobuf_field_t f;if(!read_field(old,on,&p,&f)){memset(key,0,sizeof(key));return false;}
        if(f.field!=2){if(p-begin>sizeof(settings)-n){memset(key,0,sizeof(key));return false;}memcpy(settings+n,old+begin,p-begin);n+=p-begin;}}
    bool ok=append_bytes_field(settings,sizeof(settings),&n,2,key,kn);memset(key,0,sizeof(key));
    return ok&&mesh_radio_manage_channel(index,channels[index].info.role,settings,n,revision);
}
bool mesh_radio_set_channel_role(uint8_t index,uint8_t role,bool erase,uint32_t revision) {
    uint8_t settings[160];size_t n=sizeof(settings);
    if(index>=8||!mesh_radio_export_channel_settings(index,settings,&n))return false;
    if(erase){if(role)return false;n=0;}
    return mesh_radio_manage_channel(index,role,settings,n,revision);
}
