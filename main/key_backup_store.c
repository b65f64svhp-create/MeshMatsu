#include "key_backup.h"
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "psa/crypto.h"
#ifdef ESP_PLATFORM
#include "map_tiles.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#ifndef MESH_KEY_BACKUP_DIR
#define MESH_KEY_BACKUP_DIR "/sdcard/meshmatsu/backups"
#endif
static atomic_int state;
static mesh_key_result_t result;
typedef struct { mesh_key_job_t job;mesh_key_backup_t backup;char password[MESH_KEY_PASSWORD_MAX+1],path[64];bool safety; } key_job_t;
static bool filename_valid(const char *p) {
    size_t n=p?strlen(p):0;
    if(n<10||n>=64||strstr(p,"..")||strcmp(p+n-4,".mmk")||(strncmp(p,"keys-",5)&&strncmp(p,"safety-",7)))return false;
    for(size_t i=0;i<n;i++)if(!((p[i]>='a'&&p[i]<='z')||(p[i]>='0'&&p[i]<='9')||p[i]=='-'||p[i]=='.'))return false;
    return true;
}
static bool path_for(const char *name,char out[MESH_KEY_PATH_MAX]) {
    return filename_valid(name)&&snprintf(out,MESH_KEY_PATH_MAX,"%s/%s",MESH_KEY_BACKUP_DIR,name)<MESH_KEY_PATH_MAX;
}
static bool directory(const char *path) { struct stat st;if(!stat(path,&st))return S_ISDIR(st.st_mode);return errno==ENOENT&&!mkdir(path,0700); }
static bool storage_ready(void) {
#ifdef ESP_PLATFORM
    if(!mesh_map_storage_mount()||!directory("/sdcard/meshmatsu"))return false;
#endif
    return directory(MESH_KEY_BACKUP_DIR);
}
static int descending(const void *a,const void *b){
    const char *x=a,*y=b;int time= strncmp(strchr(y,'-')+1,strchr(x,'-')+1,8);
    return time?time:strcmp(y,x);
}
static bool list_files(void) {
    DIR *d=opendir(MESH_KEY_BACKUP_DIR);if(!d)return false;
    struct dirent *entry;
    while((entry=readdir(d))){char path[MESH_KEY_PATH_MAX];struct stat st;
        if(!path_for(entry->d_name,path)||stat(path,&st)||!S_ISREG(st.st_mode))continue;
        // Keep the lexically newest bounded set, rather than inaccessible old entries.
        size_t i=result.count;
        if(i==MESH_KEY_FILE_LIMIT){if(descending(entry->d_name,result.files[i-1])>=0)continue;i--;}
        else result.count++;
        snprintf(result.files[i],64,"%.63s",entry->d_name);qsort(result.files,result.count,64,descending);
    }
    return closedir(d)==0;
}
static bool read_file(const char *path,uint8_t *bytes,size_t *size) {
    FILE *f=fopen(path,"rb");if(!f)return false;
    size_t n=fread(bytes,1,MESH_KEY_FILE_MAX,f);bool ok=!ferror(f)&&fgetc(f)==EOF&&n>0;
    if(fclose(f))ok=false;
    *size=n;return ok;
}
static bool same_backup(const mesh_key_backup_t *a,const mesh_key_backup_t *b) {
    if(a->node!=b->node||a->timestamp!=b->timestamp||a->identity!=b->identity||a->channel_mask!=b->channel_mask||strcmp(a->name,b->name))return false;
    if(a->identity&&(memcmp(a->public_key,b->public_key,32)||memcmp(a->private_key,b->private_key,32)))return false;
    for(unsigned i=0;i<8;i++)if(a->channel_mask&(1u<<i)){
        const mesh_backup_channel_t *x=&a->channel[i],*y=&b->channel[i];
        if(x->role!=y->role||x->length!=y->length||strcmp(x->name,y->name)||memcmp(x->key,y->key,x->length))return false;
    }
    return true;
}
static bool save_file(key_job_t *job) {
    uint8_t salt[16],nonce[12],random[4],bytes[MESH_KEY_FILE_MAX],readback[MESH_KEY_FILE_MAX];
    if(psa_crypto_init()!=PSA_SUCCESS||psa_generate_random(salt,sizeof(salt))!=PSA_SUCCESS||
        psa_generate_random(nonce,sizeof(nonce))!=PSA_SUCCESS||psa_generate_random(random,sizeof(random))!=PSA_SUCCESS)return false;
    size_t n=sizeof(bytes),rn=0;bool ok=mesh_keys_seal(&job->backup,job->password,salt,nonce,bytes,&n);
    uint32_t unique=(uint32_t)random[0]|(uint32_t)random[1]<<8|(uint32_t)random[2]<<16|(uint32_t)random[3]<<24;
    snprintf(result.path,sizeof(result.path),"%s-%08lx-%08lx-%08lx.mmk",job->safety?"safety":"keys",(unsigned long)job->backup.timestamp,(unsigned long)job->backup.node,(unsigned long)unique);
    char path[MESH_KEY_PATH_MAX]={0},temporary[MESH_KEY_PATH_MAX+8];
    if(!path_for(result.path,path))ok=false;
    snprintf(temporary,sizeof(temporary),"%s.tmp",path);
    struct stat st;if(!stat(path,&st))ok=false;
    int fd=-1;if(ok)fd=open(temporary,O_WRONLY|O_CREAT|O_EXCL,0600);
    if(fd<0)ok=false;
    if(ok){size_t done=0;while(done<n){ssize_t w=write(fd,bytes+done,n-done);if(w<=0){ok=false;break;}done+=(size_t)w;}if(fsync(fd))ok=false;}
    if(fd>=0&&close(fd))ok=false;
    mesh_key_backup_t verified={0};
    if(ok)ok=read_file(temporary,readback,&rn)&&rn==n&&!memcmp(bytes,readback,n)&&mesh_keys_open(readback,rn,job->password,&verified)&&same_backup(&job->backup,&verified);
    if(ok)ok=rename(temporary,path)==0;
    if(!ok&&fd>=0)unlink(temporary);
    mesh_keys_wipe(bytes,sizeof(bytes));mesh_keys_wipe(readback,sizeof(readback));mesh_keys_wipe(&verified,sizeof(verified));return ok;
}
static void worker(void *argument) {
    key_job_t *job=argument;memset(&result,0,sizeof(result));result.job=job->job;
    bool ok=storage_ready();
    if(ok&&job->job==MESH_KEY_JOB_LIST)ok=list_files();
    else if(ok&&job->job==MESH_KEY_JOB_SAVE)ok=save_file(job);
    else if(ok&&job->job==MESH_KEY_JOB_OPEN){
        char path[MESH_KEY_PATH_MAX];uint8_t bytes[MESH_KEY_FILE_MAX];size_t n=0;
        ok=path_for(job->path,path)&&read_file(path,bytes,&n)&&mesh_keys_open(bytes,n,job->password,&result.backup);
        snprintf(result.path,sizeof(result.path),"%s",job->path);mesh_keys_wipe(bytes,sizeof(bytes));
    }
    result.ok=ok;mesh_keys_wipe(job,sizeof(*job));free(job);
    atomic_store_explicit(&state,2,memory_order_release);
#ifdef ESP_PLATFORM
    vTaskDelete(NULL);
#endif
}
bool mesh_key_store_start(mesh_key_job_t job,const mesh_key_backup_t *backup,const char *password,const char *path,bool safety) {
    if(job<MESH_KEY_JOB_LIST||job>MESH_KEY_JOB_OPEN||(job==MESH_KEY_JOB_SAVE&&!backup)||
        (job!=MESH_KEY_JOB_LIST&&(!password||strlen(password)<12||strlen(password)>MESH_KEY_PASSWORD_MAX))||
        (job==MESH_KEY_JOB_OPEN&&!filename_valid(path)))return false;
    int expected=0;if(!atomic_compare_exchange_strong(&state,&expected,1))return false;
    key_job_t *request=calloc(1,sizeof(*request));
    if(!request){atomic_store(&state,0);return false;}
    request->job=job;request->safety=safety;if(backup)request->backup=*backup;
    if(password)snprintf(request->password,sizeof(request->password),"%s",password);
    if(path)snprintf(request->path,sizeof(request->path),"%s",path);
#ifdef ESP_PLATFORM
    if(xTaskCreate(worker,"key_backup",16*1024,request,tskIDLE_PRIORITY+1,NULL)!=pdPASS){mesh_keys_wipe(request,sizeof(*request));free(request);atomic_store(&state,0);return false;}
#else
    worker(request);
#endif
    return true;
}
bool mesh_key_store_busy(void){return atomic_load_explicit(&state,memory_order_acquire)!=0;}
bool mesh_key_store_take(mesh_key_result_t *out) {
    if(!out||atomic_load_explicit(&state,memory_order_acquire)!=2)return false;
    *out=result;mesh_keys_wipe(&result,sizeof(result));atomic_store_explicit(&state,0,memory_order_release);return true;
}
