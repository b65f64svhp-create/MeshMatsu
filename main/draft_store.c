#include "draft_store.h"
#include "message_reuse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#ifndef MESH_DRAFT_PATH
#define MESH_DRAFT_PATH "/locfd/meshmatsu-drafts.bin"
#endif
#define DRAFT_ROW 210
#define DRAFT_HEADER 16
typedef struct {mesh_draft_t records[MESH_DRAFT_LIMIT];uint32_t replies[MESH_DRAFT_LIMIT];size_t count;bool ok;} draft_job_t;
static atomic_int job_state;
static draft_job_t *completion;
static bool store_ready,primary_bad;
static void *draft_alloc(size_t n){
#ifdef ESP_PLATFORM
 return heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
 return malloc(n);
#endif
}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(8*i);}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t hash_bytes(uint32_t h,const void *data,size_t n){const uint8_t *p=data;while(n--)h=(h^*p++)*16777619u;return h;}
static bool row_encode(uint8_t row[DRAFT_ROW],const mesh_draft_t *d,uint32_t reply){
 size_t n=strnlen(d->text,sizeof(d->text));char checked[MESH_DRAFT_BYTES+1];
 if(d->peer==UINT32_MAX||d->channel>=8||n>MESH_DRAFT_BYTES||(!n&&!reply)||mesh_message_reuse(checked,sizeof(checked),d->text)!=n)return false;
 memset(row,0,DRAFT_ROW);put32(row,d->peer);put32(row+4,reply);row[8]=d->peer?0:d->channel;row[9]=(uint8_t)n;memcpy(row+10,d->text,n);return true;
}
static bool row_decode(const uint8_t row[DRAFT_ROW],mesh_draft_t *d,uint32_t *reply){
 *d=(mesh_draft_t){.peer=get32(row),.channel=row[8]};*reply=get32(row+4);unsigned n=row[9];
 if(n>MESH_DRAFT_BYTES)return false;
 memcpy(d->text,row+10,n);
 uint8_t canonical[DRAFT_ROW];return row_encode(canonical,d,*reply)&&!memcmp(canonical,row,DRAFT_ROW);
}
static bool read_archive(const char *path,draft_job_t *job,bool *missing){
 *missing=false;errno=0;FILE *f=fopen(path,"rb");if(!f){*missing=errno==ENOENT;return false;}
 uint8_t header[DRAFT_HEADER],row[DRAFT_ROW];bool ok=fread(header,1,sizeof(header),f)==sizeof(header);
 bool future=ok&&!memcmp(header,"MMD1",4)&&header[4]!=1;
 size_t count=ok?((size_t)header[6]|(size_t)header[7]<<8):0;
 ok=ok&&!memcmp(header,"MMD1",4)&&header[4]==1&&!header[5]&&count<=MESH_DRAFT_LIMIT&&get32(header+8)==count*DRAFT_ROW;
 uint32_t hash=ok?hash_bytes(2166136261u,header,12):0;
 for(size_t i=0;ok&&i<count;i++){
  ok=fread(row,1,sizeof(row),f)==sizeof(row)&&row_decode(row,&job->records[i],&job->replies[i]);
  if(ok)hash=hash_bytes(hash,row,sizeof(row));
  for(size_t j=0;ok&&j<i;j++)if(job->records[j].peer==job->records[i].peer&&job->records[j].channel==job->records[i].channel)ok=false;
 }
 ok=ok&&hash==get32(header+12)&&fgetc(f)==EOF&&!ferror(f);if(fclose(f))ok=false;
 if(future){errno=ENOTSUP;return false;}
 if(ok)job->count=count;
 return ok;
}
bool mesh_draft_store_load(mesh_draft_t *drafts,uint32_t *replies,size_t capacity,size_t *count,bool *recovered){
 if(!drafts||!replies||!count||!recovered||mesh_draft_store_busy())return false;
 store_ready=false;*recovered=false;draft_job_t *job=draft_alloc(sizeof(*job));if(!job)return false;
 bool missing=false,backup_missing=false;bool ok=read_archive(MESH_DRAFT_PATH,job,&missing);primary_bad=!ok;
 if(!ok&&errno==ENOTSUP){free(job);return false;}
 if(!ok){ok=read_archive(MESH_DRAFT_PATH ".bak",job,&backup_missing);*recovered=ok;}
 if(!ok&&missing&&backup_missing){job->count=0;ok=true;primary_bad=false;}
 if(ok&&job->count>capacity)ok=false;
 if(ok){memcpy(drafts,job->records,job->count*sizeof(*drafts));memcpy(replies,job->replies,job->count*sizeof(*replies));*count=job->count;store_ready=true;}
 free(job);return ok;
}
static bool write_archive(draft_job_t *job){
 uint8_t header[DRAFT_HEADER]={0},row[DRAFT_ROW];memcpy(header,"MMD1",4);header[4]=1;header[6]=job->count;header[7]=job->count>>8;put32(header+8,job->count*DRAFT_ROW);
 uint32_t hash=hash_bytes(2166136261u,header,12);
 for(size_t i=0;i<job->count;i++){if(!row_encode(row,&job->records[i],job->replies[i]))return false;hash=hash_bytes(hash,row,sizeof(row));}put32(header+12,hash);
 FILE *f=fopen(MESH_DRAFT_PATH ".tmp","wb");if(!f)return false;
 bool ok=fwrite(header,1,sizeof(header),f)==sizeof(header);
 for(size_t i=0;ok&&i<job->count;i++){row_encode(row,&job->records[i],job->replies[i]);ok=fwrite(row,1,sizeof(row),f)==sizeof(row);}
 if(ok)ok=fflush(f)==0&&fsync(fileno(f))==0;
 if(fclose(f))ok=false;
 // Compare the complete canonical file before rotating either durable snapshot.
 f=ok?fopen(MESH_DRAFT_PATH ".tmp","rb"):NULL;if(ok&&!f)ok=false;
 if(f){uint8_t bytes[DRAFT_ROW];ok=fread(bytes,1,DRAFT_HEADER,f)==DRAFT_HEADER&&!memcmp(bytes,header,DRAFT_HEADER);
  for(size_t i=0;ok&&i<job->count;i++){row_encode(row,&job->records[i],job->replies[i]);ok=fread(bytes,1,DRAFT_ROW,f)==DRAFT_ROW&&!memcmp(bytes,row,DRAFT_ROW);}
  ok=ok&&fgetc(f)==EOF&&!ferror(f);if(fclose(f))ok=false;
 }
 bool rotated=false;struct stat st;
 if(ok&&stat(MESH_DRAFT_PATH,&st)==0){
  if(primary_bad)ok=remove(MESH_DRAFT_PATH)==0; // Keep the verified recovery backup.
  else {if(remove(MESH_DRAFT_PATH ".bak")&&errno!=ENOENT)ok=false;if(ok){rotated=rename(MESH_DRAFT_PATH,MESH_DRAFT_PATH ".bak")==0;ok=rotated;}}
 }else if(ok&&errno!=ENOENT)ok=false;
 if(ok)ok=rename(MESH_DRAFT_PATH ".tmp",MESH_DRAFT_PATH)==0;
 if(!ok){if(rotated)rename(MESH_DRAFT_PATH ".bak",MESH_DRAFT_PATH);remove(MESH_DRAFT_PATH ".tmp");}
 else primary_bad=false;
 return ok;
}
static void draft_worker(void *argument){draft_job_t *job=argument;job->ok=write_archive(job);completion=job;atomic_store_explicit(&job_state,2,memory_order_release);
#ifdef ESP_PLATFORM
 vTaskDelete(NULL);
#endif
}
bool mesh_draft_store_busy(void){return atomic_load(&job_state)!=0;}
bool mesh_draft_store_poll(bool *success){
 if(atomic_load_explicit(&job_state,memory_order_acquire)!=2)return false;
 if(success)*success=completion->ok;
 free(completion);completion=NULL;atomic_store_explicit(&job_state,0,memory_order_release);return true;
}
bool mesh_draft_store_start(const mesh_draft_t *drafts,const uint32_t *replies,size_t count){
 if(!store_ready||mesh_draft_store_busy()||!drafts||!replies||count>MESH_DRAFT_LIMIT)return false;
 draft_job_t *job=draft_alloc(sizeof(*job));if(!job)return false;job->count=0;
 for(size_t i=0;i<count;i++){
  if(!drafts[i].text[0]&&!replies[i])continue;
  uint8_t row[DRAFT_ROW];if(!row_encode(row,&drafts[i],replies[i])){free(job);return false;}
  for(size_t j=0;j<job->count;j++)if(job->records[j].peer==drafts[i].peer&&job->records[j].channel==(drafts[i].peer?0:drafts[i].channel)){free(job);return false;}
  job->records[job->count]=drafts[i];job->records[job->count].channel=drafts[i].peer?0:drafts[i].channel;job->replies[job->count++]=replies[i];
 }
 atomic_store(&job_state,1);
#ifdef ESP_PLATFORM
 if(xTaskCreate(draft_worker,"draft_save",8*1024,job,tskIDLE_PRIORITY+1,NULL)!=pdPASS){atomic_store(&job_state,0);free(job);return false;}
#else
 draft_worker(job);
#endif
 return true;
}
