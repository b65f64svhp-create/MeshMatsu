"""Run the actual ESP archive worker with deferred tasks and independent decoding."""
from pathlib import Path
import os,struct,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
prefix=(r/'tests/test_message_archive.c').read_text().split('static int64_t now=')[0]
prefix=prefix.replace('#include "message_archive.c"',r'''
void (*test_work)(void *);void *test_arg;
static int task_failure,allocation_failure;static bool sd_available=true;
bool mesh_map_storage_mount(void){return sd_available;}
#include "message_archive.c"
''')
harness=prefix+r'''
static int64_t now=1000000;
static void work(void){assert(test_work);void (*f)(void *)=test_work;test_work=NULL;f(test_arg);}
static void finish(void){work();assert(atomic_load(&ar_state)==2);now+=1000000;mesh_archive_poll(now);}
int main(void){
 mesh_archive_init();mesh_archive_poll(now);assert(test_work);finish();assert(mesh_archive_configure(true,5000));mesh_archive_poll(++now);finish();
 mesh_message_t m={.packet_id=7,.timestamp=1800000000,.peer_node=42,.channel=2,.outgoing=true,.delivery=MESH_DELIVERY_ACKNOWLEDGED,.reply_id=88};strcpy(m.from,"You");strcpy(m.text,"Frozen it's working 🙂");
 mesh_reaction_t reaction={.sender=99,.packet_id=8,.reply_id=7,.peer_node=42,.channel=2,.timestamp=1800000001};strcpy(reaction.emoji,"👍");
 mesh_archive_capture(&m,&reaction,1,"Frozen Name");now+=1000000;mesh_archive_poll(now);assert(test_work&&mesh_archive_status().busy&&mesh_archive_status().volatile_count==1);
 strcpy(m.text,"changed after queue");strcpy(reaction.emoji,"changed");assert(!mesh_archive_request(0,"",true,0,0));
 m.packet_id=9;mesh_archive_capture(&m,NULL,0,"Next name");assert(mesh_archive_status().volatile_count==2);finish();assert(ar_status.count==1&&ar_queue_count==1);
 now+=1000000;mesh_archive_poll(now);finish();assert(ar_status.count==2&&!ar_queue_count);
 assert(mesh_archive_request(0,"FROZEN",false,42,0));work();assert(!ar_page.ready);now+=1000000;mesh_archive_poll(now);assert(ar_page.ready&&ar_page.count==1&&!strcmp(ar_page.rows[0].message.text,"Frozen it's working 🙂")&&!strcmp(ar_page.rows[0].reactions,"👍"));
 // A lost card leaves durable pending data, then recovers on a later worker.
 sd_available=false;m.packet_id=10;mesh_archive_capture(&m,NULL,0,"Pending");now+=1000000;mesh_archive_poll(now);finish();assert(ar_status.failed&&ar_status.pending==1&&mesh_archive_flush(now)==1);
 sd_available=true;now+=31000000;mesh_archive_poll(now);finish();assert(ar_status.count==3&&!ar_status.pending);
 // Failed task creation and memory allocation cannot transfer ownership or
 // replace a valid current file with an older backup.
 m.packet_id=11;mesh_archive_capture(&m,NULL,0,"Retry");task_failure=1;now+=1000000;mesh_archive_poll(now);assert(ar_queue_count==1&&!test_work&&ar_status.failed);task_failure=0;
 now+=31000000;mesh_archive_poll(now);finish();assert(ar_status.count==4);
 allocation_failure=1;assert(!mesh_archive_request(0,"",true,0,0)&&!ar_status.busy);allocation_failure=0;
 char src[256];size_t count=0;allocation_failure=1;assert(ar_source(AR_PATH,src,AR_MAX,&count)==AR_UNAVAILABLE);allocation_failure=0;assert(ar_source(AR_PATH,src,AR_MAX,&count)==AR_VALID&&count==4);
 // Write result files survive process exit for independent Python decoding.
 puts("PASS: actual ESP worker ownership, queued arrivals, read publication, SD retry, task/PSRAM failures and no OOM backup downgrade");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'freertos').mkdir();(p/'pax_gfx.h').write_text('#pragma once\ntypedef struct {int unused;} pax_buf_t;\n')
 (p/'esp_heap_caps.h').write_text('''#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static inline void *heap_caps_calloc(size_t n,size_t s,int caps){assert(caps==3&&n*s<100000);return allocation_failure?NULL:calloc(n,s);}
''')
 (p/'freertos/FreeRTOS.h').write_text('#pragma once\n#define pdPASS 1\n#define tskIDLE_PRIORITY 0\n')
 (p/'freertos/task.h').write_text('''#pragma once
static inline int xTaskCreate(void (*f)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle){assert(stack==12*1024&&priority==1&&!test_work);if(task_failure)return 0;test_work=f;test_arg=arg;return 1;}
static inline void vTaskDelete(void *task){}
''')
 (p/'test.c').write_text(harness)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-DESP_PLATFORM','-I',d,'-I',str(r/'tests/stubs'),'-I',str(r/'main'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
 data=(p/'archive-sd/messages.msa').read_bytes();magic,version,count,reserved=struct.unpack_from('<4sIII',data)
 assert(magic,version,count,reserved)==(b'MSA1',1,4,0)
 def fnv(value,chunk):
  for byte in chunk:value=((value^byte)*16777619)&0xffffffff
  return value
 offset=16;rows=[]
 for i in range(count):
  size,checksum=struct.unpack_from('<II',data,offset);offset+=8;payload=data[offset:offset+size];offset+=size
  assert fnv(2166136261,payload)==checksum
  lo,hi=struct.unpack_from('<II',payload);text=payload[20:254].split(b'\0')[0].decode();sender=payload[254:294].split(b'\0')[0].decode()
  timestamp,packet=struct.unpack_from('<II',payload,294);delivery=payload[302];peer,reply,error=struct.unpack_from('<III',payload,303);failure,outgoing,channel=payload[315:318];reactions=struct.unpack_from('<H',payload,318)[0]
  assert size==320+85*reactions;rows.append((text,sender,packet,peer,reply,delivery,channel,reactions))
 assert rows[0]==("Frozen it's working 🙂",'Frozen Name',7,42,88,2,2,1)
 assert data[16+8+320+21:16+8+320+85].split(b'\0')[0].decode()=='👍'
 assert data[offset:offset+4]==b'END1' and struct.unpack_from('<I',data,offset+4)[0]==fnv(2166136261,data[:offset]) and offset+8==len(data)
 print('PASS: independent little-endian archive decoding, complete-file/record hashes, UTF-8, reply, scope, delivery and full reaction payload')
