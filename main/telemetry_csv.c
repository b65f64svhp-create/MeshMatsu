#include "telemetry_csv.h"
#include <string.h>
#include <time.h>
typedef struct {FILE *file;bool ok;mesh_tlm_csv_stats_t stats;} csv_writer_t;
static void csv_emit(csv_writer_t *w,const char *text){
 if(!w->ok)return;
 size_t n=strlen(text);if(fwrite(text,1,n,w->file)!=n){w->ok=false;return;}
 w->stats.bytes+=n;for(size_t i=0;i<n;i++)w->stats.hash=(w->stats.hash^(uint8_t)text[i])*16777619u;
}
static void csv_text(csv_writer_t *w,const char *text){csv_emit(w,"\"");for(;*text;text++){char c[]={*text,0};csv_emit(w,c);if(*text=='"')csv_emit(w,"\"");}csv_emit(w,"\"");}
static void csv_time(uint32_t stamp,char *raw,size_t raw_size,char *utc,size_t utc_size){
 raw[0]=utc[0]=0;if(!stamp)return;snprintf(raw,raw_size,"%lu",(unsigned long)stamp);
 if(stamp<1700000000)return;
 time_t value=stamp;struct tm date;if(gmtime_r(&value,&date))strftime(utc,utc_size,"%Y-%m-%dT%H:%M:%SZ",&date);
}
bool mesh_tlm_csv_write(FILE *file,const mesh_tlm_sample_t *samples,size_t count,mesh_tlm_csv_stats_t *stats){
 if(stats)*stats=(mesh_tlm_csv_stats_t){0};
 if(!file||!samples||!stats||!count||count>MESH_TLM_SERIES_LIMIT)return false;
 for(size_t i=0;i<count;i++)if(!mesh_tlm_valid(&samples[i])||samples[i].node!=samples[0].node||samples[i].kind!=samples[0].kind)return false;
 csv_writer_t w={.file=file,.ok=true,.stats={.hash=2166136261u}};
 csv_emit(&w,"report,node_id,report_type,packet_id,measurement_unix,measurement_utc,received_unix,received_utc,cached,restored,clock_skew,metric,value,unit,value_status\r\n");
 for(size_t i=0;w.ok&&i<count;i++){
  const mesh_tlm_sample_t *s=&samples[i];char measured[16],received[16],mu[32],ru[32],packet[16]={0},prefix[224];
  csv_time(s->time,measured,sizeof(measured),mu,sizeof(mu));csv_time(s->received_at,received,sizeof(received),ru,sizeof(ru));if(s->packet)snprintf(packet,sizeof(packet),"%lu",(unsigned long)s->packet);
  snprintf(prefix,sizeof(prefix),"%u,!%08lx,%s,%s,%s,%s,%s,%s,%u,%u,%u,",(unsigned)i+1,(unsigned long)s->node,mesh_tlm_family(s->kind),packet,measured,mu,received,ru,(s->flags&MESH_TLM_CACHED)!=0,(s->flags&MESH_TLM_RESTORED)!=0,(s->flags&MESH_TLM_CLOCK_SKEW)!=0);
  for(unsigned f=1;w.ok&&f<=MESH_TLM_FIELDS;f++){
   const mesh_tlm_field_t *info=mesh_tlm_field(s->kind,f);if(!info)break;
   bool present=(s->present&(1u<<(f-1)))!=0,external=present&&s->kind==MESH_TLM_DEVICE&&f==1&&s->value[0]>100;
   csv_emit(&w,prefix);csv_text(&w,info->name);csv_emit(&w,",");
   if(present){char number[64];snprintf(number,sizeof(number),info->integer?"%.0f":"%.9g",s->value[f-1]);csv_emit(&w,number);}
   csv_emit(&w,",");csv_text(&w,external?"code":info->unit);csv_emit(&w,",");csv_emit(&w,!present?"missing":external?"external_power":"reported");csv_emit(&w,"\r\n");w.stats.rows++;
  }
 }
 *stats=w.stats;return w.ok&&!ferror(file);
}
