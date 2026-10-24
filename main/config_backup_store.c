#include "config_backup.h"
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "psa/crypto.h"
#ifdef ESP_PLATFORM
#include "map_tiles.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#else
#define EXT_RAM_BSS_ATTR
#endif
#ifndef MESH_CONFIG_DIR
#define MESH_CONFIG_DIR "/sdcard/meshmatsu/backups"
#endif
static atomic_int cfg_store_state;
static EXT_RAM_BSS_ATTR mesh_cfg_result_t cfg_result;
typedef struct {
    mesh_cfg_job_t job;
    mesh_config_backup_t archive;
    char password[97],path[64];
    bool safety;
}
config_job_t;
static void *cfg_alloc(size_t n){
#ifdef ESP_PLATFORM
    return heap_caps_calloc(1,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
    return calloc(1,n);
#endif
}
static bool cfg_filename(const char *s){
    size_t n=s?strlen(s):0,base=0;
    if(n==37&&!strncmp(s,"config-",7))base=7;
    else if(n==43&&!strncmp(s,"safetyconfig-",13))base=13;
    else return false;
    if(strcmp(s+n-4,".mmc"))return false;
    for(size_t i=0;i<26;i++){
        char c=s[base+i];
        if(i==8||i==17){
            if(c!='-')return false;
        }
        else if(!((c>='a'&&c<='f')||(c>='0'&&c<='9')))return false;
    }
    return true;
}
static bool cfg_dir(const char *path){
    struct stat st;
    if(!stat(path,&st))return S_ISDIR(st.st_mode);
    if(errno!=ENOENT)return false;
    if(!mkdir(path,0700))return true;
    return errno==EEXIST&&!stat(path,&st)&&S_ISDIR(st.st_mode);
}
static bool cfg_storage(void){
#ifdef ESP_PLATFORM
    if(!mesh_map_storage_mount()||!cfg_dir("/sdcard/meshmatsu"))return false;
#endif
    return cfg_dir(MESH_CONFIG_DIR);
}
static bool cfg_path(const char *name,char path[160]){
    return cfg_filename(name)&&snprintf(path,160,"%s/%s",MESH_CONFIG_DIR,name)<160;
}
static int cfg_descending(const void *a,const void *b){
    const char *x=a,*y=b;
    int by_time=strncmp(strchr(y,'-')+1,strchr(x,'-')+1,8);
    return by_time?by_time:strcmp(y,x);
}
static bool cfg_list(void){
    DIR *d=opendir(MESH_CONFIG_DIR);
    if(!d)return false;
    struct dirent *e;
    while((e=readdir(d))){
        char path[160];
        struct stat st;
        if(!cfg_path(e->d_name,path)||stat(path,&st)||!S_ISREG(st.st_mode))continue;
        size_t i=cfg_result.count;
        if(i==32){
            if(cfg_descending(e->d_name,cfg_result.files[31])>=0)continue;
            i--;
        }
        else cfg_result.count++;
        snprintf(cfg_result.files[i],64,"%.63s",e->d_name);
        qsort(cfg_result.files,cfg_result.count,64,cfg_descending);
    }
    return closedir(d)==0;
}
static bool cfg_read(const char *path,uint8_t **out,size_t *size){
    struct stat st;
    if(stat(path,&st)||st.st_size<61||st.st_size>MESH_CFG_WIRE_MAX+60)return false;
    uint8_t *bytes=cfg_alloc(st.st_size);
    if(!bytes)return false;
    FILE *f=fopen(path,"rb");
    bool ok=f!=NULL;
    if(f){
        ok=fread(bytes,1,st.st_size,f)==(size_t)st.st_size&&fgetc(f)==EOF&&!ferror(f);
        if(fclose(f))ok=false;
    }
    if(!ok){
        mesh_keys_wipe(bytes,st.st_size);
        free(bytes);
        return false;
    }
    *out=bytes;
    *size=st.st_size;
    return true;
}
static bool cfg_save(config_job_t *job){
    uint8_t *plain=cfg_alloc(MESH_CFG_WIRE_MAX),*sealed=NULL,*readback=NULL,*check=NULL;
    size_t n=MESH_CFG_WIRE_MAX,fn=0,rn=0,cn=0;
    bool ok=plain&&mesh_config_pack(&job->archive,plain,&n);
    uint8_t salt[16],nonce[12],random[4];
    if(ok)ok=psa_crypto_init()==PSA_SUCCESS&&psa_generate_random(salt,16)==PSA_SUCCESS&&psa_generate_random(nonce,12)==PSA_SUCCESS&&psa_generate_random(random,4)==PSA_SUCCESS;
    if(ok){
        fn=n+60;
        sealed=cfg_alloc(fn);
        ok=sealed&&mesh_config_seal(plain,n,job->password,salt,nonce,sealed,&fn);
    }
    char path[160]={
        0
    }
    ,temp[168]={
        0
    }
    ;
    int fd=-1;
    bool own_temp=false;
    if(ok){
        uint32_t random_id=(uint32_t)random[0]|(uint32_t)random[1]<<8|(uint32_t)random[2]<<16|(uint32_t)random[3]<<24;
        snprintf(cfg_result.path,64,"%s-%08lx-%08lx-%08lx.mmc",job->safety?"safetyconfig":"config",(unsigned long)job->archive.timestamp,(unsigned long)job->archive.node,(unsigned long)random_id);
        ok=cfg_path(cfg_result.path,path);
        struct stat st;
        if(ok&&(!stat(path,&st)||errno!=ENOENT))ok=false;
        snprintf(temp,sizeof(temp),"%s.part",path);
        if(ok){
            fd=open(temp,O_WRONLY|O_CREAT|O_EXCL,0600);
            own_temp=fd>=0;
            ok=own_temp;
        }
    }
    if(ok){
        size_t done=0;
        while(done<fn){
            ssize_t w=write(fd,sealed+done,fn-done);
            if(w<=0){
                ok=false;
                break;
            }
            done+=w;
        }
        if(fsync(fd))ok=false;
    }
    if(fd>=0&&close(fd))ok=false;
    if(ok){
        ok=cfg_read(temp,&readback,&rn)&&rn==fn&&!memcmp(readback,sealed,fn);
        cn=n;
        check=cfg_alloc(n);
        ok=ok&&check&&mesh_config_open(readback,rn,job->password,check,&cn)&&cn==n&&!memcmp(plain,check,n);
    }
    if(ok){
        struct stat st;
        ok=stat(path,&st)!=0&&errno==ENOENT&&rename(temp,path)==0;
    }
    if(!ok&&own_temp)unlink(temp);
    if(plain){
        mesh_keys_wipe(plain,MESH_CFG_WIRE_MAX);
        free(plain);
    }
    if(sealed){
        mesh_keys_wipe(sealed,fn);
        free(sealed);
    }
    if(readback){
        mesh_keys_wipe(readback,rn);
        free(readback);
    }
    if(check){
        mesh_keys_wipe(check,n);
        free(check);
    }
    return ok;
}
static void cfg_worker(void *arg){
    config_job_t *job=arg;
    mesh_keys_wipe(&cfg_result,sizeof(cfg_result));
    cfg_result.job=job->job;
    bool ok=cfg_storage();
    if(ok&&job->job==CFG_JOB_LIST)ok=cfg_list();
    else if(ok&&job->job==CFG_JOB_SAVE)ok=cfg_save(job);
    else if(ok&&job->job==CFG_JOB_OPEN){
        char path[160];
        uint8_t *bytes=NULL,*plain=NULL;
        size_t n=0,pn=0;
        ok=cfg_path(job->path,path)&&cfg_read(path,&bytes,&n);
        if(ok){
            pn=n-60;
            plain=cfg_alloc(pn);
            ok=plain&&mesh_config_open(bytes,n,job->password,plain,&pn)&&mesh_config_unpack(plain,pn,&cfg_result.archive);
        }
        if(bytes){
            mesh_keys_wipe(bytes,n);
            free(bytes);
        }
        if(plain){
            mesh_keys_wipe(plain,pn);
            free(plain);
        }
        snprintf(cfg_result.path,64,"%s",job->path);
    }
    cfg_result.ok=ok;
    mesh_keys_wipe(job,sizeof(*job));
    free(job);
    atomic_store_explicit(&cfg_store_state,2,memory_order_release);
#ifdef ESP_PLATFORM
    vTaskDelete(NULL);
#endif
}
bool mesh_config_store_start(mesh_cfg_job_t job,const mesh_config_backup_t *archive,const char *password,const char *path,bool safety){
    if(job<CFG_JOB_LIST||job>CFG_JOB_OPEN||(job==CFG_JOB_SAVE&&!archive)||(job!=CFG_JOB_LIST&&(!password||strlen(password)<12||strlen(password)>96))||(job==CFG_JOB_OPEN&&!cfg_filename(path)))return false;
    int expected=0;
    if(!atomic_compare_exchange_strong(&cfg_store_state,&expected,1))return false;
    config_job_t *request=cfg_alloc(sizeof(*request));
    if(!request){
        atomic_store(&cfg_store_state,0);
        return false;
    }
    request->job=job;
    request->safety=safety;
    if(archive)request->archive=*archive;
    if(password)snprintf(request->password,97,"%s",password);
    if(path)snprintf(request->path,64,"%s",path);
#ifdef ESP_PLATFORM
    if(xTaskCreate(cfg_worker,"config_backup",16*1024,request,tskIDLE_PRIORITY+1,NULL)!=pdPASS){
        mesh_keys_wipe(request,sizeof(*request));
        free(request);
        atomic_store(&cfg_store_state,0);
        return false;
    }
#else
    cfg_worker(request);
#endif
    return true;
}
bool mesh_config_store_busy(void){
    return atomic_load_explicit(&cfg_store_state,memory_order_acquire)!=0;
}
bool mesh_config_store_take(mesh_cfg_result_t *out){
    if(!out||atomic_load_explicit(&cfg_store_state,memory_order_acquire)!=2)return false;
    *out=cfg_result;
    mesh_keys_wipe(&cfg_result,sizeof(cfg_result));
    atomic_store_explicit(&cfg_store_state,0,memory_order_release);
    return true;
}
