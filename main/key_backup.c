#include "key_backup.h"
#include <string.h>
#include "psa/crypto.h"
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#define PAYLOAD_SIZE 486
#define HEADER_SIZE 44
#define FILE_SIZE (HEADER_SIZE+PAYLOAD_SIZE+16)
_Static_assert(FILE_SIZE<=MESH_KEY_FILE_MAX,"Backup buffer too small");
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
bool mesh_keys_identity_valid(const uint8_t pub[32],const uint8_t priv[32]) {
    unsigned nonzero=0;for(unsigned i=0;i<32;i++)nonzero|=priv[i];
    if(!nonzero||psa_crypto_init()!=PSA_SUCCESS)return false;
    psa_key_attributes_t a=PSA_KEY_ATTRIBUTES_INIT;mbedtls_svc_key_id_t k=0;
    psa_set_key_type(&a,PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_MONTGOMERY));
    psa_set_key_bits(&a,255);psa_set_key_algorithm(&a,PSA_ALG_ECDH);psa_set_key_usage_flags(&a,PSA_KEY_USAGE_DERIVE);
    uint8_t derived[32]={0};size_t n=0;
    bool ok=psa_import_key(&a,priv,32,&k)==PSA_SUCCESS&&psa_export_public_key(k,derived,sizeof(derived),&n)==PSA_SUCCESS&&n==32;
    unsigned diff=0;for(unsigned i=0;i<32;i++)diff|=derived[i]^pub[i];
    if(k)psa_destroy_key(k);
    mesh_keys_wipe(derived,sizeof(derived));psa_reset_key_attributes(&a);return ok&&!diff;
}
bool mesh_keys_fingerprint(const uint8_t pub[32],char out[17]) {
    uint8_t hash[32];size_t n=0;static const char hex[]="0123456789ABCDEF";
    if(psa_crypto_init()!=PSA_SUCCESS||psa_hash_compute(PSA_ALG_SHA_256,pub,32,hash,sizeof(hash),&n)!=PSA_SUCCESS||n!=32)return false;
    for(unsigned i=0;i<8;i++){out[2*i]=hex[hash[i]>>4];out[2*i+1]=hex[hash[i]&15];}out[16]=0;return true;
}
static bool valid_record(const mesh_key_backup_t *b) {
    if(!b||!b->node||!memchr(b->name,0,sizeof(b->name))||(!b->identity&&!b->channel_mask))return false;
    if(b->identity&&!mesh_keys_identity_valid(b->public_key,b->private_key))return false;
    for(unsigned i=0;i<8;i++)if(b->channel_mask&(1u<<i)){
        const mesh_backup_channel_t *c=&b->channel[i];
        if(!memchr(c->name,0,sizeof(c->name))||!c->role||c->role>2||
            (c->length!=0&&c->length!=1&&c->length!=16&&c->length!=32))return false;
        // Exported secondary keys are materialized; zero length is allowed only for primary/no encryption.
        if(c->role==2&&!c->length)return false;
    }
    return true;
}
static bool pack(const mesh_key_backup_t *b,uint8_t out[PAYLOAD_SIZE]) {
    if(!valid_record(b))return false;
    memset(out,0,PAYLOAD_SIZE);memcpy(out,"KEY1",4);put32(out+4,b->node);put32(out+8,b->timestamp);
    memcpy(out+12,b->name,40);out[52]=b->identity;
    if(b->identity){memcpy(out+53,b->public_key,32);memcpy(out+85,b->private_key,32);}out[117]=b->channel_mask;
    for(unsigned i=0;i<8;i++)if(b->channel_mask&(1u<<i)){
        uint8_t *p=out+118+i*46;const mesh_backup_channel_t *c=&b->channel[i];
        memcpy(p,c->name,12);p[12]=c->role;p[13]=c->length;memcpy(p+14,c->key,c->length);
    }
    return true;
}
static bool unpack(const uint8_t in[PAYLOAD_SIZE],mesh_key_backup_t *b) {
    memset(b,0,sizeof(*b));if(memcmp(in,"KEY1",4)||in[52]>1)return false;
    b->node=get32(in+4);b->timestamp=get32(in+8);memcpy(b->name,in+12,40);b->identity=in[52];
    memcpy(b->public_key,in+53,32);memcpy(b->private_key,in+85,32);b->channel_mask=in[117];
    for(unsigned i=0;i<8;i++){
        const uint8_t *p=in+118+i*46;mesh_backup_channel_t *c=&b->channel[i];
        memcpy(c->name,p,12);c->role=p[12];c->length=p[13];memcpy(c->key,p+14,32);
    }
    return valid_record(b);
}
// PBKDF2-HMAC-SHA256, one 32-byte block. Yield during the work so radio/UI tasks
// and the idle watchdog continue to run; cryptographic primitives come from PSA.
static bool derive(const char *password,const uint8_t salt[16],uint8_t out[32]) {
    size_t length=password?strlen(password):0;if(length<12||length>MESH_KEY_PASSWORD_MAX)return false;
    psa_key_attributes_t a=PSA_KEY_ATTRIBUTES_INIT;mbedtls_svc_key_id_t key=0;
    psa_set_key_type(&a,PSA_KEY_TYPE_HMAC);psa_set_key_algorithm(&a,PSA_ALG_HMAC(PSA_ALG_SHA_256));psa_set_key_usage_flags(&a,PSA_KEY_USAGE_SIGN_MESSAGE);
    if(psa_import_key(&a,(const uint8_t*)password,length,&key)!=PSA_SUCCESS)return false;
    uint8_t first[20],u[32]={0},next[32]={0};size_t n=0;memcpy(first,salt,16);memset(first+16,0,4);first[19]=1;
    bool ok=psa_mac_compute(key,PSA_ALG_HMAC(PSA_ALG_SHA_256),first,20,u,sizeof(u),&n)==PSA_SUCCESS&&n==32;
    memcpy(out,u,32);
    for(uint32_t i=1;ok&&i<MESH_KEY_ROUNDS;i++){
        ok=psa_mac_compute(key,PSA_ALG_HMAC(PSA_ALG_SHA_256),u,32,next,sizeof(next),&n)==PSA_SUCCESS&&n==32;
        for(unsigned j=0;j<32;j++){u[j]=next[j];out[j]^=next[j];}
#ifdef ESP_PLATFORM
        if((i&1023)==0)vTaskDelay(1);
#endif
    }
    psa_destroy_key(key);psa_reset_key_attributes(&a);mesh_keys_wipe(u,sizeof(u));mesh_keys_wipe(next,sizeof(next));return ok;
}
static bool crypt(bool encrypt,const char *password,const uint8_t *header,const uint8_t *input,size_t size,uint8_t *out,size_t cap,size_t *n) {
    if(psa_crypto_init()!=PSA_SUCCESS)return false;
    uint8_t derived[32]={0};bool ok=derive(password,header+12,derived);
    psa_key_attributes_t a=PSA_KEY_ATTRIBUTES_INIT;mbedtls_svc_key_id_t key=0;
    psa_set_key_type(&a,PSA_KEY_TYPE_AES);psa_set_key_bits(&a,256);psa_set_key_algorithm(&a,PSA_ALG_GCM);
    psa_set_key_usage_flags(&a,encrypt?PSA_KEY_USAGE_ENCRYPT:PSA_KEY_USAGE_DECRYPT);
    if(ok)ok=psa_import_key(&a,derived,32,&key)==PSA_SUCCESS;
    if(ok)ok=(encrypt?psa_aead_encrypt(key,PSA_ALG_GCM,header+28,12,header,HEADER_SIZE,input,size,out,cap,n):
        psa_aead_decrypt(key,PSA_ALG_GCM,header+28,12,header,HEADER_SIZE,input,size,out,cap,n))==PSA_SUCCESS;
    if(key)psa_destroy_key(key);
    psa_reset_key_attributes(&a);mesh_keys_wipe(derived,sizeof(derived));return ok;
}
bool mesh_keys_seal(const mesh_key_backup_t *backup,const char *password,const uint8_t salt[16],const uint8_t nonce[12],uint8_t *out,size_t *size) {
    if(!out||!size||*size<FILE_SIZE||!salt||!nonce)return false;
    uint8_t payload[PAYLOAD_SIZE];if(!pack(backup,payload))return false;
    memcpy(out,"MMKEY001",8);put32(out+8,MESH_KEY_ROUNDS);memcpy(out+12,salt,16);memcpy(out+28,nonce,12);put32(out+40,PAYLOAD_SIZE);
    size_t n=0;bool ok=crypt(true,password,out,payload,sizeof(payload),out+HEADER_SIZE,*size-HEADER_SIZE,&n)&&n==PAYLOAD_SIZE+16;
    mesh_keys_wipe(payload,sizeof(payload));if(ok)*size=HEADER_SIZE+n;else mesh_keys_wipe(out,*size);return ok;
}
bool mesh_keys_open(const uint8_t *file,size_t size,const char *password,mesh_key_backup_t *out) {
    if(!out)return false;
    memset(out,0,sizeof(*out));
    if(!file||size!=FILE_SIZE||memcmp(file,"MMKEY001",8)||get32(file+8)!=MESH_KEY_ROUNDS||get32(file+40)!=PAYLOAD_SIZE)return false;
    uint8_t plain[PAYLOAD_SIZE]={0};size_t n=0;
    bool ok=crypt(false,password,file,file+HEADER_SIZE,size-HEADER_SIZE,plain,sizeof(plain),&n)&&n==PAYLOAD_SIZE&&unpack(plain,out);
    mesh_keys_wipe(plain,sizeof(plain));if(!ok)mesh_keys_wipe(out,sizeof(*out));return ok;
}

