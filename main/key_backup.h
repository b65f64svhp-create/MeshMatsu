#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MESH_KEY_PASSWORD_MAX 96
#define MESH_KEY_FILE_MAX 640
#define MESH_KEY_FILE_LIMIT 32
#define MESH_KEY_PATH_MAX 128
#define MESH_KEY_ROUNDS 600000u
// A key backup intentionally excludes names/settings changes, roles, LoRa and admin policy.
typedef struct { char name[12]; uint8_t role,length,key[32]; } mesh_backup_channel_t;
typedef struct {
    uint32_t node,timestamp;
    char name[40];
    bool identity;
    uint8_t public_key[32],private_key[32],channel_mask;
    mesh_backup_channel_t channel[8];
} mesh_key_backup_t;
static inline void mesh_keys_wipe(void *p,size_t size) { volatile unsigned char *v=p; while(size--) *v++=0; }
bool mesh_keys_identity_valid(const uint8_t public_key[32],const uint8_t private_key[32]);
bool mesh_keys_fingerprint(const uint8_t public_key[32],char out[17]);
bool mesh_keys_seal(const mesh_key_backup_t *backup,const char *password,const uint8_t salt[16],const uint8_t nonce[12],uint8_t *out,size_t *size);
bool mesh_keys_open(const uint8_t *file,size_t size,const char *password,mesh_key_backup_t *out);
// Worker performs only SD/cryptography. Main UI remains the sole radio owner.
typedef enum { MESH_KEY_JOB_NONE,MESH_KEY_JOB_LIST,MESH_KEY_JOB_SAVE,MESH_KEY_JOB_OPEN } mesh_key_job_t;
typedef struct {
    mesh_key_job_t job;
    bool ok;
    char path[MESH_KEY_PATH_MAX];
    char files[MESH_KEY_FILE_LIMIT][64];
    size_t count;
    mesh_key_backup_t backup;
} mesh_key_result_t;
bool mesh_key_store_start(mesh_key_job_t job,const mesh_key_backup_t *backup,const char *password,const char *path,bool safety);
bool mesh_key_store_busy(void);
bool mesh_key_store_take(mesh_key_result_t *result);
