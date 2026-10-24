#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "key_backup.h"
#define MESH_CFG_RECORDS 35
#define MESH_CFG_DATA 6144
#define MESH_CFG_RADIO_MAX 211 // 233-byte Admin payload minus wrapper and up to 16-byte session passkey.
#define MESH_CFG_WIRE_MAX 220000
// IDs: owner=0; config/module=protobuf oneof tag; channel=slot; local=0.
enum {
    CFG_OWNER=1,CFG_RADIO,CFG_MODULE,CFG_CHANNEL,CFG_APP,CFG_ALIASES,CFG_MUTES,CFG_HISTORY,CFG_FIXED
}
;
typedef struct {
    uint8_t kind,id;
    uint16_t size;
    uint8_t data[MESH_CFG_DATA];
}
mesh_cfg_record_t;
typedef struct {
    uint32_t node,timestamp,hardware;
    char firmware[64],name[40];
    uint16_t count;
    mesh_cfg_record_t records[MESH_CFG_RECORDS];
}
mesh_config_backup_t;
static inline const mesh_cfg_record_t *mesh_config_find(const mesh_config_backup_t *a,unsigned kind,unsigned id){
    if(!a||a->count>MESH_CFG_RECORDS)return NULL;
    for(unsigned i=0;i<a->count;i++)if(a->records[i].kind==kind&&a->records[i].id==id)return &a->records[i];
    return NULL;
}
static inline bool mesh_config_add(mesh_config_backup_t *a,unsigned kind,unsigned id,const void *data,size_t size){
    if(!a||a->count>=MESH_CFG_RECORDS||!data||!size||size>MESH_CFG_DATA||kind>255||id>255||mesh_config_find(a,kind,id))return false;
    mesh_cfg_record_t *r=&a->records[a->count];
    r->kind=kind;
    r->id=id;
    r->size=size;
    memcpy(r->data,data,size);
    a->count++;
    return true;
}
bool mesh_config_pack(const mesh_config_backup_t *a,uint8_t *out,size_t *size);
bool mesh_config_unpack(const uint8_t *data,size_t size,mesh_config_backup_t *out);
const char *mesh_config_label(const mesh_cfg_record_t *r,char *out,size_t size);
bool mesh_config_app_capture(uint8_t *out,size_t *size);
bool mesh_config_app_apply(const uint8_t *data,size_t size);
// Purpose-specific authenticated envelope; existing .mmk format stays unchanged.
bool mesh_config_seal(const uint8_t *plain,size_t size,const char *password,const uint8_t salt[16],const uint8_t nonce[12],uint8_t *out,size_t *out_size);
bool mesh_config_open(const uint8_t *file,size_t size,const char *password,uint8_t *out,size_t *out_size);
typedef enum {
    CFG_JOB_LIST=1,CFG_JOB_SAVE,CFG_JOB_OPEN
}
mesh_cfg_job_t;
typedef struct {
    mesh_cfg_job_t job;
    bool ok;
    size_t count;
    char path[64],files[32][64];
    mesh_config_backup_t archive;
}
mesh_cfg_result_t;
bool mesh_config_store_start(mesh_cfg_job_t job,const mesh_config_backup_t *archive,const char *password,const char *path,bool safety);
bool mesh_config_store_busy(void);
bool mesh_config_store_take(mesh_cfg_result_t *out);
typedef enum {
    CFG_IDLE,CFG_READING,CFG_READY,CFG_SAVING,CFG_SAVED,CFG_FAILED,CFG_TIMEOUT
}
mesh_cfg_state_t;
bool mesh_radio_config_capture(void);
mesh_cfg_state_t mesh_radio_config_state(void);
bool mesh_radio_config_snapshot(mesh_config_backup_t *out);
bool mesh_radio_config_restore(const mesh_config_backup_t *saved,const mesh_cfg_record_t *record);
void mesh_radio_config_clear(void);
void mesh_radio_config_identity_validator(bool (*check)(const uint8_t *,const uint8_t *));
