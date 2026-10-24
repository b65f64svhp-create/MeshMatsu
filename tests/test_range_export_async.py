"""Real ESP worker branch, frozen range snapshots and independently parsed CSV."""
from pathlib import Path
import subprocess,tempfile,os,csv
r=Path(__file__).resolve().parents[1]
h=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "range_counter.inc"
static mesh_range_record_t model[512];static size_t count=2;
static mesh_range_status_t session={.origin=42,.mode=MESH_RANGE_RECEIVE,.counter={.received=2,.gaps=1}};
size_t mesh_radio_range_snapshot(mesh_range_record_t *out,size_t capacity,mesh_range_status_t *s){assert(capacity>=count);memcpy(out,model,count*sizeof(*out));*s=session;return count;}
void (*test_work)(void *);void *test_arg;int test_task_fail,test_memory_fail;bool test_sd=true;
bool mesh_map_storage_mount(void){return test_sd;}
#define MESH_EXPORT_ROOT "range-export"
#include "range_export.c"
static void work(void){assert(test_work);void (*f)(void *)=test_work;test_work=NULL;f(test_arg);}
int main(void){
 model[0]=(mesh_range_record_t){.sequence=1,.packet=123,.time=1800000000,.peer=99,.segment=1,.has_snr=true,.snr=0,.has_rssi=true,.rssi=-77,.has_rx_position=true,.rx_lat=551234567,.rx_lon=-1187654321,.rx_fix=1799999900,.rx_precision=20};
 model[1]=(mesh_range_record_t){.sequence=3,.packet=125,.time=1800000060,.peer=99,.segment=1,.gaps=1};
 test_memory_fail=1;assert(mesh_range_export_start()==MESH_RANGE_EXPORT_MEMORY&&!mesh_range_export_busy());test_memory_fail=0;
 test_task_fail=1;assert(mesh_range_export_start()==MESH_RANGE_EXPORT_MEMORY&&!mesh_range_export_busy());test_task_fail=0;
 assert(mesh_range_export_start()==MESH_RANGE_EXPORT_OK&&mesh_range_export_busy()&&!mesh_range_export_poll());assert(mesh_range_export_start()==MESH_RANGE_EXPORT_BUSY);
 model[0].sequence=999;session.origin=999;count=0;work();assert(mesh_range_export_busy());assert(mesh_range_export_poll()&&!mesh_range_export_busy());mesh_range_export_status_t status=mesh_range_export_status();assert(status.success&&status.rows==2);
 char path[240],text[10000];snprintf(path,sizeof(path),EXPORT_DIR "/%s",status.filename);FILE *f=fopen(path,"rb");assert(f);size_t n=fread(text,1,sizeof(text)-1,f);text[n]=0;fclose(f);assert(strstr(text,"!0000002a,!00000063,receive,0,1800000000")&&!strstr(text,",999,"));
 assert(mesh_range_export_start()==MESH_RANGE_EXPORT_EMPTY&&!mesh_range_export_busy());count=2;session.origin=42;model[0].sequence=1;
 test_sd=false;assert(mesh_range_export_start()==MESH_RANGE_EXPORT_OK);work();mesh_range_export_poll();assert(!mesh_range_export_status().success);test_sd=true;
 assert(mesh_range_export_start()==MESH_RANGE_EXPORT_OK);work();mesh_range_export_poll();assert(mesh_range_export_status().success&&strcmp(status.filename,mesh_range_export_status().filename));
 // Exclusive .part files and existing exports survive a fixed-time filename collision.
 export_job_t job={.samples=model,.count=count,.node=42,.session=session};assert(export_write(&job));char old[64];strcpy(old,job.filename);assert(export_write(&job)&&strcmp(old,job.filename));
 snprintf(path,sizeof(path),EXPORT_DIR "/%s",job.filename);assert(export_verified(path,&job.csv));f=fopen(path,"ab");fputc('x',f);fclose(f);assert(!export_verified(path,&job.csv));remove(path);
 for(unsigned i=0;i<512;i++)model[i]=(mesh_range_record_t){.sequence=i+1,.peer=99,.time=1800000000+i,.segment=1};count=512;assert(mesh_range_export_start()==MESH_RANGE_EXPORT_OK);work();mesh_range_export_poll();assert(mesh_range_export_status().success&&mesh_range_export_status().rows==512);
 puts("PASS: frozen ESP range export, background completion, allocation/task/SD failure and retry, bounded rows, exclusive filenames and verified writes");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'freertos').mkdir();(p/'pax_gfx.h').write_text('#pragma once\ntypedef struct {int unused;} pax_buf_t;\n')
 (p/'esp_heap_caps.h').write_text('#pragma once\n#include <stdlib.h>\nextern int test_memory_fail;\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\nstatic inline void *heap_caps_malloc(size_t n,int caps){assert(n<=512*256);return test_memory_fail?NULL:malloc(n);}\n')
 (p/'freertos/FreeRTOS.h').write_text('#pragma once\n#define pdPASS 1\n#define tskIDLE_PRIORITY 0\n')
 (p/'freertos/task.h').write_text('#pragma once\nextern void (*test_work)(void *);extern void *test_arg;extern int test_task_fail;\nstatic inline int xTaskCreate(void (*f)(void *),const char *n,unsigned stack,void *arg,unsigned priority,void *handle){assert(stack>=12*1024&&priority==1);if(test_task_fail)return 0;test_work=f;test_arg=arg;return 1;}\nstatic inline void vTaskDelete(void *t){}\n')
 (p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-DESP_PLATFORM','-I',d,'-I',str(r/'tests/stubs'),'-I',str(r/'main'),str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,env=dict(os.environ,TZ='Pacific/Honolulu'),check=True)
 files=sorted((p/'range-export/exports').glob('*.csv'));assert len(files)>=4
 for f in files:
  with f.open(newline='') as handle:
   rows=list(csv.DictReader(handle));assert len(rows) in (2,512)
   for row in rows:assert len(row)==28 and None not in row
   if len(rows)==2:
    assert rows[0]['snr_db']=='0' and rows[0]['rssi_dbm']=='-77'
    assert rows[0]['local_lat']=='55.1234567' and rows[0]['local_lon']=='-118.7654321'
    assert rows[0]['received_utc']=='2027-01-15T08:00:00Z'
    assert rows[0]['local_precision_bits']=='20' and rows[0]['peer_lat']==''
    assert rows[1]['snr_db']=='' and rows[1]['rssi_dbm']=='' and rows[1]['local_lat']==''
print('PASS: independent CSV parser verifies column counts, original units, UTC under another timezone, zero/missing values and reported locations/precision')
