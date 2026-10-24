// Local-radio-only key snapshots and one-write restoration. Full nonce-matched
// dumps, never cached data or routing ACKs, establish readiness/success.
static bool keys_parse_security(const uint8_t *data,size_t size) {
    if(size>sizeof(keys_raw))return false;
    uint8_t pub[32]={0},priv[32]={0};size_t pn=0,sn=0,offset=0;uint32_t seen=0;unsigned admins=0;
    bool ok=true;
    while(offset<size){protobuf_field_t f;if(!read_field(data,size,&offset,&f)){ok=false;break;}
        if(f.field<=7&&f.field!=3){if(seen&(1u<<f.field)){ok=false;break;}seen|=1u<<f.field;}
        if(f.field==1||f.field==2){if(f.wire!=2||(f.length!=0&&f.length!=32)){ok=false;break;}
            if(f.field==1){pn=f.length;memcpy(pub,f.value,pn);}else{sn=f.length;memcpy(priv,f.value,sn);}}
        else if(f.field==3){if(f.wire!=2||f.length!=32||++admins>3){ok=false;break;}}
        else if(f.field>=4&&f.field<=7){if(f.wire!=0||f.integer>1){ok=false;break;}}
    }
    if(ok){memcpy(keys_raw,data,size);keys_raw_size=size;keys_have_identity=pn==32&&sn==32;
        memcpy(keys_public,pub,32);memcpy(keys_private,priv,32);keys_security_seen=true;}
    mesh_keys_wipe(priv,sizeof(priv));return ok;
}
static void keys_config(const uint8_t *data,size_t size) {
    if(!location_in_dump||!config_request_pending||config_nonce!=keys_nonce||
        (keys_state!=MESH_KEYS_READING&&keys_state!=MESH_KEYS_SAVING))return;
    if(!keys_parse_security(data,size))keys_security_seen=false;
}
static void keys_complete(uint32_t nonce) {
    if(!keys_nonce||nonce!=keys_nonce||nonce!=config_nonce||keys_origin!=local_node_number||
        (keys_state!=MESH_KEYS_READING&&keys_state!=MESH_KEYS_SAVING))return;
    if(!keys_security_seen||keys_channels_seen!=255){keys_state=MESH_KEYS_FAILED;mesh_radio_keys_clear();history_revision++;return;}
    keys_loaded=true;keys_loaded_us=esp_timer_get_time();keys_revision++;
    if(keys_state==MESH_KEYS_SAVING){
        bool match=keys_have_identity&&!memcmp(keys_public,keys_desired_public,32)&&!memcmp(keys_private,keys_desired_private,32);
        keys_state=match?MESH_KEYS_SAVED:MESH_KEYS_FAILED;
        if(match)owner_user_size=0;
        mesh_keys_wipe(keys_desired_private,sizeof(keys_desired_private));mesh_radio_keys_clear();
    }else keys_state=MESH_KEYS_READY;
    history_revision++;
}
void mesh_radio_keys_clear(void) {
    keys_loaded=false;keys_have_identity=false;keys_raw_size=0;
    mesh_keys_wipe(keys_raw,sizeof(keys_raw));mesh_keys_wipe(keys_private,sizeof(keys_private));memset(keys_public,0,sizeof(keys_public));
    if(keys_state==MESH_KEYS_READING||keys_state==MESH_KEYS_READY){keys_state=MESH_KEYS_IDLE;keys_nonce=0;}
}
mesh_radio_keys_state_t mesh_radio_keys_state(void){return keys_state;}
uint32_t mesh_radio_keys_revision(void){return keys_revision;}
bool mesh_radio_keys_request(void) {
    if(!channel_management_ready()||keys_state==MESH_KEYS_READING||keys_state==MESH_KEYS_SAVING)return false;
    mesh_radio_keys_clear();keys_state=MESH_KEYS_READING;keys_origin=local_node_number;keys_started=esp_timer_get_time();
    mesh_radio_request_config();return true;
}
bool mesh_radio_keys_snapshot(mesh_key_backup_t *out,bool identity,bool want_channels) {
    if(!out||(!identity&&!want_channels)||!keys_loaded||keys_origin!=local_node_number||keys_state!=MESH_KEYS_READY||
        !status.synchronized||status.reconnecting||esp_timer_get_time()-keys_loaded_us>=120000000||
        (identity&&!keys_have_identity))return false;
    mesh_key_backup_t b={.node=local_node_number,.timestamp=current_message_timestamp(),.identity=identity};
    snprintf(b.name,sizeof(b.name),"%s",local_long_name);
    if(identity){memcpy(b.public_key,keys_public,32);memcpy(b.private_key,keys_private,32);}
    if(want_channels)for(unsigned i=0;i<8;i++){
        if(!channels[i].info.loaded)goto fail;
        if(!channels[i].info.role)continue;
        uint8_t settings[160];size_t n=sizeof(settings),length=0;const uint8_t *key=NULL;
        if(!mesh_radio_export_channel_settings(i,settings,&n)||!channel_key_field(settings,n,&key,&length))goto fail;
        uint8_t primary[160];size_t primary_size=sizeof(primary);
        if(channels[i].info.role==2&&!length){bool found=false;
            for(unsigned j=0;j<8;j++)if(channels[j].info.role==1){found=mesh_radio_export_channel_settings(j,primary,&primary_size);break;}
            if(!found||!channel_key_field(primary,primary_size,&key,&length))goto fail;
        }
        if(length!=0&&length!=1&&length!=16&&length!=32)goto fail;
        mesh_backup_channel_t *c=&b.channel[i];c->role=channels[i].info.role;snprintf(c->name,sizeof(c->name),"%s",channels[i].info.name);
        if(!length){c->length=1;c->key[0]=0;}else{c->length=length;memcpy(c->key,key,length);}
        b.channel_mask|=1u<<i;mesh_keys_wipe(settings,sizeof(settings));mesh_keys_wipe(primary,sizeof(primary));
    }
    if(!identity&&!b.channel_mask)goto fail;
    *out=b;mesh_keys_wipe(&b,sizeof(b));return true;
fail:
    mesh_keys_wipe(&b,sizeof(b));return false;
}
bool mesh_radio_keys_restore_identity(const uint8_t pub[32],const uint8_t priv[32],uint32_t revision) {
    if(!pub||!priv||!channel_management_ready()||!keys_loaded||keys_state!=MESH_KEYS_READY||keys_origin!=local_node_number||
        revision!=keys_revision||esp_timer_get_time()-keys_loaded_us>=120000000)return false;
    unsigned nonzero=0;for(unsigned i=0;i<32;i++)nonzero|=priv[i];if(!nonzero)return false;
    uint8_t security[224],config[240],admin[264];size_t n=0,p=0,a=0,q=0;bool ok=true;
    while(q<keys_raw_size){size_t start=q;protobuf_field_t f;if(!read_field(keys_raw,keys_raw_size,&q,&f)){ok=false;break;}
        if(f.field==1||f.field==2)continue;
        if(q-start>sizeof(security)-n){ok=false;break;}memcpy(security+n,keys_raw+start,q-start);n+=q-start;}
    if(ok)ok=append_bytes_field(security,sizeof(security),&n,1,pub,32)&&append_bytes_field(security,sizeof(security),&n,2,priv,32)&&
        append_bytes_field(config,sizeof(config),&p,8,security,n)&&append_bytes_field(admin,sizeof(admin),&a,34,config,p)&&
        append_bytes_field(admin,sizeof(admin),&a,101,session_passkey,session_passkey_size)&&send_admin_payload_with_id(admin,a,&keys_write_id);
    mesh_keys_wipe(security,sizeof(security));mesh_keys_wipe(config,sizeof(config));mesh_keys_wipe(admin,sizeof(admin));
    if(!ok)return false;
    keys_state=MESH_KEYS_SAVING;keys_started=esp_timer_get_time();keys_read_at=keys_started+1500000;keys_nonce=0;
    memcpy(keys_desired_public,pub,32);memcpy(keys_desired_private,priv,32);mesh_radio_keys_clear();history_revision++;return true;
}
bool mesh_radio_keys_restore_channel(uint8_t index,const mesh_backup_channel_t *saved,uint32_t revision) {
    if(index>=8||!saved||!channels[index].info.loaded||!channels[index].info.role||
        (saved->length!=0&&saved->length!=1&&saved->length!=16&&saved->length!=32))return false;
    uint8_t current[160],merged[160];size_t cn=sizeof(current),p=0,n=0;bool ok=mesh_radio_export_channel_settings(index,current,&cn);
    while(ok&&p<cn){size_t start=p;protobuf_field_t f;if(!read_field(current,cn,&p,&f)){ok=false;break;}
        if(f.field==2)continue;
        if(p-start>sizeof(merged)-n){ok=false;break;}memcpy(merged+n,current+start,p-start);n+=p-start;}
    if(ok)ok=append_bytes_field(merged,sizeof(merged),&n,2,saved->key,saved->length)&&mesh_radio_manage_channel(index,channels[index].info.role,merged,n,revision);
    mesh_keys_wipe(current,sizeof(current));mesh_keys_wipe(merged,sizeof(merged));return ok;
}
static void keys_poll(int64_t now) {
    if(keys_loaded&&now-keys_loaded_us>=120000000)mesh_radio_keys_clear();
    if(keys_state!=MESH_KEYS_READING&&keys_state!=MESH_KEYS_SAVING)return;
    if(keys_origin!=local_node_number){keys_state=MESH_KEYS_INTERRUPTED;mesh_radio_keys_clear();mesh_keys_wipe(keys_desired_private,32);history_revision++;return;}
    if(now-keys_started>=60000000){keys_state=MESH_KEYS_TIMEOUT;mesh_radio_keys_clear();mesh_keys_wipe(keys_desired_private,32);history_revision++;return;}
    if(keys_state==MESH_KEYS_SAVING&&now>=keys_read_at&&!keys_nonce&&status.synchronized&&!status.reconnecting)mesh_radio_request_config();
}