// Configuration files use a different authenticated purpose marker and variable length.
bool mesh_config_seal(const uint8_t *plain,size_t size,const char *password,const uint8_t salt[16],const uint8_t nonce[12],uint8_t *out,size_t *out_size){
    if(!plain||!size||size>220000||!salt||!nonce||!out||!out_size||*out_size<size+60)return false;
    memcpy(out,"MMCFG001",8);put32(out+8,MESH_KEY_ROUNDS);memcpy(out+12,salt,16);memcpy(out+28,nonce,12);put32(out+40,size);
    size_t n=0;bool ok=crypt(true,password,out,plain,size,out+44,*out_size-44,&n)&&n==size+16;
    if(ok)*out_size=44+n;else mesh_keys_wipe(out,*out_size);return ok;
}
bool mesh_config_open(const uint8_t *file,size_t size,const char *password,uint8_t *out,size_t *out_size){
    if(!out||!out_size)return false;
    if(!file||size<61||size>220060||memcmp(file,"MMCFG001",8)||get32(file+8)!=MESH_KEY_ROUNDS||get32(file+40)!=size-60||*out_size<size-60){mesh_keys_wipe(out,*out_size);return false;}
    size_t n=0;bool ok=crypt(false,password,file,file+44,size-44,out,*out_size,&n)&&n==size-60;
    if(ok)*out_size=n;else mesh_keys_wipe(out,*out_size);return ok;
}
