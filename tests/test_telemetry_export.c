#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#define EXT_RAM_BSS_ATTR
#include "telemetry_data.h"
#include "telemetry_data.inc"
#include "telemetry_csv.c"
#define MESH_EXPORT_ROOT "csv-test"
#include "telemetry_export.c"
static mesh_tlm_sample_t sample(uint8_t kind,uint32_t id,double value){return (mesh_tlm_sample_t){.node=42,.kind=kind,.packet=id,.present=1,.time=1800000000+id,.received_at=1800000100+id,.value={value}};}
static void cleanup(void){DIR *d=opendir(EXPORT_DIR);if(d){struct dirent *e;while((e=readdir(d)))if(e->d_name[0]!='.'){char p[300];snprintf(p,sizeof(p),EXPORT_DIR "/%s",e->d_name);remove(p);}closedir(d);}rmdir(EXPORT_DIR);rmdir(MESH_EXPORT_ROOT);}
int main(void){cleanup();mesh_tlm_reset();assert(mesh_tlm_export_start(0,3)==MESH_EXPORT_INVALID&&mesh_tlm_export_start(42,1)==MESH_EXPORT_INVALID&&mesh_tlm_export_start(42,3)==MESH_EXPORT_EMPTY&&!mesh_tlm_export_busy());
 mesh_tlm_sample_t s=sample(3,1,-20);s.present=3;s.value[1]=0;assert(mesh_tlm_add(&s));assert(mesh_tlm_export_start(42,3)==MESH_EXPORT_OK&&mesh_tlm_export_busy());assert(mesh_tlm_export_start(99,2)==MESH_EXPORT_BUSY);assert(mesh_tlm_export_status().busy&&mesh_tlm_export_status().node==42&&mesh_tlm_export_status().kind==3);
 s=sample(3,2,0);assert(mesh_tlm_add(&s));assert(mesh_tlm_export_poll()&&!mesh_tlm_export_poll()&&!mesh_tlm_export_busy());mesh_tlm_export_status_t a=mesh_tlm_export_status();assert(a.success&&a.reports==1&&a.rows==22&&strstr(a.message,"Saved 1 report"));char path[240];snprintf(path,sizeof(path),EXPORT_DIR "/%s",a.filename);FILE *f=fopen(path,"rb");assert(f);char contents[12000];size_t bytes=fread(contents,1,sizeof(contents)-1,f);contents[bytes]=0;fclose(f);assert(strstr(contents,"\"Temperature\",-20,\"C\",reported")&&strstr(contents,"\"Humidity\",0,\"%\",reported")&&strstr(contents,"\"Pressure\",,\"hPa\",missing")&&!strstr(contents,"32,\"F\""));
 assert(mesh_tlm_export_start(42,3)==MESH_EXPORT_OK);mesh_tlm_export_poll();mesh_tlm_export_status_t b=mesh_tlm_export_status();assert(b.success&&strcmp(a.filename,b.filename)&&b.reports==2);snprintf(path,sizeof(path),EXPORT_DIR "/%s",a.filename);f=fopen(path,"rb");assert(f);assert(fread(contents,1,sizeof(contents),f)==bytes);fclose(f); // prior export unchanged
 // Fixed creation time exercises numbered collisions and preservation of an abandoned partial file.
 s=sample(3,9,-5);export_job_t j={.samples=&s,.count=1,.node=42,.kind=3,.created=0};assert(export_write(&j));char first[64];snprintf(first,sizeof(first),"%s",j.filename);assert(export_write(&j)&&strcmp(first,j.filename));
 char held[248];snprintf(held,sizeof(held),EXPORT_DIR "/tlm-0000002a-k3-0000000000-003.csv.part");f=fopen(held,"wb");assert(f);fputs("keep",f);fclose(f);assert(export_write(&j)&&strstr(j.filename,"-004.csv"));f=fopen(held,"rb");assert(f);assert(fread(contents,1,4,f)==4&&!memcmp(contents,"keep",4));fclose(f);
 snprintf(path,sizeof(path),EXPORT_DIR "/%s",j.filename);assert(export_verified(path,&j.csv));f=fopen(path,"ab");assert(f);fputc('x',f);fclose(f);assert(!export_verified(path,&j.csv));
 mesh_tlm_csv_stats_t stats;f=tmpfile();assert(f);assert(!mesh_tlm_csv_write(f,NULL,0,&stats));s=sample(3,1,NAN);assert(!mesh_tlm_csv_write(f,&s,1,&stats));fclose(f);
 // CSV string quoting is complete, even though all exported labels are trusted constants.
 f=tmpfile();csv_writer_t writer={.file=f,.ok=true,.stats={.hash=2166136261u}};csv_text(&writer,"comma, quote\" and\r\nline");fflush(f);rewind(f);memset(contents,0,sizeof(contents));fread(contents,1,100,f);assert(!strcmp(contents,"\"comma, quote\"\" and\r\nline\""));fclose(f);
 cleanup();f=fopen(MESH_EXPORT_ROOT,"wb");assert(f);fputs("preserve",f);fclose(f);assert(mesh_tlm_export_start(42,3)==MESH_EXPORT_OK);mesh_tlm_export_poll();assert(!mesh_tlm_export_status().success&&strstr(mesh_tlm_export_status().message,"SD unavailable"));remove(MESH_EXPORT_ROOT);
 assert(mesh_tlm_export_start(42,3)==MESH_EXPORT_OK);mesh_tlm_export_poll();assert(mesh_tlm_export_status().success);cleanup();
 puts("PASS: scoped snapshot export, busy/frozen identity, signed/zero/missing values, canonical units, unique files/preserved exports, verified writes, CSV quoting, invalid input, SD failure and retry");}
