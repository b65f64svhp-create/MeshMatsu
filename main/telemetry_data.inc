// Bounded pure telemetry data model. No radio transmission, I/O or clock access.
#include <stdlib.h>
#define TF(n,u,lo,hi) {n,u,lo,hi,false,false}
#define TI(n,u,lo,hi) {n,u,lo,hi,true,false}
#define TT(n) {n,"C",-273.15,1000,false,true}
static const mesh_tlm_field_t tlm_device[]={TI("Battery","%",0,UINT32_MAX),TF("Battery voltage","V",0,1000000),TF("Channel use","%",0,100),TF("Transmit airtime","%",0,100),TI("Uptime","s",0,UINT32_MAX)};
static const mesh_tlm_field_t tlm_environment[]={TT("Temperature"),TF("Humidity","%",0,100),TF("Pressure","hPa",0,20000),TF("Gas resistance","MOhm",0,1e12),TF("Sensor voltage","V",0,1000000),TF("Sensor current","mA",-1e9,1e9),TI("Indoor air quality","index",0,500),TF("Sensor distance","mm",0,1e12),TF("Light","lux",0,1e12),TF("White light","lux",0,1e12),TF("Infrared light","lux",0,1e12),TF("Ultraviolet light","lux",0,1e12),TI("Wind direction","deg",0,360),TF("Wind speed","m/s",0,1000),TF("Weight","kg",-1e9,1e9),TF("Wind gust","m/s",0,1000),TF("Wind lull","m/s",0,1000),TF("Radiation","uR/h",0,1e12),TF("Rain / last hour","mm",0,1e9),TF("Rain / last day","mm",0,1e9),TI("Soil moisture","%",0,100),TT("Soil temperature")};
static const mesh_tlm_field_t tlm_air[]={TI("PM1.0 standard","ug/m3",0,UINT32_MAX),TI("PM2.5 standard","ug/m3",0,UINT32_MAX),TI("PM10 standard","ug/m3",0,UINT32_MAX),TI("PM1.0 environmental","ug/m3",0,UINT32_MAX),TI("PM2.5 environmental","ug/m3",0,UINT32_MAX),TI("PM10 environmental","ug/m3",0,UINT32_MAX),TI("Particles >0.3um","count",0,UINT32_MAX),TI("Particles >0.5um","count",0,UINT32_MAX),TI("Particles >1.0um","count",0,UINT32_MAX),TI("Particles >2.5um","count",0,UINT32_MAX),TI("Particles >5.0um","count",0,UINT32_MAX),TI("Particles >10um","count",0,UINT32_MAX),TI("Carbon dioxide","ppm",0,UINT32_MAX)};
static const mesh_tlm_field_t tlm_power[]={TF("Channel 1 voltage","V",0,1000000),TF("Channel 1 current","mA",-1e9,1e9),TF("Channel 2 voltage","V",0,1000000),TF("Channel 2 current","mA",-1e9,1e9),TF("Channel 3 voltage","V",0,1000000),TF("Channel 3 current","mA",-1e9,1e9)};
static const mesh_tlm_field_t tlm_local[]={TI("Uptime","s",0,UINT32_MAX),TF("Channel use","%",0,100),TF("Transmit airtime","%",0,100)};
#undef TF
#undef TI
#undef TT
const mesh_tlm_field_t *mesh_tlm_field(uint8_t kind,unsigned field){
 const mesh_tlm_field_t *fields=NULL;unsigned count=0;
 if(kind==2){fields=tlm_device;count=5;}else if(kind==3){fields=tlm_environment;count=22;}else if(kind==4){fields=tlm_air;count=13;}else if(kind==5){fields=tlm_power;count=6;}else if(kind==6){fields=tlm_local;count=3;}
 return field&&field<=count?&fields[field-1]:NULL;
}
const char *mesh_tlm_family(uint8_t kind){return kind==2?"Device":kind==3?"Environment":kind==4?"Air quality":kind==5?"External power":kind==6?"Local radio":"Unknown";}
static EXT_RAM_BSS_ATTR mesh_tlm_sample_t tlm_samples[MESH_TLM_LIMIT];
static EXT_RAM_BSS_ATTR uint64_t tlm_order[MESH_TLM_LIMIT];
static size_t tlm_count;
static uint64_t tlm_revision,tlm_next=UINT64_C(1)<<32;
void mesh_tlm_reset(void){memset(tlm_order,0,sizeof(tlm_order));tlm_count=0;tlm_revision=0;tlm_next=UINT64_C(1)<<32;}
void mesh_tlm_forget(uint32_t node){bool changed=false;for(size_t i=0;i<MESH_TLM_LIMIT;i++)if(tlm_order[i]&&tlm_samples[i].node==node){tlm_order[i]=0;tlm_count--;changed=true;}if(changed)tlm_revision++;}
size_t mesh_tlm_count(void){return tlm_count;}
uint64_t mesh_tlm_revision(void){return tlm_revision;}
bool mesh_tlm_valid(const mesh_tlm_sample_t *s){
 if(!s||!s->node||s->node==UINT32_MAX||!s->present||(s->flags&~7)||s->kind<2||s->kind>6||s->present>>MESH_TLM_FIELDS)return false;
 for(unsigned i=0;i<MESH_TLM_FIELDS;i++)if(s->present&(1u<<i)){
  const mesh_tlm_field_t *f=mesh_tlm_field(s->kind,i+1);double v=s->value[i];
  if(!f||!isfinite(v)||v<f->min||v>f->max||(f->integer&&v!=floor(v)))return false;
 }
 return true;
}
static bool tlm_equal(const mesh_tlm_sample_t *a,const mesh_tlm_sample_t *b){
 if(a->present!=b->present)return false;
 for(unsigned i=0;i<MESH_TLM_FIELDS;i++)if((a->present&(1u<<i))&&a->value[i]!=b->value[i])return false;
 return true;
}
static uint32_t tlm_time(const mesh_tlm_sample_t *s){return s->flags&(MESH_TLM_CACHED|MESH_TLM_CLOCK_SKEW)?0:s->time;}
static bool tlm_put(const mesh_tlm_sample_t *sample,uint64_t rank,bool imported){
 if(!mesh_tlm_valid(sample)||!rank)return false;
 size_t free_slot=MESH_TLM_LIMIT,oldest=MESH_TLM_LIMIT,old_family=MESH_TLM_LIMIT,latest=MESH_TLM_LIMIT,n=0;
 for(size_t i=0;i<MESH_TLM_LIMIT;i++){
  if(!tlm_order[i]){if(free_slot==MESH_TLM_LIMIT)free_slot=i;continue;}
  if(oldest==MESH_TLM_LIMIT||tlm_order[i]<tlm_order[oldest])oldest=i;
  mesh_tlm_sample_t *p=&tlm_samples[i];if(p->node!=sample->node||p->kind!=sample->kind)continue;
  n++;if(old_family==MESH_TLM_LIMIT||tlm_order[i]<tlm_order[old_family])old_family=i;
  if(latest==MESH_TLM_LIMIT||tlm_order[i]>tlm_order[latest])latest=i;
  if(sample->packet&&p->packet==sample->packet)return false;
  if(tlm_equal(p,sample)&&((sample->time&&sample->time==p->time)||((sample->flags&MESH_TLM_CACHED)&&(p->flags&MESH_TLM_CACHED))))return false;
 }
 if(!imported&&latest<MESH_TLM_LIMIT){mesh_tlm_sample_t *p=&tlm_samples[latest];
  if((sample->flags&MESH_TLM_CACHED)&&!(p->flags&MESH_TLM_CACHED))return false;
  if(tlm_time(sample)&&tlm_time(p)&&tlm_time(sample)<tlm_time(p))return false;
 }
 size_t slot=n>=MESH_TLM_SERIES_LIMIT?old_family:free_slot<MESH_TLM_LIMIT?free_slot:oldest;
 if(slot==MESH_TLM_LIMIT||(tlm_order[slot]&&rank<=tlm_order[slot]))return false;
 if(!tlm_order[slot])tlm_count++;
 tlm_samples[slot]=*sample;tlm_order[slot]=rank;tlm_revision++;return true;
}
bool mesh_tlm_add(const mesh_tlm_sample_t *sample){return tlm_put(sample,++tlm_next,false);}
bool mesh_tlm_import(const mesh_tlm_sample_t *sample,uint64_t order){if(!sample||order>=(UINT64_C(1)<<32))return false;mesh_tlm_sample_t s=*sample;s.flags|=MESH_TLM_RESTORED;s.received_us=0;return tlm_put(&s,order,true);}
bool mesh_tlm_latest(uint32_t node,uint8_t kind,unsigned field,mesh_tlm_sample_t *out){
 if(!out||field>MESH_TLM_FIELDS)return false;
 size_t best=MESH_TLM_LIMIT;
 for(size_t i=0;i<MESH_TLM_LIMIT;i++)if(tlm_order[i]&&tlm_samples[i].node==node&&tlm_samples[i].kind==kind&&(!field||(tlm_samples[i].present&(1u<<(field-1))))&&(best==MESH_TLM_LIMIT||tlm_order[i]>tlm_order[best]))best=i;
 if(best==MESH_TLM_LIMIT)return false;
 *out=tlm_samples[best];return true;
}
uint32_t mesh_tlm_fields(uint32_t node,uint8_t kind){uint32_t fields=0;for(size_t i=0;i<MESH_TLM_LIMIT;i++)if(tlm_order[i]&&tlm_samples[i].node==node&&tlm_samples[i].kind==kind)fields|=tlm_samples[i].present;return fields;}
size_t mesh_tlm_series(uint32_t node,uint8_t kind,mesh_tlm_sample_t *out,size_t capacity){
 uint64_t ranks[MESH_TLM_SERIES_LIMIT];size_t count=0;
 if(!out)return 0;
 if(capacity>MESH_TLM_SERIES_LIMIT)capacity=MESH_TLM_SERIES_LIMIT;
 for(size_t i=0;i<MESH_TLM_LIMIT;i++)if(tlm_order[i]&&tlm_samples[i].node==node&&tlm_samples[i].kind==kind){
  size_t at=count;if(at==capacity){if(!capacity||tlm_order[i]<=ranks[0])continue;memmove(out,out+1,(capacity-1)*sizeof(*out));memmove(ranks,ranks+1,(capacity-1)*sizeof(*ranks));at=capacity-1;}else count++;
  while(at&&ranks[at-1]>tlm_order[i]){ranks[at]=ranks[at-1];out[at]=out[at-1];at--;}
  ranks[at]=tlm_order[i];out[at]=tlm_samples[i];
 }
 return count;
}
static int tlm_slot_compare(const void *a,const void *b){uint64_t x=tlm_order[*(const uint16_t*)a],y=tlm_order[*(const uint16_t*)b];return x<y?-1:x>y;}
size_t mesh_tlm_snapshot(mesh_tlm_sample_t *out,size_t capacity){
 if(!out||capacity<tlm_count||!tlm_count)return 0;
 uint16_t *slots=malloc(tlm_count*sizeof(*slots));if(!slots)return 0;
 size_t n=0;for(size_t i=0;i<MESH_TLM_LIMIT;i++)if(tlm_order[i])slots[n++]=i;
 qsort(slots,n,sizeof(*slots),tlm_slot_compare);
 for(size_t i=0;i<n;i++)out[i]=tlm_samples[slots[i]];
 free(slots);return n;
}
