#include "telemetry_data.h"
#include "telemetry_store.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef ESP_PLATFORM
#include "map_tiles.h"
#include "radio_link.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#ifndef MESH_TLM_DIRECTORY
#define MESH_TLM_DIRECTORY "/sdcard/meshmatsu"
#endif
#define TLM_PATH MESH_TLM_DIRECTORY "/telemetry.bin"
#define TLM_TEMP TLM_PATH ".tmp"
#define TLM_BACKUP TLM_PATH ".bak"
#define TLM_WIRE_SIZE 200
_Static_assert(sizeof(double)==8,"Telemetry file requires 64-bit IEEE doubles");
#define TLM_HEADER_SIZE 20
static atomic_int job_state;
static bool loaded,force_save,last_error;
static int64_t next_attempt;
static uint64_t saved_revision;
typedef struct {bool read,ok;uint64_t revision;size_t count;mesh_tlm_sample_t *samples;} tlm_job_t;
static tlm_job_t *completion;
static size_t merge_position;
static bool merge_started,merge_had_live;
static uint64_t merge_expected;
static void *tlm_alloc(size_t size){
#ifdef ESP_PLATFORM
 return heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
 return malloc(size);
#endif
}
static uint32_t tlm_get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void tlm_put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
static uint32_t tlm_hash(uint32_t h,const uint8_t *p,size_t n){while(n--)h=(h^*p++)*16777619u;return h;}
static void tlm_pack(const mesh_tlm_sample_t *s,uint8_t p[TLM_WIRE_SIZE]){
 memset(p,0,TLM_WIRE_SIZE);tlm_put32(p,s->node);p[4]=s->kind;p[5]=s->flags&(MESH_TLM_CACHED|MESH_TLM_CLOCK_SKEW);tlm_put32(p+8,s->time);tlm_put32(p+12,s->received_at);tlm_put32(p+16,s->packet);tlm_put32(p+20,s->present);
 for(unsigned f=0;f<MESH_TLM_FIELDS;f++)if(s->present&(1u<<f)){uint64_t bits;memcpy(&bits,&s->value[f],8);for(unsigned b=0;b<8;b++)p[24+f*8+b]=bits>>(b*8);}
}
static bool tlm_unpack(const uint8_t p[TLM_WIRE_SIZE],mesh_tlm_sample_t *s){
 memset(s,0,sizeof(*s));if(p[6]||p[7]||(p[5]&~(MESH_TLM_CACHED|MESH_TLM_CLOCK_SKEW)))return false;
 s->node=tlm_get32(p);s->kind=p[4];s->flags=p[5];s->time=tlm_get32(p+8);s->received_at=tlm_get32(p+12);s->packet=tlm_get32(p+16);s->present=tlm_get32(p+20);
 for(unsigned f=0;f<MESH_TLM_FIELDS;f++){uint64_t bits=0;for(unsigned b=0;b<8;b++)bits|=(uint64_t)p[24+f*8+b]<<(b*8);memcpy(&s->value[f],&bits,8);}
 return mesh_tlm_valid(s);
}
static bool tlm_storage(void){
#ifdef ESP_PLATFORM
 if(!mesh_map_storage_mount())return false;
#endif
 struct stat st;if(!stat(MESH_TLM_DIRECTORY,&st))return S_ISDIR(st.st_mode);
 if(errno!=ENOENT)return false;
 if(!mkdir(MESH_TLM_DIRECTORY,0700))return true;
 return errno==EEXIST&&!stat(MESH_TLM_DIRECTORY,&st)&&S_ISDIR(st.st_mode);
}
static bool tlm_read_file(const char *path,tlm_job_t *job){
 FILE *f=fopen(path,"rb");if(!f)return false;
 uint8_t head[TLM_HEADER_SIZE],wire[TLM_WIRE_SIZE];bool ok=fread(head,1,sizeof(head),f)==sizeof(head)&&!memcmp(head,"MMTEL001",8)&&tlm_get32(head+12)==TLM_WIRE_SIZE;
 uint32_t count=ok?tlm_get32(head+8):0;ok=ok&&count<=MESH_TLM_LIMIT;
 mesh_tlm_sample_t *samples=ok&&count?tlm_alloc(count*sizeof(*samples)):NULL;
 if(count&&!samples)ok=false;
 uint32_t hash=tlm_hash(2166136261u,head,ok?16:0);
 for(uint32_t i=0;ok&&i<count;i++){ok=fread(wire,1,sizeof(wire),f)==sizeof(wire)&&tlm_unpack(wire,&samples[i]);if(ok)hash=tlm_hash(hash,wire,sizeof(wire));}
 if(ok)ok=hash==tlm_get32(head+16)&&fgetc(f)==EOF&&!ferror(f);
 if(fclose(f))ok=false;
 if(ok){job->samples=samples;job->count=count;}else free(samples);
 return ok;
}
static bool tlm_load(tlm_job_t *job){
 const char *paths[]={TLM_PATH,TLM_BACKUP,TLM_TEMP};bool existed=false;
 for(unsigned i=0;i<3;i++){struct stat st;if(!stat(paths[i],&st)){existed=true;if(tlm_read_file(paths[i],job))return true;}}
 return !existed;
}
static bool tlm_save(tlm_job_t *job){
 FILE *f=fopen(TLM_TEMP,"wb");if(!f)return false;
 uint8_t head[TLM_HEADER_SIZE]={0},wire[TLM_WIRE_SIZE];memcpy(head,"MMTEL001",8);tlm_put32(head+8,job->count);tlm_put32(head+12,TLM_WIRE_SIZE);
 uint32_t hash=tlm_hash(2166136261u,head,16);bool ok=fwrite(head,1,sizeof(head),f)==sizeof(head);
 for(size_t i=0;ok&&i<job->count;i++){tlm_pack(&job->samples[i],wire);hash=tlm_hash(hash,wire,sizeof(wire));ok=fwrite(wire,1,sizeof(wire),f)==sizeof(wire);}
 tlm_put32(head+16,hash);if(ok)ok=!fseek(f,0,SEEK_SET)&&fwrite(head,1,sizeof(head),f)==sizeof(head)&&!fflush(f)&&!fsync(fileno(f));
 if(fclose(f))ok=false;
 if(!ok){remove(TLM_TEMP);return false;}
 // Validate the actual saved bytes before replacing a previous good snapshot.
 tlm_job_t verified={0};ok=tlm_read_file(TLM_TEMP,&verified)&&verified.count==job->count;free(verified.samples);
 if(!ok){remove(TLM_TEMP);return false;}
 struct stat st;bool moved=false;
 if(!stat(TLM_PATH,&st)){remove(TLM_BACKUP);moved=rename(TLM_PATH,TLM_BACKUP)==0;if(!moved){remove(TLM_TEMP);return false;}}
 if(rename(TLM_TEMP,TLM_PATH)){if(moved)rename(TLM_BACKUP,TLM_PATH);return false;}
 return true;
}
static void tlm_worker(void *argument){
 tlm_job_t *job=argument;job->ok=tlm_storage()&&(job->read?tlm_load(job):tlm_save(job));completion=job;atomic_store_explicit(&job_state,2,memory_order_release);
#ifdef ESP_PLATFORM
 vTaskDelete(NULL);
#endif
}
static bool tlm_start(bool read){
 tlm_job_t *job=calloc(1,sizeof(*job));if(!job)return false;job->read=read;job->revision=mesh_tlm_revision();
 if(!read){job->count=mesh_tlm_count();if(job->count){job->samples=tlm_alloc(job->count*sizeof(*job->samples));if(!job->samples||mesh_tlm_snapshot(job->samples,job->count)!=job->count){free(job->samples);free(job);return false;}}}
 atomic_store(&job_state,1);
#ifdef ESP_PLATFORM
 if(xTaskCreate(tlm_worker,"telemetry_sd",12*1024,job,tskIDLE_PRIORITY+1,NULL)!=pdPASS){atomic_store(&job_state,0);free(job->samples);free(job);return false;}
#else
 tlm_worker(job);
#endif
 return true;
}
void mesh_tlm_store_poll(int64_t now,bool busy){
 if(atomic_load_explicit(&job_state,memory_order_acquire)==2){
  tlm_job_t *job=completion;
  if(job->read&&job->ok){
   if(!merge_started){merge_started=true;merge_position=0;merge_had_live=mesh_tlm_count()!=0;merge_expected=mesh_tlm_revision();}
   if(mesh_tlm_revision()!=merge_expected)merge_had_live=true;
   if(busy)return;
   size_t end=merge_position+4;if(end>job->count)end=job->count;
   while(merge_position<end){bool accept=true;
#ifdef ESP_PLATFORM
    mesh_node_t node;if(mesh_radio_get_node_by_number(job->samples[merge_position].node,&node)&&node.is_ignored){accept=false;merge_had_live=true;}
#endif
    if(accept){mesh_tlm_import(&job->samples[merge_position],merge_position+1);}
    merge_position++;
   }
   merge_expected=mesh_tlm_revision();
   if(merge_position<job->count)return;
   loaded=true;if(!merge_had_live)saved_revision=mesh_tlm_revision();merge_started=false;
  }
  else if(!job->read&&job->ok)saved_revision=job->revision;
  last_error=!job->ok;next_attempt=now+300000000;free(job->samples);free(job);completion=NULL;atomic_store_explicit(&job_state,0,memory_order_release);
 }
 if(atomic_load(&job_state)||busy)return;
 if(loaded&&saved_revision==mesh_tlm_revision()){force_save=false;return;}
 if(now<next_attempt&&!force_save)return;
 bool read=!loaded;
 if(!tlm_start(read)){last_error=true;next_attempt=now+300000000;force_save=false;}
 else{next_attempt=now+300000000;force_save=false;}
}
int mesh_tlm_store_flush(int64_t now){
 if((!mesh_tlm_count()&&!loaded)||(loaded&&saved_revision==mesh_tlm_revision()&&!atomic_load(&job_state)))return 1;
 if(last_error&&!atomic_load(&job_state))return -1;
 force_save=true;mesh_tlm_store_poll(now,false);return 0;
}
const char *mesh_tlm_store_status(void){return atomic_load(&job_state)?"SD history: working...":last_error?"History in memory / SD unavailable":!loaded?"Loading SD history...":saved_revision==mesh_tlm_revision()?"History saved on SD":"History in memory / SD save queued";}

void mesh_tlm_store_retry(void){next_attempt=0;last_error=false;force_save=true;}
