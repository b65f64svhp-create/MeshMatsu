static void draw_position_settings(void) {
    bool fixed=current_screen==SCREEN_FIXED_POSITION;
    mesh_position_config_t current;
    if(mesh_radio_get_position_config(&current)) {
        if(!position_loaded || (!position_edited && !position_pending)) {position_draft=current;position_loaded=true;}
        if(position_pending && mesh_radio_location_save().state==MESH_LOCATION_SAVED) {
            position_draft=current;position_edited=false;
        }
        if(fixed && (!fixed_revision || (fixed_pending && mesh_radio_location_save().state==MESH_LOCATION_SAVED))) fixed_revision=current.revision;
    }
    mesh_node_t own;char summary[96],age[48];
    if(mesh_radio_get_node_by_number(mesh_radio_local_node(),&own)&&own.position_available) {
        format_node_last_seen(own.position_time,time(NULL),age,sizeof(age));
        snprintf(summary,sizeof(summary),"Current: %.7f, %.7f",own.latitude_i/1e7,own.longitude_i/1e7);
    } else {snprintf(summary,sizeof(summary),"Current location unavailable");snprintf(age,sizeof(age),"No position reported by the radio");}
    pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,12,20,88,summary);
    snprintf(summary,sizeof(summary),"Position age: %s",age);
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,110,summary);
    if(fixed) {
        const char *labels[]={"LATITUDE (decimal degrees)","LONGITUDE (decimal degrees)","SAVE FIXED LOCATION"};
        const char *values[]={fixed_lat[0]?fixed_lat:"Enter latitude",fixed_lon[0]?fixed_lon:"Enter longitude","Enables fixed position on the radio"};
        for(size_t i=0;i<3;i++) {
            int y=145+i*62;
            pax_draw_rect(&fb,fixed_field==i?UI_SELECTED:UI_PANEL,20,y,logical_h_res-40,54);
            pax_draw_rect(&fb,fixed_field==i?UI_GREEN:UI_BORDER,20,y,3,54);
            pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,34,y+7,labels[i]);
            pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,14,34,y+27,values[i]);
        }
        pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,344,"Saving may broadcast this location at your channel precision.");
        draw_rich_text(fixed_pending?location_operation_feedback():fixed_feedback,20,369,11,logical_h_res-40,16,2,UI_GREEN);
    } else {
        char interval[48];
        if(!position_loaded)snprintf(interval,sizeof(interval),"Loading...");
        else if(!position_draft.broadcast_secs)snprintf(interval,sizeof(interval),"Radio default");
        else if(position_draft.broadcast_secs<3600)snprintf(interval,sizeof(interval),"%lu minutes",(unsigned long)position_draft.broadcast_secs/60);
        else snprintf(interval,sizeof(interval),"%lu seconds",(unsigned long)position_draft.broadcast_secs);
        const char *labels[]={"Broadcast interval","Smart broadcast","Position source","Set fixed coordinates","Channel sharing precision","SAVE POSITION SETTINGS"};
        const char *values[]={interval,position_draft.smart?"On":"Off",position_draft.fixed?"Fixed location":"Radio / GPS / phone","Open >","Choose a channel >",""};
        for(size_t i=0;i<6;i++) {
            int y=140+i*38;
            pax_draw_rect(&fb,position_field==i?UI_SELECTED:UI_PANEL,20,y,logical_h_res-40,32);
            pax_draw_rect(&fb,position_field==i?UI_GREEN:UI_BORDER,20,y,3,32);
            pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,12,34,y+10,labels[i]);
            pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,12,logical_h_res/2+30,y+10,values[i]);
        }
        const char *feedback=position_pending?location_operation_feedback():position_feedback;
        draw_rich_text(feedback[0]?feedback:"Smart broadcast adds movement-based updates. GPS hardware is unchanged.",20,377,10,logical_h_res-40,15,2,UI_MUTED);
    }
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,logical_v_res-UI_HINT_OFFSET,
        fixed?"Tab/Up/Down: field  Type: edit  Enter: save selected  Esc: back":"Up/Down: select  L/R: change  Enter: open/save  Esc: back");
}
