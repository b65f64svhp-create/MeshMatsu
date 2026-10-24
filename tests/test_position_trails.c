#define main existing_radio_tests
#include "test_direct_messages.c"
#undef main
static size_t fix(uint8_t *p,int32_t lat,int32_t lon,uint32_t stamp){size_t n=0;p[n++]=13;memcpy(p+n,&lat,4);n+=4;p[n++]=21;memcpy(p+n,&lon,4);n+=4;p[n++]=61;memcpy(p+n,&stamp,4);return n+4;}
static void update(uint32_t node,uint32_t dest,const uint8_t *p,size_t length){uint8_t data[100],packet[140];size_t d=0,n=0;assert(append_varint(data,sizeof(data),&d,8)&&append_varint(data,sizeof(data),&d,3));assert(append_bytes_field(data,sizeof(data),&d,2,p,length));packet[n++]=13;memcpy(packet+n,&node,4);n+=4;packet[n++]=21;memcpy(packet+n,&dest,4);n+=4;assert(append_bytes_field(packet,sizeof(packet),&n,4,data,d));decode_mesh_packet(packet,n);}
static void snapshot(uint32_t id,const uint8_t *p,size_t length,bool ignored){uint8_t raw[120];size_t n=0;assert(append_varint(raw,sizeof(raw),&n,8)&&append_varint(raw,sizeof(raw),&n,id));assert(append_bytes_field(raw,sizeof(raw),&n,3,p,length));if(ignored){assert(append_varint(raw,sizeof(raw),&n,88)&&append_varint(raw,sizeof(raw),&n,1));}decode_node(raw,n);}
int main(void){reset();uint32_t now=current_message_timestamp();assert(now>=1700000000);uint8_t p[40];mesh_trail_point_t out[64];
 size_t n=fix(p,550000000,-1180000000,now-600);snapshot(42,p,n,false);snapshot(42,p,n,false);assert(mesh_trails_series(42,now,out,64)==1);
 n=fix(p,550001000,-1180000000,now-500);update(42,MESH_BROADCAST_NODE,p,n);assert(mesh_trails_series(42,now,out,64)==2&&mesh_trails_join(&out[0],&out[1],now));
 n=fix(p,550000500,-1180000000,now-550);snapshot(42,p,n,false);assert(mesh_trails_series(42,now,out,64)==2&&out[1].time==now-500);
 const uint8_t partial[]={8,42};decode_node(partial,sizeof(partial));assert(mesh_trails_series(42,now,out,64)==2);
 n=fix(p,0,0,0);update(43,MESH_BROADCAST_NODE,p,n);mesh_node_t node;assert(mesh_radio_get_node_by_number(43,&node)&&node.position_available&&!mesh_trails_series(43,now,out,64));
 n=fix(p,900000001,0,now);update(44,MESH_BROADCAST_NODE,p,n);assert(mesh_radio_get_node_by_number(44,&node)&&!node.position_available&&!mesh_trails_series(44,now,out,64));
 n=fix(p,0,0,now-400);update(local_node_number,MESH_BROADCAST_NODE,p,n);n=fix(p,0,1000,now-300);update(local_node_number,MESH_BROADCAST_NODE,p,n);assert(mesh_trails_series(local_node_number,now,out,64)==2);
 n=fix(p,0,0,now);update(45,0xabcdef01,p,n);assert(mesh_radio_get_node_by_number(45,&node)&&!node.position_available&&!mesh_trails_series(45,now,out,64));
 n=fix(p,550002000,-1180000000,now-400);snapshot(42,p,n,true);assert(!mesh_trails_series(42,now,out,64));update(42,MESH_BROADCAST_NODE,p,n);assert(!mesh_trails_series(42,now,out,64));
 snapshot(46,p,n,true);assert(!mesh_trails_series(46,now,out,64));
 begin_radio_reconnect();assert(mesh_trails_series(local_node_number,now,out,64)==2);assert(!message_count&&!reaction_count&&!message_alerts&&!write_count&&!mesh_radio_unread_total(true)&&!mesh_radio_unread_total(false));
 reset();assert(!mesh_trails_count());puts("PASS: actual NodeInfo/live/local position recording, repeat/partial/stale cache protection, undated/invalid/private/ignored exclusions, ignore clearing, reconnect retention/reset, no messages/unread/alerts or transmissions");}
