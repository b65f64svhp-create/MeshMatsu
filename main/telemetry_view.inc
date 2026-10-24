#include "telemetry_export.h"
static uint32_t telemetry_view_node;
static bool telemetry_view_local,telemetry_graph,telemetry_export_visible;
static uint8_t telemetry_kind=2,telemetry_fields[7]={0,0,1,1,1,1,1};
static EXT_RAM_BSS_ATTR mesh_tlm_sample_t telemetry_plot[MESH_TLM_SERIES_LIMIT];
static void telemetry_view_open(uint32_t node,bool local){telemetry_view_node=node;telemetry_view_local=local;telemetry_kind=local?6:2;telemetry_graph=false;telemetry_export_visible=false;telemetry_feedback[0]=0;for(unsigned i=2;i<=6;i++)telemetry_fields[i]=1;}
static uint32_t telemetry_available(void){uint32_t fields=mesh_tlm_fields(telemetry_view_node,telemetry_kind);if(telemetry_kind==2)fields|=31;if(telemetry_kind==6)fields|=7;return fields;}
static unsigned telemetry_selected(void){
 uint32_t mask=telemetry_available();unsigned selected=telemetry_fields[telemetry_kind];
 if(selected&&selected<=22&&(mask&(1u<<(selected-1))))return selected;
 for(unsigned f=1;f<=22;f++)if(mask&(1u<<(f-1))){telemetry_fields[telemetry_kind]=f;return f;}
 return 1;
}
static bool telemetry_view_navigation(int key){
 if(key==BSP_INPUT_NAVIGATION_KEY_LEFT||key==BSP_INPUT_NAVIGATION_KEY_RIGHT){
  unsigned last=telemetry_view_node==mesh_radio_local_node()?6:5;
  if(key==BSP_INPUT_NAVIGATION_KEY_RIGHT)telemetry_kind=telemetry_kind==last?2:telemetry_kind+1;
  else telemetry_kind=telemetry_kind==2?last:telemetry_kind-1;
  telemetry_export_visible=false;telemetry_feedback[0]=0;telemetry_selected();return true;
 }
 if(key==BSP_INPUT_NAVIGATION_KEY_UP||key==BSP_INPUT_NAVIGATION_KEY_DOWN){
  uint32_t mask=telemetry_available();unsigned field=telemetry_selected();
  for(unsigned i=0;i<22;i++){field=key==BSP_INPUT_NAVIGATION_KEY_DOWN?field%22+1:(field+20)%22+1;if(mask&(1u<<(field-1))){telemetry_fields[telemetry_kind]=field;break;}}
  return true;
 }
 return false;
}
static bool telemetry_view_key(char key){
 if(current_screen!=SCREEN_RADIO_ACTIVITY&&!(current_screen==SCREEN_NODES&&node_modal==NODE_TELEMETRY))return false;
 if(key=='\t'){telemetry_graph=!telemetry_graph;return true;}
 if(key=='r'||key=='R'){mesh_tlm_store_retry();return true;}
 if(key=='e'||key=='E'){
  mesh_tlm_export_start_t result=mesh_tlm_export_start(telemetry_view_node,telemetry_kind);telemetry_export_visible=result==MESH_EXPORT_OK;
  const char *message=result==MESH_EXPORT_OK?"":result==MESH_EXPORT_BUSY?"An export is already running. Wait, then press E again.":result==MESH_EXPORT_EMPTY?"No readings to export for this node/report type.":result==MESH_EXPORT_MEMORY?"Not enough memory to export. Try again later.":"Choose a valid node/report type to export.";
  snprintf(telemetry_feedback,sizeof(telemetry_feedback),"%s",message);return true;
 }
 return false;
}
static double telemetry_display_value(unsigned field,double value){
 const mesh_tlm_field_t *info=mesh_tlm_field(telemetry_kind,field);
 if(info&&info->temperature)return mesh_units_temperature_value(temperature_units,value);
 return value;
}
static void telemetry_value_text(unsigned field,double value,char *out,size_t size){
 const mesh_tlm_field_t *info=mesh_tlm_field(telemetry_kind,field);if(!info){snprintf(out,size,"Unavailable");return;}
 if(info->temperature){mesh_units_temperature(temperature_units,value,out,size);return;}
 if(telemetry_kind==2&&field==1&&value>100){snprintf(out,size,"External power");return;}
 snprintf(out,size,info->integer?"%.0f %s":"%.2f %s",value,info->unit);
}
static void telemetry_age(const mesh_tlm_sample_t *s,char *out,size_t size){
 if(s->flags&MESH_TLM_CACHED){snprintf(out,size,"Radio cache / measurement time unavailable");return;}
 time_t now=time(NULL);
 if((s->flags&MESH_TLM_CLOCK_SKEW)||(s->time&&(uint64_t)s->time>(uint64_t)now+300))snprintf(out,size,"Report clock differs / measurement age unknown");
 else if(s->time>=1700000000&&now>=s->time){uint64_t age=(uint64_t)now-s->time;if(age<3600)snprintf(out,size,"Measured %llum %llus ago",(unsigned long long)(age/60),(unsigned long long)(age%60));else snprintf(out,size,"Measured %lluh %llum ago",(unsigned long long)(age/3600),(unsigned long long)(age%3600/60));}
 else if(!(s->flags&MESH_TLM_RESTORED)&&s->received_us&&esp_timer_get_time()>=s->received_us)snprintf(out,size,"Received %llds ago / measurement time unavailable",(long long)((esp_timer_get_time()-s->received_us)/1000000));
 else snprintf(out,size,"Saved report / measurement age unknown");
}
