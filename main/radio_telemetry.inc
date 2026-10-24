// Passive Meshtastic TELEMETRY_APP decoding. DeviceMetrics fields are optional;
// absence must not become a zero battery/voltage/activity measurement.
static bool decode_device_metrics(const uint8_t *data, size_t size, bool local_stats,
                                  mesh_device_metrics_t *out) {
    mesh_device_metrics_t m = {0}; size_t offset = 0;
    if (local_stats) m.has_channel_use = m.has_airtime = m.has_uptime = true;
    while (offset < size) {
        protobuf_field_t f;
        if (!read_field(data, size, &offset, &f)) return false;
        unsigned battery_tag = local_stats ? 0 : 1, uptime_tag = local_stats ? 1 : 5;
        unsigned voltage_tag = local_stats ? 0 : 2, channel_tag = local_stats ? 2 : 3, airtime_tag = local_stats ? 3 : 4;
        if (f.field == battery_tag || f.field == uptime_tag) {
            if (f.wire != 0 || f.integer > UINT32_MAX) return false;
            if (f.field == battery_tag) { m.has_battery = true; m.battery = (uint32_t)f.integer; }
            else { m.has_uptime = true; m.uptime = (uint32_t)f.integer; }
        } else if (f.field == voltage_tag || f.field == channel_tag || f.field == airtime_tag) {
            if (f.wire != 5 || f.length != 4) return false;
            float value; memcpy(&value, f.value, 4);
            bool valid = isfinite(value) && value >= 0;
            if (f.field == voltage_tag) { m.has_voltage = valid; if (valid) m.voltage = value; }
            else if (f.field == channel_tag) { m.has_channel_use = valid && value <= 100; if (m.has_channel_use) m.channel_use = value; }
            else { m.has_airtime = valid && value <= 100; if (m.has_airtime) m.airtime = value; }
        }
    }
    m.available = m.has_battery || m.has_voltage || m.has_channel_use || m.has_airtime || m.has_uptime;
    if (!m.available) return false;
    *out = m; return true;
}
static bool metrics_replace(const mesh_device_metrics_t *old, const mesh_device_metrics_t *incoming) {
    if (!incoming->available) return false;
    if (!old->available) return true;
    if (incoming->cached && !old->cached) return false;
    if (!incoming->cached && !old->cached) {
        if (incoming->packet_id && incoming->packet_id == old->packet_id) return false;
        if (incoming->sample_time && old->sample_time && incoming->sample_time < old->sample_time) return false;
        if (incoming->sample_time && incoming->sample_time == old->sample_time &&
            incoming->has_battery == old->has_battery && incoming->has_voltage == old->has_voltage &&
            incoming->has_channel_use == old->has_channel_use && incoming->has_airtime == old->has_airtime &&
            incoming->has_uptime == old->has_uptime && incoming->battery == old->battery &&
            incoming->voltage == old->voltage && incoming->channel_use == old->channel_use &&
            incoming->airtime == old->airtime && incoming->uptime == old->uptime) return false;
    }
    return true;
}
static void record_device_report(uint32_t node,const mesh_device_metrics_t *m,uint8_t kind) {
 if(!m->available)return;
 mesh_tlm_sample_t s={.node=node,.time=m->sample_time,.packet=m->packet_id,.received_at=current_message_timestamp(),.received_us=esp_timer_get_time(),.kind=kind,.flags=m->cached?MESH_TLM_CACHED:0};
 if(kind==2){
  const bool has[]={m->has_battery,m->has_voltage,m->has_channel_use,m->has_airtime,m->has_uptime};
  const double v[]={m->battery,m->voltage,m->channel_use,m->airtime,m->uptime};
  for(unsigned i=0;i<5;i++)if(has[i]){s.present|=1u<<i;s.value[i]=v[i];}
 }else{
  if(m->has_uptime){s.present|=1;s.value[0]=m->uptime;}
  if(m->has_channel_use){s.present|=2;s.value[1]=m->channel_use;}
  if(m->has_airtime){s.present|=4;s.value[2]=m->airtime;}
 }
 if(s.time&&s.received_at&&(uint64_t)s.time>(uint64_t)s.received_at+300u)s.flags|=MESH_TLM_CLOCK_SKEW;
 mesh_tlm_add(&s);
}
static bool decode_sensor_report(uint32_t node,uint8_t kind,uint32_t timestamp,uint32_t packet,const uint8_t *data,size_t size) {
 mesh_tlm_sample_t s={.node=node,.kind=kind,.time=timestamp>=VALID_UNIX_TIME?timestamp:0,.packet=packet,.received_at=current_message_timestamp(),.received_us=esp_timer_get_time()};
 size_t p=0;uint32_t seen=0;
 while(p<size){protobuf_field_t f;if(!read_field(data,size,&p,&f))return false;
  const mesh_tlm_field_t *info=mesh_tlm_field(kind,f.field);if(!info)continue;
  uint32_t bit=1u<<(f.field-1);if(seen&bit)return false;seen|=bit;double value;
  if(info->integer){if(f.wire!=0||f.integer>UINT32_MAX)return false;value=f.integer;}
  else{if(f.wire!=5||f.length!=4)return false;float v;memcpy(&v,f.value,4);value=v;}
  if(!isfinite(value)||value<info->min||value>info->max)continue;
  s.present|=bit;s.value[f.field-1]=value;
 }
 if(s.time&&s.received_at&&(uint64_t)s.time>(uint64_t)s.received_at+300)s.flags|=MESH_TLM_CLOCK_SKEW;
 if(!s.present)return false;
 if(mesh_tlm_add(&s))history_revision++;
 return true;
}
static void update_node_metrics(uint32_t sender, const mesh_device_metrics_t *metrics) {
    if (!sender || sender == MESH_BROADCAST_NODE) return;
    record_device_report(sender,metrics,2);
    if (sender == local_node_number && (local_activity_node != sender || metrics_replace(&local_activity, metrics))) { local_activity = *metrics;local_activity_node=sender;history_revision++; }
    for (size_t i=0; i<node_count; i++) if (nodes[i].number == sender) {
        if (metrics_replace(&nodes[i].metrics, metrics)) { nodes[i].metrics = *metrics; history_revision++; }
        return;
    }
    if (node_count >= MESH_NODE_LIMIT) return;
    mesh_node_t *node = &nodes[node_count++]; *node = (mesh_node_t){.number = sender, .metrics = *metrics};
    snprintf(node->name, sizeof(node->name), "!%08lx", (unsigned long)sender);
    status.node_count = (uint16_t)node_count; history_revision++;
}
static void decode_telemetry(uint32_t sender, uint32_t destination, uint32_t packet_id, uint32_t request_id,
                             const uint8_t *data, size_t size) {
    if (!sender || sender == MESH_BROADCAST_NODE ||
        (destination && destination != MESH_BROADCAST_NODE && destination != local_node_number)) return;
    const uint8_t *payload = NULL; size_t payload_size = 0, offset = 0;
    uint32_t sample_time = 0; unsigned variant = 0;
    while (offset < size) {
        protobuf_field_t f;
        if (!read_field(data, size, &offset, &f)) return;
        if (f.field == 1) {
            if (f.wire != 5 || f.length != 4) return; // Telemetry.time is fixed32.
            memcpy(&sample_time, f.value, 4);
        } else if (f.field >= 2 && f.field <= 11) {
            if (f.wire != 2) return;
            if(variant)return;
            variant = f.field; payload = f.value; payload_size = f.length;
        }
    }
    if(variant>=3&&variant<=5){if(decode_sensor_report(sender,variant,sample_time,packet_id,payload,payload_size))telemetry_request_reply(sender,destination,request_id,variant);return;}
    if (variant != 2 && variant != 6) return;
    if (variant == 6 && (!local_node_number || sender != local_node_number)) return;
    mesh_device_metrics_t metrics;
    if (!decode_device_metrics(payload, payload_size, variant == 6, &metrics)) return;
    metrics.sample_time = sample_time >= VALID_UNIX_TIME ? sample_time : 0;
    metrics.received_us = esp_timer_get_time(); metrics.packet_id = packet_id;
    if (variant == 6) {
        record_device_report(sender,&metrics,6);
        if (local_activity_node != sender || metrics_replace(&local_activity, &metrics)) { local_activity = metrics;local_activity_node=sender;history_revision++; }
    } else {
        update_node_metrics(sender, &metrics);
        telemetry_request_reply(sender,destination,request_id,2);
    }
}
bool mesh_radio_get_node_metrics(uint32_t number, mesh_device_metrics_t *out) {
    if (!out) return false;
    *out = (mesh_device_metrics_t){0};
    for (size_t i=0; i<node_count; i++) if (nodes[i].number == number) { *out=nodes[i].metrics;return out->available; }
    return false;
}
bool mesh_radio_get_local_activity(mesh_device_metrics_t *out) {
    if (!out) return false;
    *out = local_activity_node == local_node_number ? local_activity : (mesh_device_metrics_t){0};
    // MyNodeInfo may follow the cached NodeInfo in the initial dump.
    mesh_device_metrics_t cached;
    if (mesh_radio_get_node_metrics(local_node_number, &cached) && metrics_replace(out, &cached)) *out = cached;
    return out->available;
}

