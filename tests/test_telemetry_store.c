#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define EXT_RAM_BSS_ATTR
#include "telemetry_data.h"
#include "telemetry_data.inc"
#define MESH_TLM_DIRECTORY "telemetry_test"
#include "telemetry_store.c"
static void restart(void){assert(!atomic_load(&job_state));mesh_tlm_reset();loaded=force_save=last_error=false;next_attempt=saved_revision=0;merge_started=merge_had_live=false;merge_position=merge_expected=0;}
static void cleanup(void){remove(TLM_PATH);remove(TLM_BACKUP);remove(TLM_TEMP);rmdir(MESH_TLM_DIRECTORY);}
static void add(unsigned id,double value){mesh_tlm_sample_t s={.node=42,.kind=3,.present=1,.packet=id,.value={value},.received_us=100};if(!mesh_tlm_add(&s)){fprintf(stderr,"add failed id=%u value=%f count=%zu\n",id,value,mesh_tlm_count());assert(false);}}
static void poll_until_done(int64_t now){for(unsigned i=0;i<2000;i++){mesh_tlm_store_poll(now,false);if(!atomic_load(&job_state))return;}assert(false);}
int main(void){
 cleanup();restart();mesh_tlm_store_poll(1,false);assert(atomic_load(&job_state));poll_until_done(1);assert(loaded&&!last_error);
 for(unsigned i=1;i<=12;i++)add(i,-20.0+i);assert(mesh_tlm_store_flush(2)==0);poll_until_done(2);assert(mesh_tlm_store_flush(2)==1&&!last_error);
 struct stat st;assert(!stat(TLM_PATH,&st)&&st.st_size==20+12*200);
 restart();add(100,30);mesh_tlm_store_poll(1,false);mesh_tlm_store_poll(1,true);assert(mesh_tlm_count()==1&&!loaded);mesh_tlm_store_poll(1,false);assert(mesh_tlm_count()==5&&!loaded); // bounded merge
 add(101,31);poll_until_done(1);mesh_tlm_sample_t s;assert(loaded&&mesh_tlm_count()==14&&mesh_tlm_latest(42,3,1,&s)&&s.packet==101&&s.received_us==100);
 mesh_tlm_sample_t series[96];assert(mesh_tlm_series(42,3,series,96)==14&&series[0].packet==1&&(series[0].flags&MESH_TLM_RESTORED)&&!series[0].received_us);
 assert(mesh_tlm_store_flush(2)==0);poll_until_done(2);assert(!stat(TLM_BACKUP,&st));
 // Truncated primary recovers the previous verified snapshot; corrupt reads never replace it.
 FILE *f=fopen(TLM_PATH,"wb");assert(f);fputs("MMTEL001",f);fclose(f);restart();mesh_tlm_store_poll(1,false);poll_until_done(1);assert(loaded&&!last_error&&mesh_tlm_count()==12);
 // Same format checksum rejects a flipped value, and failed restore keeps live RAM data.
 remove(TLM_BACKUP);f=fopen(TLM_PATH,"wb");assert(f);uint8_t garbage[25]={0};memcpy(garbage,"MMTEL001",8);tlm_put32(garbage+8,1);tlm_put32(garbage+12,200);fwrite(garbage,1,sizeof(garbage),f);fclose(f);
 restart();add(200,-40);mesh_tlm_store_poll(1,false);poll_until_done(1);assert(!loaded&&last_error&&mesh_tlm_count()==1&&mesh_tlm_store_flush(2)==-1);
 assert(!stat(TLM_PATH,&st)&&st.st_size==25);remove(TLM_PATH);mesh_tlm_store_retry();poll_until_done(3);assert(loaded&&!last_error);assert(mesh_tlm_store_flush(4)==0);poll_until_done(4);assert(mesh_tlm_store_flush(4)==1);
 // Removing an ignored node persists an empty snapshot rather than restoring old readings.
 mesh_tlm_forget(42);assert(!mesh_tlm_count()&&mesh_tlm_store_flush(5)==0);poll_until_done(5);assert(mesh_tlm_store_flush(5)==1);restart();mesh_tlm_store_poll(1,false);poll_until_done(1);assert(loaded&&!mesh_tlm_count());
 add(300,-10);assert(mesh_tlm_store_flush(2)==0);poll_until_done(2);
 tlm_job_t read={0};assert(tlm_read_file(TLM_PATH,&read)&&read.count==1);free(read.samples);
 f=fopen(TLM_PATH,"ab");assert(f);fputc(0,f);fclose(f);assert(!tlm_read_file(TLM_PATH,&read));assert(!truncate(TLM_PATH,220));
 f=fopen(TLM_PATH,"r+b");assert(f);assert(!fseek(f,24,SEEK_SET));int c=fgetc(f);assert(!fseek(f,24,SEEK_SET));fputc(c^1,f);fclose(f);assert(!tlm_read_file(TLM_PATH,&read));
 cleanup();restart();f=fopen(MESH_TLM_DIRECTORY,"wb");assert(f);fclose(f);add(1,0);mesh_tlm_store_poll(1,false);poll_until_done(1);assert(last_error&&!loaded&&mesh_tlm_store_flush(2)==-1);remove(MESH_TLM_DIRECTORY);
 cleanup();puts("PASS: checksummed SD round-trip, signed values, async/batched restore, simultaneous live precedence, backup recovery, corrupt/truncated/extra reads, failed SD preserves RAM/files, explicit retry and safe exit flush");
}
