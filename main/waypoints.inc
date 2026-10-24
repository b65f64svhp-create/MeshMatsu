#ifndef WAYPOINT_PATH
#define WAYPOINT_PATH "/locfd/meshmatsu-waypoints.bin"
#endif
static EXT_RAM_BSS_ATTR mesh_waypoint_t waypoints[MESH_WAYPOINT_LIMIT];
static int64_t waypoint_sent_us[MESH_WAYPOINT_LIMIT];
static uint8_t waypoint_hops[MESH_WAYPOINT_LIMIT];
static size_t waypoint_count;
static bool waypoint_dirty;
static int64_t waypoint_save_at;
uint32_t mesh_radio_time(void){return current_message_timestamp();}
static bool waypoint_expired(const mesh_waypoint_t *w) {
    uint32_t now=current_message_timestamp();
    return w->deleted || w->expire==1 || (w->expire&&w->expire!=INT32_MAX&&now>=VALID_UNIX_TIME&&w->expire<=now);
}
static bool waypoint_scope(const mesh_waypoint_t *a,const mesh_waypoint_t *b) {
    return a->id==b->id&&a->peer_node==b->peer_node&&(a->peer_node||a->channel==b->channel);
}
static void waypoint_changed(void){history_revision++;if(!waypoint_dirty)waypoint_save_at=esp_timer_get_time()+2000000;waypoint_dirty=true;}
static size_t waypoint_slot(const mesh_waypoint_t *w) {
    for(size_t i=0;i<waypoint_count;i++)if(waypoint_scope(&waypoints[i],w))return i;
    if(waypoint_count<MESH_WAYPOINT_LIMIT)return waypoint_count;
    // Never evict an in-flight send. Prefer expired markers/tombstones, then oldest report.
    size_t victim=SIZE_MAX;
    for(unsigned pass=0;pass<2;pass++){for(size_t i=0;i<waypoint_count;i++){
        if(waypoints[i].delivery==MESH_DELIVERY_PENDING||(!pass&&!waypoint_expired(&waypoints[i])))continue;
        if(victim==SIZE_MAX||waypoints[i].timestamp<waypoints[victim].timestamp)victim=i;
    }
        if(victim!=SIZE_MAX)break;
    }
    return victim;
}
static bool waypoint_active(const mesh_waypoint_t *w){return w->id&&!waypoint_expired(w);}
size_t mesh_radio_waypoint_count(void){size_t n=0;for(size_t i=0;i<waypoint_count;i++)if(waypoint_active(&waypoints[i]))n++;return n;}
bool mesh_radio_get_waypoint(size_t index,mesh_waypoint_t *out){if(!out)return false;for(size_t i=0;i<waypoint_count;i++)if(waypoint_active(&waypoints[i])&&!index--){*out=waypoints[i];return true;}
    return false;}
