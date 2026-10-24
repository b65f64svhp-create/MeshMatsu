#include "key_backup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 mesh_key_backup_t b={.node=0x12345678,.timestamp=1800000000,.identity=true,.channel_mask=2};strcpy(b.name,"TESTNODE");
 const unsigned char priv[]={0x77,0x07,0x6d,0x0a,0x73,0x18,0xa5,0x7d,0x3c,0x16,0xc1,0x72,0x51,0xb2,0x66,0x45,0xdf,0x4c,0x2f,0x87,0xeb,0xc0,0x99,0x2a,0xb1,0x77,0xfb,0xa5,0x1d,0xb9,0x2c,0x2a};
 const unsigned char pub[]={0x85,0x20,0xf0,0x09,0x89,0x30,0xa7,0x54,0x74,0x8b,0x7d,0xdc,0xb4,0x3e,0xf7,0x5a,0x0d,0xbf,0x3a,0x0d,0x26,0x38,0x1a,0xf4,0xeb,0xa4,0xa9,0x8e,0xaa,0x9b,0x4e,0x6a};
 memcpy(b.private_key,priv,32);memcpy(b.public_key,pub,32);assert(mesh_keys_identity_valid(pub,priv));
 b.channel[1]=(mesh_backup_channel_t){.name="TEST_CHAN",.role=2,.length=32};memset(b.channel[1].key,42,32);
 uint8_t file[640],salt[16]={1},nonce[12]={2};size_t n=sizeof(file);assert(mesh_keys_seal(&b,"a long test passphrase",salt,nonce,file,&n));
 FILE *f=fopen("fixture.mmk","wb");assert(f);assert(fwrite(file,1,n,f)==n);assert(!fclose(f));
 mesh_key_backup_t r;assert(mesh_keys_open(file,n,"a long test passphrase",&r));assert(!memcmp(r.private_key,priv,32)&&r.channel[1].key[0]==42);

 assert(!mesh_keys_open(file,n,"the wrong long password",&r));for(size_t i=0;i<sizeof(r);i++)assert(!((uint8_t*)&r)[i]);
 file[n-1]^=1;assert(!mesh_keys_open(file,n,"a long test passphrase",&r));file[n-1]^=1;
 assert(!mesh_keys_open(file,n-1,"a long test passphrase",&r));
 file[8]^=1;assert(!mesh_keys_open(file,n,"a long test passphrase",&r));file[8]^=1;
 uint8_t mismatched[32]={0};assert(!mesh_keys_identity_valid(mismatched,priv));
 uint8_t zeros[32]={0};assert(!mesh_keys_identity_valid(pub,zeros));
 size_t capacity=sizeof(file);assert(!mesh_keys_seal(&b,"short",salt,nonce,file,&capacity));
 assert(mesh_key_store_start(MESH_KEY_JOB_SAVE,&b,"a long test passphrase",NULL,false));
 assert(mesh_key_store_busy()&&!mesh_key_store_start(MESH_KEY_JOB_LIST,NULL,NULL,NULL,false));
 mesh_key_result_t result;assert(mesh_key_store_take(&result)&&result.ok&&!result.backup.identity);char path[64];strcpy(path,result.path);
 assert(mesh_key_store_start(MESH_KEY_JOB_LIST,NULL,NULL,NULL,false));assert(mesh_key_store_take(&result)&&result.ok&&result.count==1&&!strcmp(result.files[0],path));
 assert(!mesh_key_store_start(MESH_KEY_JOB_OPEN,NULL,"a long test passphrase","../keys-bad.mmk",false));
 assert(mesh_key_store_start(MESH_KEY_JOB_OPEN,NULL,"a long test passphrase",path,false));assert(mesh_key_store_take(&result)&&result.ok&&result.backup.identity&&result.backup.private_key[0]==priv[0]);mesh_keys_wipe(&result,sizeof(result));
 assert(mesh_key_store_start(MESH_KEY_JOB_SAVE,&b,"a long test passphrase",NULL,true));assert(mesh_key_store_take(&result)&&result.ok&&strcmp(result.path,path));
 puts("PASS: actual PSA X25519 validation, PBKDF2/AES-GCM, wrong passphrase/tag/header/truncation rejection, wiped failed output, verified encrypted SD save/list/open/safety and traversal/duplicate-job guards");
}
