#define main earlier_direct_tests
#include "test_direct_messages.c"
#undef main
static const uint32_t peer=0xaabbccdd,other=0x33445566;
static void nb_setup(void){reset();neighbor_reset();}
static size_t nb_payload(uint8_t *out,uint32_t origin,unsigned count,float snr){
 size_t n=0;assert(append_varint(out,233,&n,8)&&append_varint(out,233,&n,origin));assert(append_varint(out,233,&n,16)&&append_varint(out,233,&n,other));assert(append_varint(out,233,&n,24)&&append_varint(out,233,&n,21600));
 for(unsigned i=0;i<count;i++){uint8_t child[32];size_t c=0;assert(append_varint(child,sizeof(child),&c,8)&&append_varint(child,sizeof(child),&c,0x500+i));child[c++]=0x15;memcpy(child+c,&snr,4);c+=4;assert(append_bytes_field(out,233,&n,4,child,c));}return n;
}
static void nb_packet(uint32_t from,uint32_t to,uint32_t packet,uint32_t request,const uint8_t *payload,size_t n,bool mqtt,uint32_t stamp){
 uint8_t data[260],mesh[340];size_t d=0,p=0;
 assert(append_varint(data,sizeof(data),&d,8)&&append_varint(data,sizeof(data),&d,71)&&append_bytes_field(data,sizeof(data),&d,2,payload,n));
 if(request){data[d++]=0x35;memcpy(data+d,&request,4);d+=4;}
 mesh[p++]=0x0d;memcpy(mesh+p,&from,4);p+=4;mesh[p++]=0x15;memcpy(mesh+p,&to,4);p+=4;
 assert(append_bytes_field(mesh,sizeof(mesh),&p,4,data,d));mesh[p++]=0x35;memcpy(mesh+p,&packet,4);p+=4;
 mesh[p++]=0x3d;memcpy(mesh+p,&stamp,4);p+=4;float relay_snr=99;mesh[p++]=0x45;memcpy(mesh+p,&relay_snr,4);p+=4;
 mesh[p++]=0x48;mesh[p++]=1;mesh[p++]=0x78;mesh[p++]=3; // relayed: never substitute packet SNR as an edge
 if(mqtt){mesh[p++]=0x70;mesh[p++]=1;}decode_mesh_packet(mesh,p);
}