// One explicit unicast query per press. The firmware handles its normal reliable
// packet retries; the app never automatically re-sends a telemetry request.
unsigned mesh_radio_telemetry_cooldown_seconds(void) {
    int64_t elapsed=esp_timer_get_time()-last_telemetry_request_us;
    if(!telemetry_request_sent || elapsed>=60000000)return 0;
    if(elapsed<0)return 60;
    return (unsigned)((60000000-elapsed+999999)/1000000);
}
mesh_telemetry_start_t mesh_radio_request_telemetry(uint32_t number) {return mesh_radio_request_telemetry_kind(number,2);}
mesh_telemetry_start_t mesh_radio_request_telemetry_kind(uint32_t number,uint8_t kind) {
    if(kind<2||kind>5)return MESH_TELEMETRY_START_INVALID;
    mesh_node_t node;
    if(!number || number==MESH_BROADCAST_NODE || number==local_node_number ||
        !mesh_radio_get_node_by_number(number,&node)||node.is_ignored)return MESH_TELEMETRY_START_INVALID;
    if(!status.started || !status.synchronized || status.reconnecting || !local_node_number || config_request_pending)
        return MESH_TELEMETRY_START_OFFLINE;
    for(size_t i=0;i<8;i++)if(telemetry_requests[i].result.state==MESH_TELEMETRY_WAITING)return MESH_TELEMETRY_START_BUSY;
    if(mesh_radio_telemetry_cooldown_seconds())return MESH_TELEMETRY_START_COOLDOWN;
    // Match the normal Meshtastic telemetry exchange: include our reported
    // DeviceMetrics if available, so the request doesn't clear the peer's cached
    // metrics for us. With none available the oneof is still explicitly present.
    mesh_device_metrics_t own={0};mesh_radio_get_node_metrics(local_node_number,&own);
    uint8_t metrics[192],telemetry[208],data[224];size_t mn=0,tn=0,dn=0;
    if(own.has_battery && (!append_varint(metrics,sizeof(metrics),&mn,8) ||
        !append_varint(metrics,sizeof(metrics),&mn,own.battery)))return MESH_TELEMETRY_START_WRITE_FAILED;
    const bool present[]={own.has_voltage,own.has_channel_use,own.has_airtime};
    const float values[]={own.voltage,own.channel_use,own.airtime};
    for(unsigned i=0;i<3;i++)if(present[i]) {
        metrics[mn++]=(uint8_t)(((i+2)<<3)|5);memcpy(metrics+mn,&values[i],4);mn+=4;
    }
    if(own.has_uptime && (!append_varint(metrics,sizeof(metrics),&mn,40) ||
        !append_varint(metrics,sizeof(metrics),&mn,own.uptime)))return MESH_TELEMETRY_START_WRITE_FAILED;
    if(own.sample_time && !own.cached){telemetry[tn++]=0x0d;memcpy(telemetry+tn,&own.sample_time,4);tn+=4;}
    if(kind!=2){
        mn=tn=0;mesh_tlm_sample_t own_sensor;
        if(mesh_tlm_latest(local_node_number,kind,0,&own_sensor)&&!(own_sensor.flags&(MESH_TLM_CACHED|MESH_TLM_RESTORED|MESH_TLM_CLOCK_SKEW))){
            for(unsigned f=1;f<=MESH_TLM_FIELDS;f++)if(own_sensor.present&(1u<<(f-1))){
                const mesh_tlm_field_t *info=mesh_tlm_field(kind,f);if(!info)continue;
                if(info->integer){if(!append_varint(metrics,sizeof(metrics),&mn,f<<3)||!append_varint(metrics,sizeof(metrics),&mn,(uint32_t)own_sensor.value[f-1]))return MESH_TELEMETRY_START_WRITE_FAILED;}
                else{if(!append_varint(metrics,sizeof(metrics),&mn,(f<<3)|5)||mn+4>sizeof(metrics))return MESH_TELEMETRY_START_WRITE_FAILED;float value=own_sensor.value[f-1];memcpy(metrics+mn,&value,4);mn+=4;}
            }
            if(own_sensor.time){telemetry[tn++]=0x0d;memcpy(telemetry+tn,&own_sensor.time,4);tn+=4;}
        }
    }
    if(!append_bytes_field(telemetry,sizeof(telemetry),&tn,kind,metrics,mn) ||
        !append_varint(data,sizeof(data),&dn,8) || !append_varint(data,sizeof(data),&dn,67) ||
        !append_bytes_field(data,sizeof(data),&dn,2,telemetry,tn) ||
        !append_varint(data,sizeof(data),&dn,24) || !append_varint(data,sizeof(data),&dn,1))return MESH_TELEMETRY_START_WRITE_FAILED;
    uint32_t id;uint8_t hops;
    if(!transmit_chat_data(number,0,data,dn,&id,&hops))return MESH_TELEMETRY_START_WRITE_FAILED;
    size_t slot=8;
    for(size_t i=0;i<8;i++)if(telemetry_requests[i].result.node==number){slot=i;break;}
    if(slot==8)for(size_t i=0;i<8;i++)if(!telemetry_requests[i].result.node){slot=i;break;}
    if(slot==8){slot=telemetry_replace_index;telemetry_replace_index=(telemetry_replace_index+1)%8;}
    telemetry_requests[slot].result=(mesh_telemetry_request_t){.node=number,.origin=local_node_number,
        .request_id=id,.state=MESH_TELEMETRY_WAITING,.kind=kind};
    telemetry_requests[slot].started_us=last_telemetry_request_us=esp_timer_get_time();
    telemetry_request_sent=true;history_revision++;return MESH_TELEMETRY_START_OK;
}
bool mesh_radio_get_telemetry_request(uint32_t number, mesh_telemetry_request_t *out) {
    if(!out || !number)return false;
    for(size_t i=0;i<8;i++)if(telemetry_requests[i].result.node==number) {
        *out=telemetry_requests[i].result;
        int64_t elapsed=esp_timer_get_time()-telemetry_requests[i].started_us;
        out->elapsed_seconds=elapsed>0?(unsigned)(elapsed/1000000):0;return true;
    }
    return false;
}
static void telemetry_request_reply(uint32_t sender, uint32_t destination, uint32_t id,uint8_t kind) {
    if(!id)return;
    for(size_t i=0;i<8;i++) {
        mesh_telemetry_request_t *r=&telemetry_requests[i].result;
        if((r->state==MESH_TELEMETRY_WAITING || r->state==MESH_TELEMETRY_TIMEOUT) &&
            r->node==sender && r->origin==local_node_number && destination==r->origin && r->request_id==id && (r->kind?r->kind:2)==kind) {
            r->state=MESH_TELEMETRY_REPLY;history_revision++;return;
        }
    }
}
static void telemetry_request_error(uint32_t sender, uint32_t destination, uint32_t id, uint32_t reason) {
    if(!reason)return; // A delivery ACK is not a telemetry response.
    for(size_t i=0;i<8;i++) {
        mesh_telemetry_request_t *r=&telemetry_requests[i].result;
        if(r->state==MESH_TELEMETRY_WAITING && r->origin==local_node_number && r->request_id==id &&
            (sender==r->origin || sender==r->node) && (destination==r->origin || destination==0)) {
            r->state=MESH_TELEMETRY_FAILED;r->error_reason=reason;history_revision++;return;
        }
    }
}
static void telemetry_request_poll(int64_t now) {
    for(size_t i=0;i<8;i++) {
        mesh_telemetry_request_t *r=&telemetry_requests[i].result;
        if(r->state!=MESH_TELEMETRY_WAITING)continue;
        if(r->origin!=local_node_number){r->state=MESH_TELEMETRY_INTERRUPTED;history_revision++;}
        else if(now-telemetry_requests[i].started_us>=120000000){r->state=MESH_TELEMETRY_TIMEOUT;history_revision++;}
    }
}
