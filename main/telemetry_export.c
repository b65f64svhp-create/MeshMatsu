#include "telemetry_export.h"
#include "telemetry_csv.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef ESP_PLATFORM
#include "map_tiles.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#ifndef MESH_EXPORT_ROOT
#define MESH_EXPORT_ROOT "/sdcard/meshmatsu"
#endif
#define EXPORT_DIR MESH_EXPORT_ROOT "/exports"
typedef struct {mesh_tlm_sample_t *samples;size_t count;uint32_t node,created;uint8_t kind;bool ok;const char *error;mesh_tlm_csv_stats_t csv;char filename[64];} export_job_t;
static atomic_int export_job_state;
static export_job_t *export_completion;
static mesh_tlm_export_status_t export_status;
static bool export_directory(const char *path){struct stat st;if(!stat(path,&st))return S_ISDIR(st.st_mode);if(errno!=ENOENT)return false;if(!mkdir(path,0700))return true;return errno==EEXIST&&!stat(path,&st)&&S_ISDIR(st.st_mode);}
static bool export_mount(void){
#ifdef ESP_PLATFORM
 if(!mesh_map_storage_mount())return false;
#endif
 return export_directory(MESH_EXPORT_ROOT)&&export_directory(EXPORT_DIR);
}
static bool export_verified(const char *path,const mesh_tlm_csv_stats_t *expected){
 FILE *f=fopen(path,"rb");if(!f)return false;uint8_t bytes[1024];size_t n,total=0;uint32_t hash=2166136261u;
 while((n=fread(bytes,1,sizeof(bytes),f))){total+=n;for(size_t i=0;i<n;i++)hash=(hash^bytes[i])*16777619u;}
 bool ok=!ferror(f)&&total==expected->bytes&&hash==expected->hash;if(fclose(f))ok=false;return ok;
}
static bool export_write(export_job_t *job){
 char final[240],partial[248];FILE *file=NULL;
 for(unsigned sequence=1;sequence<=999;sequence++){
  snprintf(job->filename,sizeof(job->filename),"tlm-%08lx-k%u-%010lu-%03u.csv",(unsigned long)job->node,job->kind,(unsigned long)job->created,sequence);
  snprintf(final,sizeof(final),EXPORT_DIR "/%s",job->filename);snprintf(partial,sizeof(partial),"%s.part",final);
  struct stat st;if(!stat(final,&st))continue;if(errno!=ENOENT)return false;
  int fd=open(partial,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0){if(errno==EEXIST)continue;return false;}
  file=fdopen(fd,"wb");if(!file){close(fd);remove(partial);return false;}break;
 }
 if(!file)return false;
 bool ok=mesh_tlm_csv_write(file,job->samples,job->count,&job->csv);
 if(ok){ok=!fflush(file)&&!fsync(fileno(file));}
 if(fclose(file))ok=false;
 if(ok)ok=export_verified(partial,&job->csv);
 // A single export worker and exclusive partial files preserve earlier exports.
 struct stat st;if(ok)ok=stat(final,&st)!=0&&errno==ENOENT;
 if(ok)ok=rename(partial,final)==0;
 if(!ok)remove(partial);
 return ok;
}
static void export_worker(void *argument){
 export_job_t *job=argument;
 if(!export_mount())job->error="SD unavailable. Check the card, then press E to retry.";
 else if(!(job->ok=export_write(job)))job->error="Export not saved. Check SD space/card, then press E to retry.";
 export_completion=job;atomic_store_explicit(&export_job_state,2,memory_order_release);
#ifdef ESP_PLATFORM
 vTaskDelete(NULL);
#endif
}
bool mesh_tlm_export_poll(void){
 if(atomic_load_explicit(&export_job_state,memory_order_acquire)!=2)return false;
 export_job_t *job=export_completion;export_status.busy=false;export_status.success=job->ok;export_status.rows=job->ok?job->csv.rows:0;
 if(job->ok){snprintf(export_status.filename,sizeof(export_status.filename),"%s",job->filename);snprintf(export_status.message,sizeof(export_status.message),"Saved %u report%s: meshmatsu/exports/%s",(unsigned)job->count,job->count==1?"":"s",job->filename);}
 else snprintf(export_status.message,sizeof(export_status.message),"%s",job->error?job->error:"Export failed. Press E to retry.");
 free(job->samples);free(job);export_completion=NULL;atomic_store_explicit(&export_job_state,0,memory_order_release);return true;
}
bool mesh_tlm_export_busy(void){return atomic_load(&export_job_state)!=0;}
mesh_tlm_export_status_t mesh_tlm_export_status(void){return export_status;}
mesh_tlm_export_start_t mesh_tlm_export_start(uint32_t node,uint8_t kind){
 if(mesh_tlm_export_busy())return MESH_EXPORT_BUSY;
 if(!node||node==UINT32_MAX||kind<2||kind>6)return MESH_EXPORT_INVALID;
 export_job_t *job=calloc(1,sizeof(*job));if(!job)return MESH_EXPORT_MEMORY;
#ifdef ESP_PLATFORM
 job->samples=heap_caps_malloc(MESH_TLM_SERIES_LIMIT*sizeof(*job->samples),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
 job->samples=malloc(MESH_TLM_SERIES_LIMIT*sizeof(*job->samples));
#endif
 if(!job->samples){free(job);return MESH_EXPORT_MEMORY;}
 job->count=mesh_tlm_series(node,kind,job->samples,MESH_TLM_SERIES_LIMIT);
 if(!job->count){free(job->samples);free(job);return MESH_EXPORT_EMPTY;}
 job->node=node;job->kind=kind;time_t now=time(NULL);job->created=now>=1700000000&&(uint64_t)now<=UINT32_MAX?(uint32_t)now:0;
 export_status=(mesh_tlm_export_status_t){.busy=true,.node=node,.kind=kind,.reports=job->count};snprintf(export_status.message,sizeof(export_status.message),"Exporting %u reports to SD...",(unsigned)job->count);
 atomic_store(&export_job_state,1);
#ifdef ESP_PLATFORM
 if(xTaskCreate(export_worker,"telemetry_csv",12*1024,job,tskIDLE_PRIORITY+1,NULL)!=pdPASS){atomic_store(&export_job_state,0);export_status.busy=false;free(job->samples);free(job);return MESH_EXPORT_MEMORY;}
#else
 export_worker(job);
#endif
 return MESH_EXPORT_OK;
}