bool mesh_radio_find_waypoint(uint32_t id,uint32_t peer,uint8_t channel,mesh_waypoint_t *out){
    if(!out)return false;
    mesh_waypoint_t key={.id=id,.peer_node=peer,.channel=channel};
    for(size_t i=0;i<waypoint_count;i++)if(waypoint_scope(&waypoints[i],&key)){*out=waypoints[i];return true;}
    return false;
}
bool mesh_radio_waypoint_editable(const mesh_waypoint_t *w){return w&&local_node_number&&(!w->locked_to||w->locked_to==local_node_number);}
uint32_t mesh_radio_new_waypoint_id(void){for(unsigned tries=0;tries<16;tries++){uint32_t id=esp_random();if(!id)continue;bool used=false;for(size_t i=0;i<waypoint_count;i++)if(waypoints[i].id==id)used=true;if(!used)return id;}return 0;}
static bool waypoint_coordinates(const mesh_waypoint_t *w){return w->latitude_i>=-900000000&&w->latitude_i<=900000000&&w->longitude_i>=-1800000000&&w->longitude_i<=1800000000;}
static void decode_waypoint(uint32_t sender,uint32_t destination,uint32_t packet_id,uint32_t timestamp,uint32_t channel,const uint8_t *data,size_t size) {
    if(!sender||sender==MESH_BROADCAST_NODE||!packet_id||size>233||
        (destination!=MESH_BROADCAST_NODE&&destination!=local_node_number)||channel>=8)return;
    mesh_waypoint_t w={.creator=sender,.updated_by=sender,.packet_id=packet_id,.timestamp=timestamp?timestamp:current_message_timestamp(),
        .peer_node=destination==MESH_BROADCAST_NODE?0:sender,.channel=destination==MESH_BROADCAST_NODE?channel:0};
    size_t p=0;uint16_t seen=0;
    while(p<size){size_t begin=p;protobuf_field_t f;if(!read_field(data,size,&p,&f)||!f.field)return;
        if(f.field<=8){if(seen&(1u<<f.field))return;seen|=1u<<f.field;}
        if(f.field==1||f.field==4||f.field==5){if(f.wire!=0||f.integer>UINT32_MAX)return;
            if(f.field==1)w.id=f.integer;else if(f.field==4)w.expire=f.integer;else w.locked_to=f.integer;
        }else if(f.field==2||f.field==3||f.field==8){if(f.wire!=5||f.length!=4)return;
            if(f.field==2)memcpy(&w.latitude_i,f.value,4);else if(f.field==3)memcpy(&w.longitude_i,f.value,4);else memcpy(&w.icon,f.value,4);
        }else if(f.field==6||f.field==7){char *out=f.field==6?w.name:w.description;size_t capacity=f.field==6?sizeof(w.name):sizeof(w.description);
            if(f.wire!=2||f.length>=capacity||memchr(f.value,0,f.length))return;
            memcpy(out,f.value,f.length);}
        else {if(p-begin>sizeof(w.extra)-w.extra_size)return;memcpy(w.extra+w.extra_size,data+begin,p-begin);w.extra_size+=p-begin;}
    }
    if(!w.id||w.locked_to==MESH_BROADCAST_NODE)return;
    bool expired=waypoint_expired(&w);
    if((seen&12)&&((seen&12)!=12||!waypoint_coordinates(&w)))return;
    if(!expired&&(seen&12)!=12)return;
    if(w.icon>0x10ffff||(w.icon>=0xd800&&w.icon<=0xdfff))return;
    size_t slot=waypoint_slot(&w);if(slot==SIZE_MAX)return;
    bool existing=slot<waypoint_count&&waypoint_scope(&waypoints[slot],&w);
    if(existing){const mesh_waypoint_t *old=&waypoints[slot];
        if((old->locked_to&&old->locked_to!=sender)||old->packet_id==packet_id||
            (old->timestamp&&timestamp&&timestamp<old->timestamp))return;
        w.creator=old->creator;
        if(expired&&(seen&12)!=12){w.latitude_i=old->latitude_i;w.longitude_i=old->longitude_i;}
    }
    w.deleted=expired;waypoints[slot]=w;waypoint_sent_us[slot]=0;if(slot==waypoint_count)waypoint_count++;
    waypoint_changed();
}
static bool encode_waypoint(const mesh_waypoint_t *w,uint8_t *out,size_t capacity,size_t *n){
    *n=0;
    if(!append_varint(out,capacity,n,8)||!append_varint(out,capacity,n,w->id))return false;
    if(capacity-*n<10)return false;
    out[(*n)++]=0x15;memcpy(out+*n,&w->latitude_i,4);*n+=4;
    out[(*n)++]=0x1d;memcpy(out+*n,&w->longitude_i,4);*n+=4;
    if(!append_varint(out,capacity,n,32)||!append_varint(out,capacity,n,w->expire)||
        !append_varint(out,capacity,n,40)||!append_varint(out,capacity,n,w->locked_to)||
        !append_bytes_field(out,capacity,n,6,(const uint8_t *)w->name,strlen(w->name))||
        !append_bytes_field(out,capacity,n,7,(const uint8_t *)w->description,strlen(w->description))||capacity-*n<5)return false;
    out[(*n)++]=0x45;memcpy(out+*n,&w->icon,4);*n+=4;
    if(w->extra_size>sizeof(w->extra)||w->extra_size>capacity-*n)return false;
    size_t extra_offset=0;
    while(extra_offset<w->extra_size){protobuf_field_t f;if(!read_field(w->extra,w->extra_size,&extra_offset,&f)||f.field<=8)return false;}
    memcpy(out+*n,w->extra,w->extra_size);*n+=w->extra_size;return true;
}
bool mesh_radio_send_waypoint(const mesh_waypoint_t *draft,bool remove) {
    if(!draft||!draft->id||!local_node_number||draft->peer_node==local_node_number||draft->peer_node==MESH_BROADCAST_NODE||draft->channel>=8||
        !waypoint_coordinates(draft)||!memchr(draft->name,0,sizeof(draft->name))||!memchr(draft->description,0,sizeof(draft->description))||
        (!remove&&!draft->name[0])||draft->icon>0x10ffff||(draft->icon>=0xd800&&draft->icon<=0xdfff)||!mesh_radio_waypoint_editable(draft))return false;
    if(!draft->peer_node&&(!channels[draft->channel].info.loaded||!channels[draft->channel].info.role))return false;
    mesh_waypoint_t w=*draft;size_t slot=waypoint_slot(&w);if(slot==SIZE_MAX)return false;
    bool existing=slot<waypoint_count&&waypoint_scope(&waypoints[slot],&w);
    if(existing&&(!mesh_radio_waypoint_editable(&waypoints[slot])||waypoints[slot].delivery==MESH_DELIVERY_PENDING))return false;
    if(remove&&!existing)return false;
    w.creator=existing?waypoints[slot].creator:local_node_number;w.updated_by=local_node_number;
    w.timestamp=current_message_timestamp();w.expire=remove?1:w.expire;w.deleted=remove;
    if(!remove&&waypoint_expired(&w))return false;
    if(w.locked_to&&w.locked_to!=local_node_number)return false;
    uint8_t payload[233],data[256];size_t n=0,p=0;
    if(!encode_waypoint(&w,payload,sizeof(payload),&n)||!append_varint(data,sizeof(data),&p,8)||!append_varint(data,sizeof(data),&p,8)||
        !append_bytes_field(data,sizeof(data),&p,2,payload,n))return false;
    uint32_t id;uint8_t hops;
    if(!transmit_chat_data(w.peer_node?w.peer_node:MESH_BROADCAST_NODE,w.peer_node?0:w.channel,data,p,&id,&hops))return false;
    w.packet_id=id;w.delivery=MESH_DELIVERY_PENDING;w.error_reason=0;w.failure=0;
    waypoints[slot]=w;waypoint_sent_us[slot]=esp_timer_get_time();waypoint_hops[slot]=hops;if(slot==waypoint_count)waypoint_count++;
    waypoint_changed();return true;
}
static void waypoint_ack(uint32_t sender,uint32_t destination,uint32_t request_id,uint32_t error_reason){
    if(destination!=local_node_number&&!(sender==local_node_number&&!destination))return;
    for(size_t i=0;i<waypoint_count;i++){mesh_waypoint_t *w=&waypoints[i];if(w->packet_id!=request_id||w->delivery!=MESH_DELIVERY_PENDING)continue;
        if(w->peer_node&&!error_reason&&sender!=w->peer_node)continue;
        if(error_reason&&sender!=local_node_number&&w->peer_node&&sender!=w->peer_node)continue;
        w->delivery=error_reason?MESH_DELIVERY_FAILED:MESH_DELIVERY_ACKNOWLEDGED;w->error_reason=error_reason;w->failure=error_reason?MESH_FAILURE_RADIO:0;
        waypoint_sent_us[i]=0;waypoint_changed();}
}
static void waypoint_echo(uint32_t packet_id,uint32_t hops){for(size_t i=0;i<waypoint_count;i++)if(!waypoints[i].peer_node&&waypoints[i].packet_id==packet_id&&
    waypoints[i].delivery==MESH_DELIVERY_PENDING&&hops<waypoint_hops[i]){waypoints[i].delivery=MESH_DELIVERY_ACKNOWLEDGED;waypoint_sent_us[i]=0;waypoint_changed();}}