#ifndef NB_REPORT_TEST_MAIN
#define NB_REPORT_TEST_MAIN main
#endif
int NB_REPORT_TEST_MAIN(void){
 nb_setup();uint8_t payload[233];size_t n=nb_payload(payload,peer,10,-7.5);size_t writes=write_count;uint32_t revision=history_revision;
 nb_packet(peer,MESH_BROADCAST_NODE,10,0,payload,n,true,1800000000);
 mesh_neighbor_report_t r;assert(mesh_radio_get_neighbors(peer,&r)&&r.count==10&&r.last_sent_by==other&&r.via_mqtt&&r.packet==10&&r.timestamp==1800000000&&r.neighbors[0].snr==-7.5&&r.neighbors[0].has_snr);
 assert(write_count==writes&&!message_count&&!reaction_count&&!message_alerts&&history_revision>revision);
 int64_t received=r.received_us;fake_time+=1000000;nb_packet(peer,MESH_BROADCAST_NODE,10,0,payload,n,false,1800000001);assert(mesh_radio_get_neighbors(peer,&r)&&r.received_us==received);
 nb_packet(peer,MESH_BROADCAST_NODE,11,0,payload,n,false,1799999999);assert(mesh_radio_get_neighbors(peer,&r)&&r.packet==10);
 // Wrong owner, unknown IDs/self edge, duplicate neighbors, overflow/truncation,
 // non-finite SNR and a request dummy cannot replace the verified parsed report.
 size_t wrong=nb_payload(payload,other,1,1);nb_packet(peer,MESH_BROADCAST_NODE,12,0,payload,wrong,false,1800000002);assert(mesh_radio_get_neighbors(peer,&r)&&r.count==10);
 n=nb_payload(payload,peer,1,NAN);nb_packet(peer,MESH_BROADCAST_NODE,12,0,payload,n,false,1800000002);assert(mesh_radio_get_neighbors(peer,&r)&&r.count==10);
 n=nb_payload(payload,peer,1,0);payload[n++]=0x22;payload[n++]=0;nb_packet(peer,MESH_BROADCAST_NODE,12,0,payload,n,false,1800000002);assert(mesh_radio_get_neighbors(peer,&r)&&r.count==10);
 n=nb_payload(payload,peer,11,1);nb_packet(peer,MESH_BROADCAST_NODE,12,0,payload,n,false,1800000002);assert(mesh_radio_get_neighbors(peer,&r)&&r.count==10);
 n=nb_payload(payload,peer,1,0);nb_packet(peer,other,12,0,payload,n,false,1800000002);assert(mesh_radio_get_neighbors(peer,&r)&&r.count==10);
 // An omitted proto3 zero SNR is distinguishable only as "not sent".
 n=0;uint8_t child[]={8,55};assert(append_varint(payload,sizeof(payload),&n,8)&&append_varint(payload,sizeof(payload),&n,peer)&&append_bytes_field(payload,sizeof(payload),&n,4,child,sizeof(child)));
 nb_packet(peer,MESH_BROADCAST_NODE,13,0,payload,n,false,1800000002);assert(mesh_radio_get_neighbors(peer,&r)&&r.count==1&&!r.neighbors[0].has_snr);
 // One explicit request. ACK does not satisfy it; only correlated report data.
 assert(mesh_radio_request_neighbors(peer)==MESH_NB_START_OK&&write_count==writes+1);uint32_t id=nb_request.id;
 ack(local_node_number,id,0);assert(nb_request.state==MESH_NB_WAITING);assert(mesh_radio_request_neighbors(peer)==MESH_NB_START_BUSY);
 n=nb_payload(payload,peer,0,0);nb_packet(peer,local_node_number,20,id+1,payload,n,false,1800000002);assert(nb_request.state==MESH_NB_WAITING);
 nb_packet(peer,local_node_number,21,id,payload,n,false,1800000003);assert(nb_request.state==MESH_NB_REPLY&&mesh_radio_get_neighbors(peer,&r)&&!r.count);
 assert(mesh_radio_request_neighbors(peer)==MESH_NB_START_COOLDOWN&&mesh_radio_neighbor_cooldown()>0);fake_time+=180000000;assert(!mesh_radio_neighbor_cooldown());
 assert(mesh_radio_request_neighbors(peer)==MESH_NB_START_OK);id=nb_request.id;ack(local_node_number,id,9);assert(nb_request.state==MESH_NB_FAILED&&nb_request.error==9);
 fake_time+=180000000;short_write=true;assert(mesh_radio_request_neighbors(peer)==MESH_NB_START_WRITE_FAILED);short_write=false;
 assert(mesh_radio_request_neighbors(local_node_number)==MESH_NB_START_OK);id=nb_request.id;n=nb_payload(payload,local_node_number,1,0);nb_packet(local_node_number,local_node_number,100,id,payload,n,false,1800000010);assert(nb_request.state==MESH_NB_REPLY&&mesh_radio_get_neighbors(local_node_number,&r)&&r.neighbors[0].has_snr&&!r.neighbors[0].snr);
 fake_time+=180000000;assert(mesh_radio_request_neighbors(peer)==MESH_NB_START_OK);fake_time+=120000000;neighbor_poll(fake_time);assert(nb_request.state==MESH_NB_TIMEOUT);
 fake_time+=60000000;assert(mesh_radio_request_neighbors(peer)==MESH_NB_START_OK);begin_radio_reconnect();assert(nb_request.state==MESH_NB_INTERRUPTED&&mesh_radio_get_neighbors(peer,&r));
 // Cache is bounded, favors retaining our own report, and does not invent nodes
 // for every listed neighbor. Changing local radio identity clears the cache.
 nb_setup();n=nb_payload(payload,local_node_number,1,1);nb_packet(local_node_number,local_node_number,500,0,payload,n,false,1800001000);
 for(unsigned i=1;i<=40;i++){fake_time+=1000;uint32_t source=0x700+i;n=nb_payload(payload,source,0,0);nb_packet(source,MESH_BROADCAST_NODE,500+i,0,payload,n,false,1800001000+i);}
 unsigned count=0;for(unsigned i=0;i<MESH_NEIGHBOR_REPORTS;i++)if(nb_reports[i].node)count++;assert(count==MESH_NEIGHBOR_REPORTS&&mesh_radio_get_neighbors(local_node_number,&r));
 const uint8_t own[]={8,123};decode_my_info(own,sizeof(own));assert(!mesh_radio_get_neighbors(peer,&r)&&!mesh_radio_get_neighbors(0x700+40,&r)&&nb_origin==123);
 assert(!mesh_radio_get_neighbors(peer,NULL));
 puts("PASS: real NEIGHBORINFO transport, source-scoped bounded parsing/cache, relays/MQTT, absent/zero signal, empty report, dedup/age, manual correlated requests, ACK/NAK, local replies, cooldown/timeout/short UART/reconnect and ownership reset");return 0;
}
