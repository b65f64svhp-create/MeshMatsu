"""Independently parse actual CSV and exercise the real ESP deferred-worker branch."""
from pathlib import Path
import subprocess,tempfile,csv,os
r=Path(__file__).resolve().parents[1]
base='''#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#define EXT_RAM_BSS_ATTR
#include "telemetry_data.h"
#include "telemetry_data.inc"
#include "telemetry_csv.c"
'''
format_test=base+r'''
static void save(const char *name,mesh_tlm_sample_t *s,size_t n){FILE *f=fopen(name,"wb");assert(f);mesh_tlm_csv_stats_t stats;assert(mesh_tlm_csv_write(f,s,n,&stats));assert(!fclose(f));}
int main(void){
 mesh_tlm_sample_t s[3]={{.node=42,.kind=3,.packet=UINT32_MAX,.time=1800000000,.received_at=1800000060,.present=3|(1u<<21),.value={[0]=-20,[1]=0,[21]=-5}},
 {.node=42,.kind=3,.flags=MESH_TLM_CACHED|MESH_TLM_RESTORED,.present=1,.value={0}},
 {.node=42,.kind=3,.flags=MESH_TLM_CLOCK_SKEW,.time=1800000000,.received_at=1799999999,.present=1,.value={10}}};save("environment.csv",s,3);
 s[0]=(mesh_tlm_sample_t){.node=42,.kind=2,.present=3,.value={101,0}};save("device.csv",s,1);
 s[0]=(mesh_tlm_sample_t){.node=42,.kind=5,.present=2,.value={[1]=-120}};save("power.csv",s,1);
 s[0]=(mesh_tlm_sample_t){.node=42,.kind=4,.present=1|4096,.value={[0]=0,[12]=UINT32_MAX}};save("air.csv",s,1);
 return 0;}
'''
async_test=base+r'''
void (*test_work)(void *);void *test_arg;int test_task_fail,test_memory_fail;bool test_sd=true;
bool mesh_map_storage_mount(void){return test_sd;}
#define MESH_EXPORT_ROOT "async-export"
#include "telemetry_export.c"
static void work(void){assert(test_work);void (*f)(void *)=test_work;test_work=NULL;f(test_arg);}
int main(void){mesh_tlm_sample_t s={.node=42,.kind=3,.packet=1,.present=1,.value={-20}};assert(mesh_tlm_add(&s));
 test_memory_fail=1;assert(mesh_tlm_export_start(42,3)==MESH_EXPORT_MEMORY&&!mesh_tlm_export_busy());test_memory_fail=0;
 test_task_fail=1;assert(mesh_tlm_export_start(42,3)==MESH_EXPORT_MEMORY&&!mesh_tlm_export_busy());test_task_fail=0;
 assert(mesh_tlm_export_start(42,3)==MESH_EXPORT_OK&&mesh_tlm_export_busy()&&!mesh_tlm_export_poll());assert(mesh_tlm_export_start(99,4)==MESH_EXPORT_BUSY);
 s.packet=2;s.value[0]=0;assert(mesh_tlm_add(&s));mesh_tlm_forget(42); // model changes after the immutable snapshot, before the worker runs
 work();assert(mesh_tlm_export_busy()&&mesh_tlm_export_status().busy);assert(mesh_tlm_export_poll()&&!mesh_tlm_export_busy());mesh_tlm_export_status_t status=mesh_tlm_export_status();assert(status.success&&status.node==42&&status.kind==3&&status.reports==1);
 char path[240],bytes[12000];snprintf(path,sizeof(path),EXPORT_DIR "/%s",status.filename);FILE *f=fopen(path,"rb");assert(f);size_t n=fread(bytes,1,sizeof(bytes)-1,f);bytes[n]=0;fclose(f);assert(strstr(bytes,"\"Temperature\",-20,\"C\",reported")&&!strstr(bytes,"\"Temperature\",0,"));
 s.node=99;s.kind=4;s.value[0]=0;assert(mesh_tlm_add(&s));test_sd=false;assert(mesh_tlm_export_start(99,4)==MESH_EXPORT_OK);work();mesh_tlm_export_poll();assert(!mesh_tlm_export_status().success);test_sd=true;assert(mesh_tlm_export_start(99,4)==MESH_EXPORT_OK);work();mesh_tlm_export_poll();assert(mesh_tlm_export_status().success&&mesh_tlm_export_status().node==99&&mesh_tlm_export_status().kind==4);
 puts("PASS: real ESP deferred task, immutable snapshot amid new/forgotten reports, frozen target/type, busy/completion lifecycle, PSRAM/task/mount failures and retry");return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)
 def build_run(source,name,flags=[]):
  (p/(name+'.c')).write_text(source)
  subprocess.run(['clang','-std=c11','-fsanitize=address,undefined',*flags,'-I',d,'-I',str(r/'tests/stubs'),'-I',str(r/'main'),str(p/(name+'.c')),'-lm','-o',str(p/name)],check=True)
  subprocess.run([str(p/name)],cwd=d,env=dict(os.environ,TZ='Pacific/Honolulu'),check=True)
 build_run(format_test,'format')
 def rows(name):
  with (p/name).open(newline='') as f:return list(csv.DictReader(f))
 env=rows('environment.csv');assert len(env)==66 and all(len(x)==15 and None not in x for x in env)
 temp=env[0];assert temp['value']=='-20' and temp['unit']=='C' and temp['measurement_utc']=='2027-01-15T08:00:00Z' and temp['packet_id']==str(2**32-1)
 assert env[1]['value']=='0' and env[1]['value_status']=='reported' and env[2]['value']=='' and env[2]['value_status']=='missing'
 assert env[22]['cached']=='1' and env[22]['restored']=='1' and env[22]['measurement_unix']==env[22]['measurement_utc']==env[22]['received_unix']==env[22]['received_utc']==''
 assert env[44]['clock_skew']=='1' and env[44]['measurement_unix']=='1800000000'
 dev=rows('device.csv');assert dev[0]['value']=='101' and dev[0]['unit']=='code' and dev[0]['value_status']=='external_power' and dev[1]['value']=='0'
 power=rows('power.csv');assert power[1]['value']=='-120' and power[1]['unit']=='mA' and power[0]['value']==''
 air=rows('air.csv');assert air[0]['value']=='0' and air[12]['value']==str(2**32-1)
 (p/'freertos').mkdir();(p/'pax_gfx.h').write_text('#pragma once\ntypedef struct {int unused;} pax_buf_t;\n')
 (p/'esp_heap_caps.h').write_text('#pragma once\n#include <stdlib.h>\nextern int test_memory_fail;\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\nstatic inline void *heap_caps_malloc(size_t n,int caps){return test_memory_fail?NULL:malloc(n);}\n')
 (p/'freertos/FreeRTOS.h').write_text('#pragma once\n#define pdPASS 1\n#define tskIDLE_PRIORITY 0\n')
 (p/'freertos/task.h').write_text('#pragma once\nextern void (*test_work)(void *);extern void *test_arg;extern int test_task_fail;\nstatic inline int xTaskCreate(void (*f)(void *),const char *n,unsigned stack,void *arg,unsigned priority,void *handle){assert(stack>=12*1024&&priority==1);if(test_task_fail)return 0;test_work=f;test_arg=arg;return 1;}\nstatic inline void vTaskDelete(void *t){}\n')
 build_run(async_test,'async',['-DESP_PLATFORM'])
 print('PASS: independent CSV parsing, 15-column schema, UTC unaffected by local timezone, missing/zero/signed/external-power/uint32 values, cache/restore/skew flags and deferred snapshot export')
