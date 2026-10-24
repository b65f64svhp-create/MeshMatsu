#include "config_backup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static mesh_config_backup_t a,b;
static mesh_cfg_result_t result;
int main(void){
 a.node=0x12345678;a.timestamp=1800000000;a.hardware=1;strcpy(a.name,"BUL-TEST");strcpy(a.firmware,"2.7.18");uint8_t owner[]={18,4,'T','e','s','t'},aliases[]={1,2,3,4};
 assert(mesh_config_add(&a,CFG_OWNER,0,owner,sizeof(owner))&&mesh_config_add(&a,CFG_ALIASES,0,aliases,sizeof(aliases)));
 uint8_t plain[1000],file[1100],decoded[1000],salt[16]={1},nonce[12]={2};size_t n=sizeof(plain),fn=sizeof(file),dn=sizeof(decoded);assert(mesh_config_pack(&a,plain,&n));
 assert(mesh_config_seal(plain,n,"a long test passphrase",salt,nonce,file,&fn));assert(fn==n+60);
 FILE *f=fopen("fixture.mmc","wb");assert(f&&fwrite(file,1,fn,f)==fn&&!fclose(f));
 assert(mesh_config_open(file,fn,"a long test passphrase",decoded,&dn)&&dn==n&&!memcmp(plain,decoded,n));assert(mesh_config_unpack(decoded,dn,&b)&&b.node==a.node&&b.count==2);
 dn=sizeof(decoded);assert(!mesh_config_open(file,fn,"a different long password",decoded,&dn));for(size_t i=0;i<sizeof(decoded);i++)assert(!decoded[i]);
 file[fn-1]^=1;dn=sizeof(decoded);assert(!mesh_config_open(file,fn,"a long test passphrase",decoded,&dn));file[fn-1]^=1;
 file[8]^=1;assert(!mesh_config_open(file,fn,"a long test passphrase",decoded,&dn));file[8]^=1;
 assert(!mesh_config_open(file,fn-1,"a long test passphrase",decoded,&dn));file[0]^=1;assert(!mesh_config_open(file,fn,"a long test passphrase",decoded,&dn));file[0]^=1;
 assert(!mesh_config_unpack(plain,n-1,&b));plain[120]=35;assert(!mesh_config_unpack(plain,n,&b));plain[120]=2;
 // No silent overwrite, all records survive the actual encrypted SD worker.
 assert(mesh_config_store_start(CFG_JOB_SAVE,&a,"a long test passphrase",NULL,false));assert(mesh_config_store_busy()&&!mesh_config_store_start(CFG_JOB_LIST,NULL,NULL,NULL,false));assert(mesh_config_store_take(&result)&&result.ok);
 char path[64];strcpy(path,result.path);assert(mesh_config_store_start(CFG_JOB_LIST,NULL,NULL,NULL,false)&&mesh_config_store_take(&result)&&result.ok&&result.count==1&&!strcmp(result.files[0],path));
 assert(!mesh_config_store_start(CFG_JOB_OPEN,NULL,"a long test passphrase","../config-bad.mmc",false));
 assert(mesh_config_store_start(CFG_JOB_OPEN,NULL,"a long test passphrase",path,false)&&mesh_config_store_take(&result)&&result.ok&&result.archive.count==2&&result.archive.node==a.node);
 assert(mesh_config_store_start(CFG_JOB_SAVE,&a,"a long test passphrase",NULL,true)&&mesh_config_store_take(&result)&&result.ok&&strcmp(path,result.path));
 assert(mesh_config_store_start(CFG_JOB_OPEN,NULL,"a different long password",path,false)&&mesh_config_store_take(&result)&&!result.ok&&!result.archive.count);
 puts("PASS: configuration codec, actual PSA variable-length authenticated envelope, wrong password/tag/header/purpose/truncation, wiped failed plaintext, encrypted SD save/readback/list/open/safety and traversal/busy guards");
}
