// Fresh local UART dumps only. A routing ACK never establishes restore success.
static void cfg_capture_reset(uint32_t nonce){
    if(cfg_state!=CFG_READING&&cfg_state!=CFG_SAVING)return;
    mesh_keys_wipe(&cfg_snapshot,sizeof(cfg_snapshot));
    cfg_nonce=nonce;
    cfg_bad=false;
    cfg_metadata_seen=false;
}
static void cfg_record(unsigned kind,unsigned id,const uint8_t *data,size_t n){
    if(((kind==CFG_RADIO||kind==CFG_MODULE)&&n>1024)||(kind==CFG_CHANNEL&&n>192)||cfg_bad||!mesh_config_add(&cfg_snapshot,kind,id,data,n))cfg_bad=true;
}
static void cfg_from(unsigned tag,const uint8_t *data,size_t n){
    if((cfg_state!=CFG_READING&&cfg_state!=CFG_SAVING)||!config_request_pending||cfg_nonce!=config_nonce)return;
    if(tag==13){
        size_t p=0;
        protobuf_field_t f;
        bool fw=false;
        while(p<n){
            if(!read_field(data,n,&p,&f)){
                cfg_bad=true;
                return;
            }
            if(f.field==1){
                if(f.wire!=2||!f.length||f.length>=64){
                    cfg_bad=true;
                    return;
                }
                for(size_t i=0;i<f.length;i++)if(f.value[i]<32||f.value[i]>126){
                    cfg_bad=true;
                    return;
                }
                fw=true;
            }
            if(f.field==9){
                if(f.wire!=0||f.integer>UINT32_MAX){
                    cfg_bad=true;
                    return;
                }
            }
        }
        cfg_metadata_seen=fw;
        if(!cfg_metadata_seen)cfg_bad=true;
        return;
    }
    if(tag==5||tag==9){
        size_t p=0;
        protobuf_field_t f;
        if(!read_field(data,n,&p,&f)||p!=n||f.wire!=2){
            cfg_bad=true;
            return;
        }
        if(tag==5&&(f.field==9||f.field==10))return;
        // Ephemeral session key and separate firmware UI placeholder.
        if((tag==5&&(f.field<1||f.field>8))||(tag==9&&(f.field<1||f.field>13))){
            cfg_bad=true;
            return;
        }
        size_t q=0;
        while(q<f.length){
            protobuf_field_t inner;
            if(!read_field(f.value,f.length,&q,&inner)){
                cfg_bad=true;
                return;
            }
        }
        cfg_record(tag==5?CFG_RADIO:CFG_MODULE,f.field,data,n);
    }
    else if(tag==10){
        size_t p=0;
        protobuf_field_t f;
        uint32_t index=0;
        while(p<n){
            if(!read_field(data,n,&p,&f)){
                cfg_bad=true;
                return;
            }
            if(f.field==1){
                if(f.wire!=0||f.integer>7){
                    cfg_bad=true;
                    return;
                }
                index=f.integer;
            }
        }
        cfg_record(CFG_CHANNEL,index,data,n);
    }
    else if(tag==4){
        size_t p=0;
        protobuf_field_t f;
        uint32_t number=0;
        const uint8_t *user=NULL;
        size_t length=0;
        mesh_node_t fixed={
            0
        }
        ;
        while(p<n){
            if(!read_field(data,n,&p,&f)){
                cfg_bad=true;
                return;
            }
            if(f.field==1&&f.wire==0)number=f.integer;
            else if(f.field==3&&f.wire==2){
                decode_position(f.value,f.length,&fixed);
            }
            else if(f.field==2&&f.wire==2){
                user=f.value;
                length=f.length;
            }
        }
        if(number==local_node_number&&user&&!mesh_config_find(&cfg_snapshot,CFG_OWNER,0)){
            cfg_record(CFG_OWNER,0,user,length);
            uint8_t coords[9]={
                fixed.position_available
            }
            ;
            if(fixed.position_available){
                memcpy(coords+1,&fixed.latitude_i,4);
                memcpy(coords+5,&fixed.longitude_i,4);
            }
            cfg_record(CFG_FIXED,0,coords,sizeof(coords));
        }
    }
}
static void cfg_complete(uint32_t nonce){
    if((cfg_state!=CFG_READING&&cfg_state!=CFG_SAVING)||!cfg_nonce||nonce!=cfg_nonce||nonce!=config_nonce)return;
    bool complete=!cfg_bad&&cfg_metadata_seen&&radio_metadata.loaded&&cfg_origin==local_node_number&&cfg_snapshot.count==31&&mesh_config_find(&cfg_snapshot,CFG_OWNER,0);
    for(unsigned i=1;i<=8;i++)complete=complete&&mesh_config_find(&cfg_snapshot,CFG_RADIO,i);
    for(unsigned i=1;i<=13;i++)complete=complete&&mesh_config_find(&cfg_snapshot,CFG_MODULE,i);
    for(unsigned i=0;i<8;i++)complete=complete&&mesh_config_find(&cfg_snapshot,CFG_CHANNEL,i);
    if(!complete){
        cfg_state=CFG_FAILED;
        mesh_keys_wipe(&cfg_desired,sizeof(cfg_desired));
        mesh_keys_wipe(&cfg_snapshot,sizeof(cfg_snapshot));
    }
    else{
        cfg_snapshot.node=local_node_number;
        cfg_snapshot.timestamp=current_message_timestamp();
        cfg_snapshot.hardware=radio_metadata.hardware_model;
        snprintf(cfg_snapshot.firmware,sizeof(cfg_snapshot.firmware),"%s",radio_metadata.firmware_version);
        snprintf(cfg_snapshot.name,sizeof(cfg_snapshot.name),"%s",local_long_name);
        if(cfg_state==CFG_SAVING){
            const mesh_cfg_record_t *actual=mesh_config_find(&cfg_snapshot,cfg_desired.kind,cfg_desired.id);
            bool match=actual&&actual->size==cfg_desired.size&&!memcmp(actual->data,cfg_desired.data,actual->size);
            if(actual&&cfg_desired.kind==CFG_OWNER){
                // Firmware sets only ID, long/short names and licensed status; public key/hardware/role are read-only User fields.
                const unsigned tags[]={1,2,3,6};match=true;
                for(unsigned i=0;i<4;i++)match=match&&channel_field_equal(actual->data,actual->size,cfg_desired.data,cfg_desired.size,tags[i],false);
            }
            cfg_state=match?CFG_SAVED:CFG_FAILED;
            mesh_keys_wipe(&cfg_desired,sizeof(cfg_desired));
            mesh_keys_wipe(&cfg_snapshot,sizeof(cfg_snapshot));
        }
        else{
            cfg_state=CFG_READY;
            cfg_loaded_us=esp_timer_get_time();
        }
    }
    history_revision++;
}
void mesh_radio_config_clear(void){
    // An already sent change keeps its verification alive, never sends it again.
    if(cfg_state!=CFG_SAVING){
        cfg_state=CFG_IDLE;
        cfg_nonce=0;
        mesh_keys_wipe(&cfg_snapshot,sizeof(cfg_snapshot));
        mesh_keys_wipe(&cfg_desired,sizeof(cfg_desired));
    }
}
mesh_cfg_state_t mesh_radio_config_state(void){
    return cfg_state;
}
bool mesh_radio_config_capture(void){
    if(cfg_state==CFG_READING||cfg_state==CFG_SAVING||!channel_management_ready())return false;
    mesh_radio_config_clear();
    cfg_state=CFG_READING;
    cfg_origin=local_node_number;
    cfg_started=esp_timer_get_time();
    mesh_radio_request_config();
    return true;
}
bool mesh_radio_config_snapshot(mesh_config_backup_t *out){
    if(!out||cfg_state!=CFG_READY||cfg_origin!=local_node_number||!status.synchronized||status.reconnecting||esp_timer_get_time()-cfg_loaded_us>=120000000)return false;
    *out=cfg_snapshot;
    // Only fixed coordinates are configuration; live GPS fixes are deliberately excluded.
    const mesh_cfg_record_t *position=mesh_config_find(out,CFG_RADIO,2);
    bool fixed=false;
    if(position){
        size_t p=0;
        protobuf_field_t wrapper,f;
        if(read_field(position->data,position->size,&p,&wrapper)&&wrapper.wire==2){
            p=0;
            while(p<wrapper.length&&read_field(wrapper.value,wrapper.length,&p,&f))if(f.field==3&&f.wire==0)fixed=f.integer!=0;
        }
    }
    if(!fixed){
        const mesh_cfg_record_t *coords=mesh_config_find(out,CFG_FIXED,0);
        if(coords)memset(out->records[coords-out->records].data,0,9);
    }
    return true;
}
void mesh_radio_config_identity_validator(bool (*check)(const uint8_t *,const uint8_t *)){
    cfg_identity_validator=check;
}
static bool cfg_serial_allowed(const mesh_cfg_record_t *r){
    if(r->kind!=CFG_RADIO||r->id!=8)return true;
    size_t p=0;
    protobuf_field_t wrapper,f;
    if(!read_field(r->data,r->size,&p,&wrapper)||wrapper.field!=8||wrapper.wire!=2||p!=r->size)return false;
    p=0;
    bool serial=false,managed=false,ok=false;
    uint8_t pub[32]={
        0
    }
    ,priv[32]={
        0
    }
    ;
    size_t pn=0,sn=0;
    while(p<wrapper.length){
        if(!read_field(wrapper.value,wrapper.length,&p,&f))goto cleanup;
        if(f.field==5){
            if(f.wire!=0||f.integer>1)goto cleanup;
            serial=f.integer;
        }
        if(f.field==4){
            if(f.wire!=0||f.integer>1)goto cleanup;
            managed=f.integer;
        }
        if(f.field==1||f.field==2){
            if(f.wire!=2||f.length!=32)goto cleanup;
            if(f.field==1){
                memcpy(pub,f.value,32);
                pn=32;
            }
            else{
                memcpy(priv,f.value,32);
                sn=32;
            }
        }
    }
    ok=serial&&!managed&&pn==32&&sn==32&&cfg_identity_validator&&cfg_identity_validator(pub,priv);
    cleanup:
    mesh_keys_wipe(priv,sizeof(priv));
    return ok;
}
static bool cfg_target_valid(const mesh_cfg_record_t *r){
    if(r->kind==CFG_RADIO||r->kind==CFG_MODULE){
        size_t p=0;
        protobuf_field_t f;
        return read_field(r->data,r->size,&p,&f)&&p==r->size&&f.wire==2&&f.field==r->id&&r->id>=1&&r->id<=(r->kind==CFG_RADIO?8:13);
    }
    if(r->kind==CFG_CHANNEL){
        size_t p=0;
        uint32_t index=0;
        protobuf_field_t f;
        bool seen=false;
        while(p<r->size){
            if(!read_field(r->data,r->size,&p,&f))return false;
            if(f.field==1){
                if(seen||f.wire!=0||f.integer>7)return false;
                seen=true;
                index=f.integer;
            }
        }
        return index==r->id;
    }
    if(r->kind==CFG_OWNER){
        size_t p=0;
        protobuf_field_t f;
        while(p<r->size)if(!read_field(r->data,r->size,&p,&f))return false;
        return true;
    }
    if(r->kind==CFG_FIXED){
        int32_t lat,lon;
        if(r->id||r->size!=9||r->data[0]!=1)return false;
        memcpy(&lat,r->data+1,4);
        memcpy(&lon,r->data+5,4);
        return lat>=-900000000&&lat<=900000000&&lon>=-1800000000&&lon<=1800000000;
    }
    return false;
}
bool mesh_radio_config_restore(const mesh_config_backup_t *saved,const mesh_cfg_record_t *r){
    if(!saved||!r||r->kind<CFG_OWNER||(r->kind>CFG_CHANNEL&&r->kind!=CFG_FIXED)||cfg_state!=CFG_READY||!channel_management_ready()||
    cfg_origin!=local_node_number||saved->node!=local_node_number||saved->hardware!=radio_metadata.hardware_model||!memchr(saved->firmware,0,sizeof(saved->firmware))||strcmp(saved->firmware,radio_metadata.firmware_version)||
    esp_timer_get_time()-cfg_loaded_us>=120000000||!r->size||r->size>MESH_CFG_RADIO_MAX||!cfg_target_valid(r)||!cfg_serial_allowed(r))return false;
    // Only permit a record actually present in the authenticated archive.
    const mesh_cfg_record_t *source=mesh_config_find(saved,r->kind,r->id);
    if(!source||source->size!=r->size||memcmp(source->data,r->data,r->size))return false;
    bool ok=false;
    uint32_t id=0;
    if(r->kind==CFG_FIXED){
        int32_t lat,lon;
        memcpy(&lat,r->data+1,4);
        memcpy(&lon,r->data+5,4);
        ok=mesh_radio_set_fixed_position(lat,lon,position_config.revision);
        id=location_op.write_id;
    }
    else if(r->kind==CFG_CHANNEL){
        uint8_t role=0;
        const uint8_t *settings=NULL;
        size_t n=0,p=0;
        protobuf_field_t f;
        while(p<r->size){
            if(!read_field(r->data,r->size,&p,&f))return false;
            if(f.field==2&&f.wire==2){
                settings=f.value;
                n=f.length;
            }
            else if(f.field==3&&f.wire==0){
                if(f.integer>2)return false;
                role=f.integer;
            }
        }
        if(!settings&&role)return false;
        mesh_channel_t current;
        if(!mesh_radio_get_channel(r->id,&current))return false;
        ok=mesh_radio_manage_channel(r->id,role,settings,n,current.revision);
        id=channel_operation_data.write_id;
    }
    else{
        uint8_t admin[264];
        size_t n=0;
        unsigned tag=r->kind==CFG_OWNER?32:r->kind==CFG_RADIO?34:35;
        ok=append_bytes_field(admin,sizeof(admin),&n,tag,r->data,r->size)&&append_bytes_field(admin,sizeof(admin),&n,101,session_passkey,session_passkey_size)&&send_admin_payload_with_id(admin,n,&id);
        mesh_keys_wipe(admin,sizeof(admin));
    }
    if(!ok)return false;
    cfg_desired=*r;
    cfg_write_id=id;
    cfg_state=CFG_SAVING;
    cfg_started=esp_timer_get_time();
    cfg_read_at=cfg_started+4000000;
    cfg_nonce=0;
    mesh_keys_wipe(&cfg_snapshot,sizeof(cfg_snapshot));
    history_revision++;
    return true;
}
static void cfg_poll(int64_t now){
    if(cfg_state==CFG_READY&&now-cfg_loaded_us>=120000000)mesh_radio_config_clear();
    if(cfg_state!=CFG_READING&&cfg_state!=CFG_SAVING)return;
    if((local_node_number&&local_node_number!=cfg_origin)||now-cfg_started>=120000000){
        cfg_state=CFG_TIMEOUT;
        mesh_keys_wipe(&cfg_snapshot,sizeof(cfg_snapshot));
        mesh_keys_wipe(&cfg_desired,sizeof(cfg_desired));
        history_revision++;
        return;
    }
    if(cfg_state==CFG_SAVING&&now>=cfg_read_at&&!cfg_nonce&&status.synchronized&&!status.reconnecting)mesh_radio_request_config();
}
