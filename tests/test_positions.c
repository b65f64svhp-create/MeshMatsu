#define main backend_tests
#include "test_direct_messages.c"
#undef main
static size_t position(uint8_t *p, int32_t lat, int32_t lon, uint32_t stamp) {
    size_t n=0;p[n++]=0x0d;memcpy(p+n,&lat,4);n+=4;p[n++]=0x15;memcpy(p+n,&lon,4);n+=4;
    p[n++]=0x3d;memcpy(p+n,&stamp,4);n+=4;return n;
}
static void packet(uint32_t sender,const uint8_t *pos,size_t length) {
    uint8_t data[128],packet[180];size_t d=0,n=0;uint32_t id=900;
    assert(append_varint(data,sizeof(data),&d,8)&&append_varint(data,sizeof(data),&d,3));
    assert(append_bytes_field(data,sizeof(data),&d,2,pos,length));
    packet[n++]=0x0d;memcpy(packet+n,&sender,4);n+=4;
    packet[n++]=0x35;memcpy(packet+n,&id,4);n+=4;
    assert(append_bytes_field(packet,sizeof(packet),&n,4,data,d));decode_mesh_packet(packet,n);
}
int main(void) {
    reset();uint8_t p[80];size_t n=position(p,551700000,-1188000000,1800000000);
    // Real received POSITION_APP and local position, with own echo suppression.
    packet(42,p,n);packet(local_node_number,p,n);assert(node_count==2);
    assert(nodes[0].position_available&&nodes[0].latitude_i==551700000&&nodes[0].longitude_i==-1188000000);
    assert(nodes[1].position_available&&nodes[1].number==local_node_number);
    assert(!message_count&&!reaction_count&&!write_count);
    mesh_node_t node={0};
    assert(decode_position(p,n,&node)&&node.position_time==1800000000);
    uint8_t precision[]={0xb8,0x01,16};memcpy(p+n,precision,3);assert(decode_position(p,n+3,&node)&&node.position_precision==16);
    // Stale fixes and partial positions cannot replace a known location.
    n=position(p,0,0,1799999999);assert(!decode_position(p,n,&node)&&node.latitude_i==551700000);
    n=position(p,0,0,1800000001);assert(decode_position(p,n,&node)&&!node.latitude_i&&!node.longitude_i);
    assert(!decode_position(p,5,&node));assert(!decode_position(NULL,0,&node));
    n=position(p,900000001,0,1800000002);assert(!decode_position(p,n,&node));
    n=position(p,0,-1800000001,1800000002);assert(!decode_position(p,n,&node));
    n=position(p,0,0,1800000002);assert(!decode_position(p,n-1,&node));
    const uint8_t wrong_wire[]={8,0,21,0,0,0,0};assert(!decode_position(wrong_wire,sizeof(wrong_wire),&node));
    n=position(p,551700000,-1188000000,1800000003);
    uint8_t snapshot[140];size_t size=0;
    assert(append_varint(snapshot,sizeof(snapshot),&size,8)&&append_varint(snapshot,sizeof(snapshot),&size,42));
    assert(append_bytes_field(snapshot,sizeof(snapshot),&size,3,p,n));decode_node(snapshot,size);
    assert(nodes[0].position_time==1800000003);
    const uint8_t partial[]={8,42};decode_node(partial,sizeof(partial));assert(nodes[0].position_available&&nodes[0].position_time==1800000003);
    n=position(p,0,0,1800000000);size=2;memcpy(snapshot,partial,2);
    assert(append_bytes_field(snapshot,sizeof(snapshot),&size,3,p,n));decode_node(snapshot,size);
    assert(nodes[0].position_time==1800000003&&nodes[0].latitude_i==551700000);
    // Complete explicit equator/prime-meridian position is valid, not absent.
    n=position(p,0,0,0);packet(43,p,n);assert(nodes[2].position_available&&!nodes[2].latitude_i);
    assert(!message_count&&!reaction_count&&!write_count);
    puts("PASS: NodeInfo/POSITION_APP, local fixes, signed coordinates, explicit zero, partial/malformed/range/stale fix rejection, precision and no radio writes");
}
