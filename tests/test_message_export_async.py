"""Exercise production ESP export worker with a frozen model and deferred task."""
from pathlib import Path
import subprocess,tempfile,os
r=Path(__file__).resolve().parents[1]
harness=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "radio_link.h"
static mesh_message_t model[1000];static size_t count=1;
size_t mesh_radio_conversation_count(uint32_t p){return p==42?count:0;}
size_t mesh_radio_channel_message_count(uint8_t c){return c==1?count:0;}
bool mesh_radio_get_conversation_message(uint32_t p,size_t i,mesh_message_t *m){if(p!=42||i>=count)return false;*m=model[i];m->peer_node=p;return true;}
bool mesh_radio_get_channel_message(uint8_t c,size_t i,mesh_message_t *m){if(c!=1||i>=count)return false;*m=model[i];m->channel=c;return true;}
void mesh_radio_message_sender_name(const mesh_message_t *m,char *s,size_t n){snprintf(s,n,"Original sender");}
const char *mesh_radio_delivery_text(const mesh_message_t *m){return "Delivered to mesh.";}
void (*test_work)(void *);void *test_arg;int test_task_fail,test_memory_fail;bool test_sd=true;
bool mesh_map_storage_mount(void){return test_sd;}
#define MESH_EXPORT_ROOT "async-chat"
#include "message_export.c"
static void work(void){assert(test_work);void (*f)(void *)=test_work;test_work=NULL;f(test_arg);}
int main(void){
 strcpy(model[0].text,"it's working 🙂\nprivate text");strcpy(model[0].from,"!11223344");model[0].timestamp=1800000000;model[0].packet_id=123;
 test_memory_fail=1;assert(mesh_chat_export_start(0,1)==MESH_CHAT_EXPORT_MEMORY&&!mesh_chat_export_busy());test_memory_fail=0;
 test_task_fail=1;assert(mesh_chat_export_start(0,1)==MESH_CHAT_EXPORT_MEMORY&&!mesh_chat_export_busy());test_task_fail=0;
 assert(mesh_chat_export_start(0,1)==MESH_CHAT_EXPORT_OK&&mesh_chat_export_busy()&&!mesh_chat_export_poll());assert(mesh_chat_export_start(42,0)==MESH_CHAT_EXPORT_BUSY);
 strcpy(model[0].text,"replacement");model[0].packet_id=999;count=0; // arrivals/deletion/screen changes cannot alter the worker snapshot
 work();assert(mesh_chat_export_busy()&&mesh_chat_export_status().busy);assert(mesh_chat_export_poll()&&!mesh_chat_export_busy());mesh_chat_export_status_t status=mesh_chat_export_status();assert(status.success&&status.count==1&&!status.peer&&status.channel==1);
 char path[240],text[5000];snprintf(path,sizeof(path),EXPORT_DIR "/%s",status.filename);FILE *f=fopen(path,"rb");assert(f);size_t n=fread(text,1,sizeof(text)-1,f);text[n]=0;fclose(f);assert(strstr(text,"it's working 🙂\nprivate text")&&strstr(text,"Original sender")&&strstr(text,"Packet: 123")&&!strstr(text,"replacement"));
 count=1;test_sd=false;assert(mesh_chat_export_start(42,0)==MESH_CHAT_EXPORT_OK);work();mesh_chat_export_poll();assert(!mesh_chat_export_status().success&&mesh_chat_export_status().peer==42);
 test_sd=true;count=1000;for(size_t i=0;i<count;i++){model[i].packet_id=i+1;snprintf(model[i].text,sizeof(model[i].text),"retained row %u",(unsigned)i+1);}
 assert(mesh_chat_export_start(42,0)==MESH_CHAT_EXPORT_OK);work();mesh_chat_export_poll();status=mesh_chat_export_status();assert(status.success&&status.count==1000&&status.peer==42);
 snprintf(path,sizeof(path),EXPORT_DIR "/%s",status.filename);f=fopen(path,"rb");assert(f);assert(!fseek(f,-200,SEEK_END));n=fread(text,1,sizeof(text)-1,f);text[n]=0;fclose(f);assert(strstr(text,"retained row 1000"));
 puts("PASS: actual ESP deferred worker, frozen text/name/metadata/scope, 1000-message bound, PSRAM/task/mount failure and recovery");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'freertos').mkdir();(p/'pax_gfx.h').write_text('#pragma once\ntypedef struct {int unused;} pax_buf_t;\n')
 (p/'esp_heap_caps.h').write_text('#pragma once\n#include <stdlib.h>\nextern int test_memory_fail;\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\nstatic inline void *heap_caps_malloc(size_t n,int caps){assert(n<=1000*512);return test_memory_fail?NULL:malloc(n);}\n')
 (p/'freertos/FreeRTOS.h').write_text('#pragma once\n#define pdPASS 1\n#define tskIDLE_PRIORITY 0\n')
 (p/'freertos/task.h').write_text('#pragma once\nextern void (*test_work)(void *);extern void *test_arg;extern int test_task_fail;\nstatic inline int xTaskCreate(void (*f)(void *),const char *n,unsigned stack,void *arg,unsigned priority,void *handle){assert(stack>=12*1024&&priority==1);if(test_task_fail)return 0;test_work=f;test_arg=arg;return 1;}\nstatic inline void vTaskDelete(void *t){}\n')
 (p/'test.c').write_text(harness)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-DESP_PLATFORM','-I',d,'-I',str(r/'tests/stubs'),'-I',str(r/'main'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,env=dict(os.environ,TZ='Pacific/Honolulu'),check=True)
