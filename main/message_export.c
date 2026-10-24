#include "message_export.h"
#include "radio_link.h"
#include <stdio.h>
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
typedef struct {mesh_message_t message;char sender[40],delivery[80];} chat_export_row_t;
typedef struct {size_t bytes;uint32_t hash;} chat_export_stats_t;
typedef struct {chat_export_row_t *samples;size_t count;uint32_t peer,created;uint8_t channel;bool ok;const char *error;chat_export_stats_t csv;char filename[64];} export_job_t;
static atomic_int export_job_state;
static export_job_t *export_completion;
static mesh_chat_export_status_t export_status;
static bool export_directory(const char *path){struct stat st;if(!stat(path,&st))return S_ISDIR(st.st_mode);if(errno!=ENOENT)return false;if(!mkdir(path,0700))return true;return errno==EEXIST&&!stat(path,&st)&&S_ISDIR(st.st_mode);}
static bool export_mount(void){
#ifdef ESP_PLATFORM
 if(!mesh_map_storage_mount())return false;
#endif
 return export_directory(MESH_EXPORT_ROOT)&&export_directory(EXPORT_DIR);
}
static bool export_verified(const char *path,const chat_export_stats_t *expected){
 FILE *f=fopen(path,"rb");if(!f)return false;uint8_t bytes[1024];size_t n,total=0;uint32_t hash=2166136261u;
 while((n=fread(bytes,1,sizeof(bytes),f))){total+=n;for(size_t i=0;i<n;i++)hash=(hash^bytes[i])*16777619u;}
 bool ok=!ferror(f)&&total==expected->bytes&&hash==expected->hash;if(fclose(f))ok=false;return ok;
}
static bool chat_emit(FILE *f,export_job_t *j,const char *s){
 size_t n=strlen(s);if(fwrite(s,1,n,f)!=n)return false;
 j->csv.bytes+=n;for(size_t i=0;i<n;i++)j->csv.hash=(j->csv.hash^(uint8_t)s[i])*16777619u;return true;
}
static bool chat_text_write(FILE *f,export_job_t *j){
 j->csv=(chat_export_stats_t){.hash=2166136261u};char line[240];
 if(j->peer)snprintf(line,sizeof(line),"MeshMatsu direct conversation: !%08lx\n",(unsigned long)j->peer);
 else snprintf(line,sizeof(line),"MeshMatsu channel conversation: slot %u\n",j->channel);
 if(!chat_emit(f,j,line))return false;
 snprintf(line,sizeof(line),"%u retained messages, oldest first. Times are UTC.\nNames and delivery state were captured at export.\nPlain text; keep private if the conversation is private.\n",(unsigned)j->count);
 if(!chat_emit(f,j,line))return false;
 for(size_t i=0;i<j->count;i++){
  const chat_export_row_t *r=&j->samples[i];const mesh_message_t *m=&r->message;
  char utc[40]="Time unavailable";if(m->timestamp>=1700000000){time_t stamp=m->timestamp;struct tm date;if(gmtime_r(&stamp,&date))strftime(utc,sizeof(utc),"%Y-%m-%d %H:%M:%S UTC",&date);}
  snprintf(line,sizeof(line),"\n--- %u | %s | %s ---\n",(unsigned)i+1,m->outgoing?"Sent":"Received",utc);
  if(!chat_emit(f,j,line)||!chat_emit(f,j,"From: ")||!chat_emit(f,j,r->sender))return false;
  snprintf(line,sizeof(line)," (%s)\nPacket: %lu | Reply to: %lu | Timestamp: %lu\n",m->from,(unsigned long)m->packet_id,(unsigned long)m->reply_id,(unsigned long)m->timestamp);
  if(!chat_emit(f,j,line))return false;
  if(m->outgoing&&(!chat_emit(f,j,"Delivery: ")||!chat_emit(f,j,r->delivery)||!chat_emit(f,j,"\n")))return false;
  if(!chat_emit(f,j,"\n")||!chat_emit(f,j,m->text)||!chat_emit(f,j,"\n"))return false;
 }
 return !ferror(f);
}
static bool export_write(export_job_t *job){
 char final[240],partial[248];FILE *file=NULL;
 for(unsigned sequence=1;sequence<=999;sequence++){
  snprintf(job->filename,sizeof(job->filename),"chat-%08lx-c%u-%010lu-%03u.txt",(unsigned long)job->peer,job->channel,(unsigned long)job->created,sequence);
  snprintf(final,sizeof(final),EXPORT_DIR "/%s",job->filename);snprintf(partial,sizeof(partial),"%s.part",final);
  struct stat st;if(!stat(final,&st))continue;if(errno!=ENOENT)return false;
  int fd=open(partial,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0){if(errno==EEXIST)continue;return false;}
  file=fdopen(fd,"wb");if(!file){close(fd);remove(partial);return false;}break;
 }
 if(!file)return false;
 bool ok=chat_text_write(file,job);
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
 if(!export_mount())job->error="SD unavailable. Check the card, then retry Export Conversation.";
 else if(!(job->ok=export_write(job)))job->error="Export not saved. Check SD space/card, then retry Export Conversation.";
 export_completion=job;atomic_store_explicit(&export_job_state,2,memory_order_release);
#ifdef ESP_PLATFORM
 vTaskDelete(NULL);
#endif
}
bool mesh_chat_export_poll(void){
 if(atomic_load_explicit(&export_job_state,memory_order_acquire)!=2)return false;
 export_job_t *job=export_completion;export_status.busy=false;export_status.success=job->ok;
 if(job->ok){snprintf(export_status.filename,sizeof(export_status.filename),"%s",job->filename);snprintf(export_status.message,sizeof(export_status.message),"Saved %u messages: meshmatsu/exports/%s",(unsigned)job->count,job->filename);}
 else snprintf(export_status.message,sizeof(export_status.message),"%s",job->error?job->error:"Export failed. Retry Export Conversation.");
 free(job->samples);free(job);export_completion=NULL;atomic_store_explicit(&export_job_state,0,memory_order_release);return true;
}
bool mesh_chat_export_busy(void){return atomic_load(&export_job_state)!=0;}
mesh_chat_export_status_t mesh_chat_export_status(void){return export_status;}
mesh_chat_export_result_t mesh_chat_export_start(uint32_t peer,uint8_t channel){
 if(mesh_chat_export_busy())return MESH_CHAT_EXPORT_BUSY;
 if(peer==UINT32_MAX||(!peer&&channel>=MESH_CHANNEL_LIMIT))return MESH_CHAT_EXPORT_INVALID;
 size_t count=peer?mesh_radio_conversation_count(peer):mesh_radio_channel_message_count(channel);
 if(!count)return MESH_CHAT_EXPORT_EMPTY;
 if(count>MESH_MESSAGE_HISTORY_MAX)return MESH_CHAT_EXPORT_INVALID;
 export_job_t *job=calloc(1,sizeof(*job));if(!job)return MESH_CHAT_EXPORT_MEMORY;
#ifdef ESP_PLATFORM
 job->samples=heap_caps_malloc(count*sizeof(*job->samples),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#else
 job->samples=malloc(count*sizeof(*job->samples));
#endif
 if(!job->samples){free(job);return MESH_CHAT_EXPORT_MEMORY;}
 // Snapshot only on the main task. The worker never touches radio/model state.
 for(size_t i=0;i<count;i++){
  chat_export_row_t *r=&job->samples[i];
  bool found=peer?mesh_radio_get_conversation_message(peer,i,&r->message):mesh_radio_get_channel_message(channel,i,&r->message);
  if(!found){free(job->samples);free(job);return MESH_CHAT_EXPORT_INVALID;}
  mesh_radio_message_sender_name(&r->message,r->sender,sizeof(r->sender));
  snprintf(r->delivery,sizeof(r->delivery),"%s",mesh_radio_delivery_text(&r->message));
 }
 job->count=count;job->peer=peer;job->channel=peer?0:channel;time_t now=time(NULL);job->created=now>=1700000000&&(uint64_t)now<=UINT32_MAX?(uint32_t)now:0;
 export_status=(mesh_chat_export_status_t){.busy=true,.peer=peer,.channel=job->channel,.count=count};snprintf(export_status.message,sizeof(export_status.message),"Exporting %u messages to SD...",(unsigned)count);
 atomic_store(&export_job_state,1);
#ifdef ESP_PLATFORM
 if(xTaskCreate(export_worker,"chat_export",12*1024,job,tskIDLE_PRIORITY+1,NULL)!=pdPASS){atomic_store(&export_job_state,0);export_status.busy=false;snprintf(export_status.message,sizeof(export_status.message),"Not enough memory. Retry Export Conversation.");free(job->samples);free(job);return MESH_CHAT_EXPORT_MEMORY;}
#else
 export_worker(job);
#endif
 return MESH_CHAT_EXPORT_OK;
}
