#define MANAGEMENT_TEST_MAIN earlier_management_test
#include "test_management.c"
static void range_setup(void){setup();range_status=(mesh_range_status_t){0};range_native=(mesh_range_native_t){0};range_native_id=0;range_native_origin=0;channels[0].info=(mesh_channel_t){.loaded=true,.role=1,.index=0};channels[1].info=(mesh_channel_t){.loaded=true,.role=2,.index=1};}
static void native_reply(uint32_t id,bool enabled,uint32_t sender){uint8_t raw[40],module[48],admin[64];size_t n=0,m=0,a=0;
 assert(append_varint(raw,sizeof(raw),&n,8)&&append_varint(raw,sizeof(raw),&n,enabled));assert(append_varint(raw,sizeof(raw),&n,16)&&append_varint(raw,sizeof(raw),&n,sender));assert(append_bytes_field(module,sizeof(module),&m,5,raw,n));assert(append_bytes_field(admin,sizeof(admin),&a,8,module,m));parse_admin_response(admin,a,id);}
static void range_packet(uint32_t sender,uint32_t id,uint32_t seq,uint32_t channel,bool mqtt,uint32_t hops,bool signal){
 uint8_t data[64],packet[160];size_t d=0,p=0;char text[16];int n=snprintf(text,sizeof(text),"seq %lu",(unsigned long)seq);
 assert(append_varint(data,sizeof(data),&d,8)&&append_varint(data,sizeof(data),&d,66));assert(append_bytes_field(data,sizeof(data),&d,2,(const uint8_t*)text,n));
 packet[p++]=0x0d;memcpy(packet+p,&sender,4);p+=4;uint32_t to=MESH_BROADCAST_NODE;packet[p++]=0x15;memcpy(packet+p,&to,4);p+=4;assert(append_varint(packet,sizeof(packet),&p,24)&&append_varint(packet,sizeof(packet),&p,channel));assert(append_bytes_field(packet,sizeof(packet),&p,4,data,d));
 packet[p++]=0x35;memcpy(packet+p,&id,4);p+=4;assert(append_varint(packet,sizeof(packet),&p,14<<3)&&append_varint(packet,sizeof(packet),&p,mqtt));assert(append_varint(packet,sizeof(packet),&p,9<<3)&&append_varint(packet,sizeof(packet),&p,hops));
 if(signal){float snr=0;packet[p++]=0x45;memcpy(packet+p,&snr,4);p+=4;assert(append_varint(packet,sizeof(packet),&p,12<<3)&&append_varint(packet,sizeof(packet),&p,(uint32_t)-77));}decode_mesh_packet(packet,p);
}
#ifndef RANGE_RADIO_TEST_MAIN
#define RANGE_RADIO_TEST_MAIN main
#endif
int RANGE_RADIO_TEST_MAIN(void){
 range_setup();size_t writes=write_count;assert(!mesh_radio_range_start(MESH_RANGE_SEND,0,0,60,900)&&write_count==writes);
 assert(mesh_radio_range_refresh());uint32_t id=range_native_id;native_reply(id+1,true,60);assert(range_native.pending&&!range_native.loaded);native_reply(id,true,60);assert(range_native.loaded&&range_native.enabled&&range_native.sender==60);assert(!mesh_radio_range_start(MESH_RANGE_SEND,0,0,60,900));
 assert(mesh_radio_range_refresh());native_reply(range_native_id,false,0);writes=write_count;assert(mesh_radio_range_start(MESH_RANGE_SEND,0,0,60,300)&&write_count==writes);
 fake_time+=4999999;range_poll(fake_time);assert(write_count==writes);fake_time++;range_poll(fake_time);assert(write_count==writes+1&&range_status.queued==1);
 protobuf_field_t packet=get(sent_bytes+4,sent_size-4,1),data=get(packet.value,packet.length,4),payload=get(data.value,data.length,2);uint32_t number;memcpy(&number,get(packet.value,packet.length,2).value,4);
 assert(number==MESH_BROADCAST_NODE&&get(packet.value,packet.length,9).integer==0&&get(packet.value,packet.length,10).integer==0&&get(packet.value,packet.length,11).integer==10);
 assert(get(data.value,data.length,1).integer==66&&payload.length==5&&!memcmp(payload.value,"seq 1",5));assert(!manage_field(data.value,data.length,3,&payload));assert(message_count==0);
 fake_time+=180000000;range_poll(fake_time);assert(write_count==writes+2&&range_status.queued==2); // no catch-up burst
 short_write=true;fake_time+=60000000;range_poll(fake_time);short_write=false;assert(range_status.write_failures==1&&range_status.sequence==3);
 fake_time+=60000000;range_poll(fake_time);assert(range_status.state==MESH_RANGE_FINISHED&&write_count==writes+3);
 range_setup();assert(mesh_radio_range_refresh());native_reply(range_native_id,false,0);assert(mesh_radio_range_start(MESH_RANGE_SEND,1,0,30,300));
 local_activity=(mesh_device_metrics_t){.available=true,.has_channel_use=true,.channel_use=25,.received_us=fake_time};local_activity_node=local_node_number;fake_time+=5000000;range_poll(fake_time);assert(range_status.skipped==1&&!range_status.queued&&!range_status.sequence&&range_status.last.event==6);
 status.reconnecting=true;fake_time+=30000000;range_poll(fake_time);assert(range_status.skipped==2&&range_status.last.event==8);status.reconnecting=false;local_activity.channel_use=0;fake_time+=30000000;range_poll(fake_time);assert(range_status.queued==1&&range_status.sequence==1);
 mesh_radio_range_stop();writes=write_count;fake_time+=30000000;range_poll(fake_time);assert(write_count==writes&&range_status.state==MESH_RANGE_STOPPED);
 range_setup();assert(mesh_radio_range_start(MESH_RANGE_RECEIVE,1,0,60,900));range_packet(peer,10,100,0,false,0,true);range_packet(peer,11,100,1,true,0,true);range_packet(peer,12,100,1,false,1,true);assert(!range_status.peer&&!range_status.records);
 range_packet(peer,13,100,1,false,0,true);assert(range_status.peer==peer&&range_status.counter.received==1&&range_status.last.has_snr&&range_status.last.snr==0&&range_status.last.rssi==-77);
 range_packet(peer+1,14,101,1,false,0,true);assert(range_status.counter.received==1);
 range_packet(peer,15,103,1,false,0,false);assert(range_status.counter.received==2&&range_status.counter.gaps==2&&!range_status.last.has_snr&&!range_status.last.has_rssi);
 range_packet(peer,16,102,1,false,0,true);assert(range_status.counter.gaps==1&&range_status.counter.late==1);range_packet(peer,16,102,1,false,0,true);assert(range_status.counter.duplicates==1&&range_status.counter.received==3);
 range_packet(peer,17,1,1,false,0,true);assert(range_status.counter.segment==2&&range_status.counter.gaps==1);
 for(unsigned i=0;i<600;i++)range_packet(peer,100+i,200+i,1,false,0,false);assert(range_status.records==512&&range_status.records_overwritten>0);mesh_range_record_t records[512];mesh_range_status_t snapshot;assert(mesh_radio_range_snapshot(records,512,&snapshot)==512&&records[511].sequence==799&&snapshot.counter.received==range_status.counter.received);
 writes=write_count;assert(!mesh_radio_range_start(MESH_RANGE_RECEIVE,0,peer,60,900)&&write_count==writes);local_node_number++;range_poll(fake_time);assert(range_status.state==MESH_RANGE_INTERRUPTED);
 range_setup();assert(mesh_radio_range_start(MESH_RANGE_RECEIVE,0,peer,60,300));range_packet(peer+1,7,1,0,false,0,true);assert(!range_status.records);range_packet(peer,8,1,0,false,0,true);assert(range_status.records==1);fake_time+=300000000;range_poll(fake_time);assert(range_status.state==MESH_RANGE_FINISHED);
 puts("PASS: official range wire format, direct/no-ACK/background broadcasts, native-sender conflict/freshness check, bounded scheduling/no catch-up, busy/offline/partial UART handling, peer/channel/MQTT/relay filters, sequence counting, raw signal presence, bounded snapshots and timer/identity stops");return 0;
}