// Fixed, checksummed local store. Complete previous snapshots survive FAT rename power loss.
typedef struct {uint32_t magic,version,count,checksum;} waypoint_header_t;
bool mesh_radio_flush_waypoints(void){
    if(!waypoint_dirty)return true;
    if(!history_fs_ready)return false;
    waypoint_header_t h={.magic=0x4d575031,.version=1,.count=waypoint_count};
    h.checksum=history_checksum(2166136261U,&h,offsetof(waypoint_header_t,checksum));h.checksum=history_checksum(h.checksum,waypoints,waypoint_count*sizeof(waypoints[0]));
    FILE *f=fopen(WAYPOINT_PATH ".tmp","wb");if(!f)return false;
    bool ok=fwrite(&h,sizeof(h),1,f)==1&&(!waypoint_count||fwrite(waypoints,sizeof(waypoints[0]),waypoint_count,f)==waypoint_count)&&fflush(f)==0&&fsync(fileno(f))==0;
    if(fclose(f))ok=false;
    bool backed=false;
    if(ok&&access(WAYPOINT_PATH,F_OK)==0){remove(WAYPOINT_PATH ".bak");backed=rename(WAYPOINT_PATH,WAYPOINT_PATH ".bak")==0;ok=backed;}
    if(ok)ok=rename(WAYPOINT_PATH ".tmp",WAYPOINT_PATH)==0;
    if(!ok){if(backed)rename(WAYPOINT_PATH ".bak",WAYPOINT_PATH);remove(WAYPOINT_PATH ".tmp");return false;}
    waypoint_dirty=false;return true;
}
static bool read_waypoints(const char *path){
    FILE *f=fopen(path,"rb");if(!f)return false;waypoint_header_t h;
    bool ok=fread(&h,sizeof(h),1,f)==1&&h.magic==0x4d575031&&h.version==1&&h.count<=MESH_WAYPOINT_LIMIT;
    uint32_t hash=ok?history_checksum(2166136261U,&h,offsetof(waypoint_header_t,checksum)):0;
    mesh_waypoint_t w;
    for(size_t i=0;ok&&i<h.count;i++){ok=fread(&w,sizeof(w),1,f)==1;if(ok)hash=history_checksum(hash,&w,sizeof(w));
        const uint8_t *raw=(const uint8_t *)&w;
        ok=ok&&w.id&&w.channel<8&&raw[offsetof(mesh_waypoint_t,deleted)]<=1&&w.extra_size<=sizeof(w.extra)&&w.delivery>=MESH_DELIVERY_NONE&&w.delivery<=MESH_DELIVERY_FAILED&&w.failure<=MESH_FAILURE_RESTARTED&&
            memchr(w.name,0,sizeof(w.name))&&memchr(w.description,0,sizeof(w.description))&&waypoint_coordinates(&w);}
    ok=ok&&hash==h.checksum&&fgetc(f)==EOF&&fseek(f,sizeof(h),SEEK_SET)==0;
    if(ok&&h.count)ok=fread(waypoints,sizeof(waypoints[0]),h.count,f)==h.count;
    fclose(f);
    if(ok)waypoint_count=h.count;
    return ok;
}
static void restore_waypoints(void){waypoint_count=0;waypoint_dirty=false;memset(waypoint_sent_us,0,sizeof(waypoint_sent_us));
    if(!history_fs_ready)return;
    if(!read_waypoints(WAYPOINT_PATH)&&read_waypoints(WAYPOINT_PATH ".bak")){remove(WAYPOINT_PATH);waypoint_changed();}
    for(size_t i=0;i<waypoint_count;i++)if(waypoints[i].delivery==MESH_DELIVERY_PENDING){waypoints[i].delivery=MESH_DELIVERY_FAILED;waypoints[i].failure=MESH_FAILURE_RESTARTED;waypoint_changed();}}
static void waypoint_poll(int64_t now){
    for(size_t i=0;i<waypoint_count;i++)if(waypoints[i].delivery==MESH_DELIVERY_PENDING&&waypoint_sent_us[i]&&now-waypoint_sent_us[i]>=90000000){
        waypoints[i].delivery=MESH_DELIVERY_FAILED;waypoints[i].failure=MESH_FAILURE_TIMEOUT;waypoint_sent_us[i]=0;waypoint_changed();}
    if(waypoint_dirty&&now>=waypoint_save_at&&!mesh_radio_flush_waypoints())waypoint_save_at=now+5000000;
}
