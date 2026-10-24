#include "config_backup.h"
#include <string.h>
#include <stdio.h>
static uint32_t rd32(const uint8_t *p){
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void wr32(uint8_t *p,uint32_t x){
    for(unsigned i=0;i<4;i++)p[i]=x>>(8*i);
}
static bool record_valid(const mesh_cfg_record_t *r){
    if(!r||!r->size||r->size>MESH_CFG_DATA)return false;
    if(r->kind==CFG_RADIO)return r->id>=1&&r->id<=8&&r->size<=1024;
    if(r->kind==CFG_MODULE)return r->id>=1&&r->id<=13&&r->size<=1024;
    if(r->kind==CFG_CHANNEL)return r->id<8&&r->size<=192;
    return r->kind>=CFG_OWNER&&r->kind<=CFG_FIXED&&!r->id;
}
static bool archive_valid(const mesh_config_backup_t *a){
    if(!a||!a->node||a->node==UINT32_MAX||!a->count||a->count>MESH_CFG_RECORDS||!a->firmware[0]||!memchr(a->firmware,0,64)||!memchr(a->name,0,40))return false;
    for(unsigned i=0;i<a->count;i++){
        if(!record_valid(&a->records[i]))return false;
        for(unsigned j=0;j<i;j++)if(a->records[j].kind==a->records[i].kind&&a->records[j].id==a->records[i].id)return false;
    }
    return true;
}
bool mesh_config_pack(const mesh_config_backup_t *a,uint8_t *out,size_t *size){
    if(!out||!size||!archive_valid(a))return false;
    size_t n=122;
    for(unsigned i=0;i<a->count;i++)n+=4+a->records[i].size;
    if(n>*size||n>MESH_CFG_WIRE_MAX)return false;
    memcpy(out,"CFG1",4);
    wr32(out+4,a->node);
    wr32(out+8,a->timestamp);
    wr32(out+12,a->hardware);
    memcpy(out+16,a->firmware,64);
    memcpy(out+80,a->name,40);
    out[120]=a->count;
    out[121]=a->count>>8;
    n=122;
    for(unsigned i=0;i<a->count;i++){
        const mesh_cfg_record_t *r=&a->records[i];
        out[n++]=r->kind;
        out[n++]=r->id;
        out[n++]=r->size;
        out[n++]=r->size>>8;
        memcpy(out+n,r->data,r->size);
        n+=r->size;
    }
    *size=n;
    return true;
}
bool mesh_config_unpack(const uint8_t *data,size_t size,mesh_config_backup_t *out){
    if(!out)return false;
    memset(out,0,sizeof(*out));
    if(!data||size<122||size>MESH_CFG_WIRE_MAX||memcmp(data,"CFG1",4))return false;
    out->node=rd32(data+4);
    out->timestamp=rd32(data+8);
    out->hardware=rd32(data+12);
    memcpy(out->firmware,data+16,64);
    memcpy(out->name,data+80,40);
    unsigned count=data[120]|data[121]<<8;
    size_t p=122;
    if(count>MESH_CFG_RECORDS)return false;
    for(unsigned i=0;i<count;i++){
        if(size-p<4)goto fail;
        unsigned kind=data[p++],id=data[p++];
        size_t n=data[p]|data[p+1]<<8;
        p+=2;
        if(n>size-p||!mesh_config_add(out,kind,id,data+p,n))goto fail;
        p+=n;
    }
    if(p==size&&archive_valid(out))return true;
    fail:mesh_keys_wipe(out,sizeof(*out));
    return false;
}
const char *mesh_config_label(const mesh_cfg_record_t *r,char *out,size_t size){
    static const char *config[]={
        "","Device / role","Position","Radio power","Network","Radio display","LoRa / region","Bluetooth","Security / identity keys"
    }
    ;
    static const char *module[]={
        "","MQTT","Serial","External notification","Store & Forward","Range Test","Telemetry","Canned Messages","Audio","Remote Hardware","Neighbor Info","Ambient lighting","Detection Sensor","Paxcounter"
    }
    ;
    if(r->kind==CFG_RADIO&&r->id<=8)snprintf(out,size,"Radio: %s",config[r->id]);
    else if(r->kind==CFG_MODULE&&r->id<=13)snprintf(out,size,"Module: %s",module[r->id]);
    else if(r->kind==CFG_CHANNEL)snprintf(out,size,"Channel slot %u (name, role, key, options)",r->id);
    else snprintf(out,size,"%s",r->kind==CFG_OWNER?"Radio owner / names / licensed status":r->kind==CFG_APP?"App display, alerts, sound, time and units":r->kind==CFG_ALIASES?"Local node nicknames":r->kind==CFG_MUTES?"Per-node notification mutes":r->kind==CFG_FIXED?"Saved fixed coordinates (latitude / longitude)":"Message history capacity");
    return out;
}
