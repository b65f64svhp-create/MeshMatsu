static void draw_behavior_settings(void){
    behavior_refresh();
    char interval[64];
    if(!behavior_loaded)snprintf(interval,sizeof(interval),"Reading radio...");
    else if(behavior_interval_draft&&behavior_interval_draft%3600==0)snprintf(interval,sizeof(interval),"Every %lu hour%s",(unsigned long)(behavior_interval_draft/3600),behavior_interval_draft==3600?"":"s");
    else snprintf(interval,sizeof(interval),"Current: %lu seconds",(unsigned long)behavior_interval_draft);
    const char *labels[]={"NODE NAME ANNOUNCEMENT","REBROADCAST MODE","SAVE TO RADIO"};
    for(size_t i=0;i<3;i++){
        int y=i<2?88+i*64:342;
        pax_draw_rect(&fb,behavior_field==i?UI_SELECTED:UI_PANEL,20,y,logical_h_res-40,i<2?56:40);
        pax_draw_rect(&fb,behavior_field==i?UI_GREEN:UI_BORDER,20,y,3,i<2?56:40);
        pax_draw_text(&fb,i<2?UI_MUTED:UI_GREEN,pax_font_sky_mono,i<2?11:14,34,y+8,labels[i]);
        if(i<2)pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,15,34,y+29,i==0?interval:behavior_loaded?behavior_mode_name(behavior_mode_draft):"Reading radio...");
    }
    draw_rich_text("Announces your radio's name periodically. Minimum 1 hour; requests can still receive a name reply.",20,222,12,logical_h_res-40,19,2,UI_MUTED);
    draw_rich_text(behavior_loaded?behavior_explanation(behavior_mode_draft):"Waiting for current device settings.",20,270,13,logical_h_res-40,21,3,WHITE);
    draw_rich_text(behavior_feedback,20,390,11,logical_h_res-40,17,2,behavior_pending?KEY_TRIANGLE_COLOR:UI_GREEN);
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,logical_v_res-UI_HINT_OFFSET,"Up/Down: field  L/R: value  Enter on Save: apply");
}
