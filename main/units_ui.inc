static uint8_t units_draft,temperature_draft;
static unsigned units_field;
static char units_feedback[96];
static void open_units_settings(void){units_draft=distance_units;temperature_draft=temperature_units;units_field=0;units_feedback[0]=0;current_screen=SCREEN_UNITS;}
static bool units_navigation(int key){
 if(current_screen!=SCREEN_UNITS)return false;
 if(key==BSP_INPUT_NAVIGATION_KEY_UP){if(units_field)units_field--;}
 else if(key==BSP_INPUT_NAVIGATION_KEY_DOWN){if(units_field<2)units_field++;}
 else if(key==BSP_INPUT_NAVIGATION_KEY_LEFT||key==BSP_INPUT_NAVIGATION_KEY_RIGHT){if(!units_field)units_draft^=1;else if(units_field==1)temperature_draft^=1;}
 else if(key==BSP_INPUT_NAVIGATION_KEY_RETURN){
  if(!units_field)units_draft^=1;
  else if(units_field==1)temperature_draft^=1;
  else if(mesh_units_save_all(units_draft,temperature_draft)==ESP_OK){distance_units=units_draft;temperature_units=temperature_draft;snprintf(units_feedback,sizeof(units_feedback),"Units saved.");}
  else snprintf(units_feedback,sizeof(units_feedback),"Could not save. Current units unchanged; retry Save.");
 }else return false;
 return true;
}
static void draw_units_settings(void){
 const char *labels[]={"DISTANCE UNITS","TEMPERATURE UNITS","SAVE SETTINGS"};
 for(unsigned i=0;i<3;i++){int y=92+i*64;pax_draw_rect(&fb,units_field==i?UI_SELECTED:UI_PANEL,20,y,logical_h_res-40,56);pax_draw_rect(&fb,units_field==i?UI_GREEN:UI_BORDER,20,y,3,56);pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,13,34,y+8,labels[i]);if(i<2)pax_draw_text(&fb,WHITE,pax_font_sky_mono,13,34,y+31,i?mesh_units_temperature_name(temperature_draft):mesh_units_name(units_draft));}
 char distance[32],temperature[32],example[120];mesh_units_distance(units_draft,5000,distance,sizeof(distance));mesh_units_temperature(temperature_draft,20,temperature,sizeof(temperature));snprintf(example,sizeof(example),"Examples: %s / %s",distance,temperature);pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,13,20,306,example);
 draw_rich_text("Used for distances, location-accuracy labels and received sensor temperatures. Radio settings and original measurements stay unchanged.",20,338,12,logical_h_res-40,22,2,UI_MUTED);
 draw_rich_text(units_feedback,20,390,11,logical_h_res-40,18,1,UI_GREEN);
 pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,logical_v_res-UI_HINT_OFFSET,"Up/Down: field  L/R: units  Enter: change/save  Esc: back");
}
