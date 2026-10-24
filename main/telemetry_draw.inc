static void draw_telemetry_chart(uint32_t number,unsigned field){
 size_t n=mesh_tlm_series(number,telemetry_kind,telemetry_plot,MESH_TLM_SERIES_LIMIT),valid=0;double low=0,high=0;
 const mesh_tlm_field_t *info=mesh_tlm_field(telemetry_kind,field);
 char name[100];snprintf(name,sizeof(name),"%s / HISTORY",info?info->name:"Reading");pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,13,20,164,name);
 bool timed=n>1;
 for(size_t i=0;i<n;i++){
  mesh_tlm_sample_t *s=&telemetry_plot[i];if(!s->time||(s->flags&(MESH_TLM_CACHED|MESH_TLM_CLOCK_SKEW))||(i&&s->time<telemetry_plot[i-1].time))timed=false;
  if(!(s->present&(1u<<(field-1)))||(telemetry_kind==2&&field==1&&s->value[0]>100))continue;
  double v=telemetry_display_value(field,s->value[field-1]);if(!valid){low=high=v;}else{if(v<low)low=v;if(v>high)high=v;}valid++;
 }
 const int x=118,y=202,w=(int)logical_h_res-148,h=110;pax_draw_rect(&fb,UI_PANEL,x,y,w,h);
 if(!valid){draw_rich_text("No saved readings for this metric yet. Graphs fill as reports arrive.",x+12,y+25,13,w-24,24,3,UI_MUTED);return;}
 double original_low=low,original_high=high;if(high==low){double margin=fmax(0.1,fabs(high)*0.05);low-=margin;high+=margin;}
 char min[48],max[48];double raw_low=original_low,raw_high=original_high;
 if(info&&info->temperature&&temperature_units==MESH_TEMP_FAHRENHEIT){raw_low=(raw_low-32)/1.8;raw_high=(raw_high-32)/1.8;}
 telemetry_value_text(field,raw_low,min,sizeof(min));telemetry_value_text(field,raw_high,max,sizeof(max));
 draw_rich_text(max,20,y,9,92,14,2,UI_MUTED);draw_rich_text(min,20,y+h-20,9,92,14,2,UI_MUTED);
 for(unsigned i=0;i<3;i++)pax_draw_line(&fb,UI_BORDER,x,y+i*h/2,x+w,y+i*h/2);
 if(timed&&telemetry_plot[0].time==telemetry_plot[n-1].time)timed=false;
 bool previous=false;float last_x=0,last_y=0;
 for(size_t i=0;i<n;i++){
  mesh_tlm_sample_t *s=&telemetry_plot[i];
  if(!(s->present&(1u<<(field-1)))||(telemetry_kind==2&&field==1&&s->value[0]>100)){previous=false;continue;}
  double fraction=timed?(double)(s->time-telemetry_plot[0].time)/(telemetry_plot[n-1].time-telemetry_plot[0].time):n>1?(double)i/(n-1):0.5;
  float px=x+3+fraction*(w-6),py=y+h-3-(telemetry_display_value(field,s->value[field-1])-low)/(high-low)*(h-6);
  if(previous)pax_draw_line(&fb,UI_GREEN,last_x,last_y,px,py);
  pax_draw_circle(&fb,UI_GREEN,px,py,2);previous=true;last_x=px;last_y=py;
 }
 char first[40],last[40],summary[140];
 if(timed){mesh_time_stamp(&time_settings,telemetry_plot[0].time,first,sizeof(first));mesh_time_stamp(&time_settings,telemetry_plot[n-1].time,last,sizeof(last));}
 else{snprintf(first,sizeof(first),"Earlier reports");snprintf(last,sizeof(last),"Latest / report order");}
 pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,9,x,318,first);float width=pax_text_size(pax_font_sky_mono,9,last).x;pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,9,x+w-width,318,last);
 snprintf(summary,sizeof(summary),"%u readings / %u reports  |  %s",(unsigned)valid,(unsigned)n,mesh_tlm_store_status());pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,340,summary);
}
static void draw_telemetry_page(uint32_t number,bool local){
 if(telemetry_view_node!=number||telemetry_view_local!=local)telemetry_view_open(number,local);
 char name[40],id[24],age[100];peer_name(number,name,sizeof(name));pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,14,20,80,name);
 snprintf(id,sizeof(id),"!%08lx",(unsigned long)number);float id_width=pax_text_size(pax_font_sky_mono,11,id).x;pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,11,logical_h_res-20-id_width,82,id);
 unsigned families=number==mesh_radio_local_node()?5:4;const char *tabs[]={"DEVICE","ENVIRONMENT","AIR QUALITY","POWER","LOCAL RADIO"};
 int tab_width=((int)logical_h_res-40)/families;
 for(unsigned i=0;i<families;i++){bool selected=telemetry_kind==i+2;pax_draw_rect(&fb,selected?UI_SELECTED:UI_PANEL,20+i*tab_width,108,tab_width-4,24);pax_draw_text(&fb,selected?UI_GREEN:UI_MUTED,pax_font_sky_mono,11,28+i*tab_width,115,tabs[i]);}
 unsigned field=telemetry_selected();mesh_tlm_sample_t selected;
 if(mesh_tlm_latest(number,telemetry_kind,field,&selected))telemetry_age(&selected,age,sizeof(age));else snprintf(age,sizeof(age),"No %s readings received yet",mesh_tlm_family(telemetry_kind));
 pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,140,age);
 if(telemetry_graph)draw_telemetry_chart(number,field);
 else{
  uint32_t fields=telemetry_available();unsigned rank=0,count=0;for(unsigned f=1;f<=22;f++)if(fields&(1u<<(f-1))){if(f<field)rank++;count++;}
  unsigned first=rank>=5?rank-4:0,row=0;
  for(unsigned f=1;f<=22;f++)if(fields&(1u<<(f-1))){unsigned index=row++;if(index<first||index>=first+5)continue;
   const mesh_tlm_field_t *info=mesh_tlm_field(telemetry_kind,f);if(!info)continue;int y=158+(index-first)*36;
   pax_draw_rect(&fb,f==field?UI_SELECTED:UI_PANEL,20,y,logical_h_res-40,32);if(f==field)pax_draw_rect(&fb,UI_GREEN,20,y,3,32);
   pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,11,34,y+10,info->name);char value[64];mesh_tlm_sample_t reading;
   if(mesh_tlm_latest(number,telemetry_kind,f,&reading))telemetry_value_text(f,reading.value[f-1],value,sizeof(value));else snprintf(value,sizeof(value),"Unavailable");
   float width=pax_text_size(pax_font_sky_mono,13,value).x;pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,13,logical_h_res-32-width,y+8,value);
  }
  if(!count)draw_rich_text("No sensor readings received. The other node needs a supported sensor with its telemetry module enabled.",20,174,15,logical_h_res-40,27,5,UI_MUTED);
  else{char status[128];snprintf(status,sizeof(status),"%u metrics / selected reading age above  |  %s",count,mesh_tlm_store_status());pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,344,status);}
 }
 bool remote=number&&number!=mesh_radio_local_node();mesh_telemetry_request_t request;
 bool requested=mesh_radio_get_telemetry_request(number,&request)&&(request.kind?request.kind:2)==telemetry_kind;
 unsigned cooldown=mesh_radio_telemetry_cooldown_seconds();char button[80],feedback[128];
 if(!remote)snprintf(button,sizeof(button),"YOUR RADIO / REPORTS UPDATE AUTOMATICALLY");
 else if(requested&&request.state==MESH_TELEMETRY_WAITING)snprintf(button,sizeof(button),"WAITING FOR %s...",mesh_tlm_family(telemetry_kind));
 else if(cooldown)snprintf(button,sizeof(button),"REQUEST %s / wait %us",mesh_tlm_family(telemetry_kind),cooldown);
 else snprintf(button,sizeof(button),"REQUEST %s  >",mesh_tlm_family(telemetry_kind));
 pax_draw_rect(&fb,remote?UI_SELECTED:UI_PANEL,20,361,logical_h_res-40,29);pax_draw_text(&fb,remote?UI_GREEN:UI_MUTED,pax_font_sky_mono,12,34,370,button);
 mesh_tlm_export_status_t exported=mesh_tlm_export_status();
 if(telemetry_export_visible&&exported.node==number&&exported.kind==telemetry_kind)snprintf(feedback,sizeof(feedback),"%s",exported.message);
 else if(telemetry_feedback[0])snprintf(feedback,sizeof(feedback),"%s",telemetry_feedback);
 else if(!requested)snprintf(feedback,sizeof(feedback),remote?"Enter requests this report type once; the node must support replies.":"These readings belong to your radio; Tanmatsu battery is in the header.");
 else if(request.state==MESH_TELEMETRY_WAITING)snprintf(feedback,sizeof(feedback),"Waiting for node reply / %u of 120 seconds",request.elapsed_seconds);
 else if(request.state==MESH_TELEMETRY_REPLY)snprintf(feedback,sizeof(feedback),"Requested telemetry received from this node.");
 else if(request.state==MESH_TELEMETRY_TIMEOUT)snprintf(feedback,sizeof(feedback),"No reply; node may be offline, busy, without a sensor or not replying.");
 else if(request.state==MESH_TELEMETRY_INTERRUPTED)snprintf(feedback,sizeof(feedback),"Reconnect interrupted the request. No automatic retry.");
 else snprintf(feedback,sizeof(feedback),"Request failed: %s",trace_error_text(request.error_reason));
 pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,400,feedback);
 pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,logical_v_res-UI_HINT_OFFSET,"L/R: report type  Up/Down: metric  Tab: graph  Enter: request  E: export type  Esc: back");
}
