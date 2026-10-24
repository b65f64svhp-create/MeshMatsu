static void wp_delivery(const mesh_waypoint_t *w,char *out,size_t capacity) {
    mesh_message_t status={.delivery=w->delivery,.peer_node=w->peer_node,.failure=w->failure,.error_reason=w->error_reason};
    snprintf(out,capacity,"%s",w->delivery==MESH_DELIVERY_NONE?"Received waypoint":mesh_radio_delivery_text(&status));
}
static void draw_waypoint_markers(void) {
    int x=20,y=94,width=logical_h_res-40,height=logical_v_res-252;
    pax_clip(&fb,x,y,width,height);
    for(size_t i=0;!map_filters.waypoints_hidden&&i<mesh_radio_waypoint_count();i++){mesh_waypoint_t w;if(!mesh_radio_get_waypoint(i,&w))continue;
        double px,py;if(!mesh_map_marker(&map_view,w.latitude_i,w.longitude_i,width,height,&px,&py))continue;
        int mx=x+(int)px,my=y+(int)py;
        pax_draw_rect(&fb,BLACK,mx-7,my-8,15,20);
        if(w.icon==0x26fa){ // Camp/tent.
            pax_outline_tri(&fb,KEY_TRIANGLE_COLOR,mx,my-7,mx-7,my+6,mx+7,my+6);
            pax_draw_line(&fb,KEY_TRIANGLE_COLOR,mx,my-3,mx,my+6);
        }else if(w.icon==0x26a0){
            pax_outline_tri(&fb,KEY_TRIANGLE_COLOR,mx,my-7,mx-7,my+6,mx+7,my+6);
            pax_draw_line(&fb,KEY_TRIANGLE_COLOR,mx,my-2,mx,my+1);pax_draw_rect(&fb,KEY_TRIANGLE_COLOR,mx,my+3,1,1);
        }else if(w.icon==0x1f4e1){
            pax_outline_circle(&fb,KEY_TRIANGLE_COLOR,mx,my-2,5);pax_draw_line(&fb,KEY_TRIANGLE_COLOR,mx,my-2,mx,my+7);
            pax_draw_line(&fb,KEY_TRIANGLE_COLOR,mx-4,my+7,mx+4,my+7);
        }else if(w.icon==0x1f3e0){
            pax_draw_line(&fb,KEY_TRIANGLE_COLOR,mx-7,my-1,mx,my-7);pax_draw_line(&fb,KEY_TRIANGLE_COLOR,mx,my-7,mx+7,my-1);
            pax_outline_rect(&fb,KEY_TRIANGLE_COLOR,mx-5,my-1,10,8);
        }else {pax_outline_rect(&fb,KEY_TRIANGLE_COLOR,mx-4,my-5,9,9);pax_draw_line(&fb,KEY_TRIANGLE_COLOR,mx,my+4,mx,my+11);}
        bool selected=w.id==map_waypoint_id&&w.peer_node==map_waypoint_peer&&(w.peer_node||w.channel==map_waypoint_channel);
        if(selected)pax_outline_circle(&fb,WHITE,mx,my,12);
        if(map_view.zoom>=9||selected){int left=mx+11;int text_width=(int)pax_text_size(pax_font_sky_mono,10,w.name).x;
            if(left+text_width>x+width)left=x+width-text_width-4;
            if(left<x)left=x;
            draw_rich_text(w.name,left,my-22,10,x+width-left,20,1,KEY_TRIANGLE_COLOR);}
    }
    int cx=x+width/2,cy=y+height/2;
    pax_draw_line(&fb,UI_MUTED,cx-7,cy,cx+7,cy);pax_draw_line(&fb,UI_MUTED,cx,cy-7,cx,cy+7);
    pax_noclip(&fb);
}
static void draw_waypoints(void) {
    char text[200];
    if(wp_mode==WP_LIST){
        size_t count=mesh_radio_waypoint_count();if(wp_selection>count)wp_selection=count;
        snprintf(text,sizeof(text),"%u saved waypoints / up to %u. Expired markers are hidden.",(unsigned)count,MESH_WAYPOINT_LIMIT);
        pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,11,20,67,text);
        size_t first=wp_selection>4?wp_selection-4:0;
        for(size_t i=first;i<=count&&i<first+5;i++){int y=91+(i-first)*58;mesh_waypoint_t w={0};
            if(i&&!mesh_radio_get_waypoint(i-1,&w))continue;
            pax_draw_rect(&fb,wp_selection==i?UI_SELECTED:UI_PANEL,20,y,logical_h_res-40,51);pax_draw_rect(&fb,wp_selection==i?UI_GREEN:UI_BORDER,20,y,3,51);
            draw_rich_text(i?w.name:"CREATE AT MAP CENTRE",34,y+7,14,logical_h_res-68,22,1,UI_GREEN);
            if(!i)snprintf(text,sizeof(text),"Pan/zoom first. The map centre is a chosen point, not your GPS fix.");
            else snprintf(text,sizeof(text),"%.5f, %.5f / %s / %s",w.latitude_i/1e7,w.longitude_i/1e7,w.peer_node?"private DM":"channel",w.locked_to?"locked":"open");
            draw_rich_text(text,34,y+29,10,logical_h_res-68,18,1,UI_MUTED);
        }
    }else if(wp_mode==WP_VIEW){
        mesh_waypoint_t live;if(mesh_radio_find_waypoint(wp_selected.id,wp_selected.peer_node,wp_selected.channel,&live))wp_selected=live;
        draw_rich_text(wp_selected.name,20,88,20,logical_h_res-40,28,2,UI_GREEN);
        draw_rich_text(wp_selected.description[0]?wp_selected.description:"No description",20,146,13,logical_h_res-40,23,3,WHITE);
        snprintf(text,sizeof(text),"%.7f, %.7f / %s %u",wp_selected.latitude_i/1e7,wp_selected.longitude_i/1e7,wp_selected.peer_node?"DM / channel":"Channel",wp_selected.channel);
        pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,11,20,225,text);
        char lock[40];if(wp_selected.locked_to)snprintf(lock,sizeof(lock),"Locked to !%08lx",(unsigned long)wp_selected.locked_to);else snprintf(lock,sizeof(lock),"Open to edits by channel members");
        snprintf(text,sizeof(text),"%s / First sender !%08lx",lock,(unsigned long)wp_selected.creator);
        draw_rich_text(text,20,247,11,logical_h_res-40,18,1,UI_MUTED);
        if(!wp_selected.expire||wp_selected.expire==INT32_MAX)snprintf(text,sizeof(text),"Expiry: never");
        else {char stamp[32];mesh_time_stamp(&time_settings,wp_selected.expire,stamp,sizeof(stamp));snprintf(text,sizeof(text),"Expires: %s",stamp);}
        pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,11,20,269,text);
        wp_delivery(&wp_selected,text,sizeof(text));draw_rich_text(text,20,291,11,logical_h_res-40,18,1,wp_selected.delivery==MESH_DELIVERY_FAILED?RED:UI_GREEN);
        const char *names[]={"SHOW ON MAP","EDIT WAYPOINT","SHARE REMOVAL"};
        for(unsigned i=0;i<3;i++){int y=320+i*26;pax_draw_rect(&fb,wp_action==i?UI_SELECTED:UI_PANEL,20,y,logical_h_res-40,23);pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,11,34,y+5,names[i]);}
    }else if(wp_mode==WP_EDIT){
        const char *labels[]={"NAME","DESCRIPTION (optional)","LOCATION (Space: use map centre)","SHARE ON","EXPIRY","OWNER LOCK","ICON","REVIEW & SEND"};
        const char *expiry[]={wp_existing?"Keep existing expiry":"Never","1 hour from send","24 hours from send","7 days from send","Never"};
        size_t first=wp_field>4?wp_field-4:0;
        for(size_t i=first;i<8&&i<first+5;i++){int y=91+(i-first)*58;pax_draw_rect(&fb,wp_field==i?UI_SELECTED:UI_PANEL,20,y,logical_h_res-40,51);
            pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,34,y+5,labels[i]);
            if(!i)snprintf(text,sizeof(text),"%s",wp_draft.name[0]?wp_draft.name:"Type a name");else if(i==1)snprintf(text,sizeof(text),"%s",wp_draft.description[0]?wp_draft.description:"Optional");
            else if(i==2)snprintf(text,sizeof(text),"%.7f, %.7f",wp_draft.latitude_i/1e7,wp_draft.longitude_i/1e7);
            else if(i==3){mesh_channel_t c={0};mesh_radio_get_channel(wp_draft.channel,&c);snprintf(text,sizeof(text),"%s %u: %s%s",wp_draft.peer_node?"Private DM / slot":"Channel",wp_draft.channel,c.name,wp_existing?" (original destination)":"");}
            else if(i==4)snprintf(text,sizeof(text),"%s",expiry[wp_expiry]);else if(i==5)snprintf(text,sizeof(text),"%s",wp_draft.locked_to?"Only your radio can edit/delete":"Unlocked: mesh members can edit");
            else if(i==6)snprintf(text,sizeof(text),"%s",wp_draft.icon==0x1f4cd?"Map pin":wp_draft.icon==0x26fa?"Camp":wp_draft.icon==0x26a0?"Warning":wp_draft.icon==0x1f4e1?"Radio":wp_draft.icon==0x1f3e0?"Home":"Other icon (preserved)");else snprintf(text,sizeof(text),"Enter opens confirmation; no automatic send");
            draw_rich_text(text,34,y+25,13,logical_h_res-68,22,1,UI_GREEN);
        }
    }else if(wp_mode==WP_CONFIRM||wp_mode==WP_DELETE){
        draw_rich_text(wp_draft.name,20,94,19,logical_h_res-40,28,2,UI_GREEN);
        draw_rich_text(wp_mode==WP_DELETE?"Broadcast an expired copy to remove this waypoint from other clients? Removal uses the original destination and respects the owner lock.":"Share this chosen map location and waypoint details? Everyone on the selected channel can receive it. This does not change your node's own shared location.",20,170,14,logical_h_res-40,26,6,WHITE);
        for(unsigned i=0;i<2;i++){int width=((int)logical_h_res-52)/2,x=20+i*(width+12);bool selected=wp_confirm==(i!=0);pax_draw_rect(&fb,selected?UI_GREEN:UI_PANEL,x,350,width,36);pax_draw_text(&fb,selected?BLACK:UI_GREEN,pax_font_sky_mono,13,x+12,361,i?wp_mode==WP_DELETE?"SHARE REMOVAL":"SEND WAYPOINT":"CANCEL");}
    }
    if(wp_mode==WP_LIST&&wp_have_send&&wp_last_send.deleted&&!wp_feedback[0]){
        mesh_waypoint_t latest;if(mesh_radio_find_waypoint(wp_last_send.id,wp_last_send.peer_node,wp_last_send.channel,&latest)){
            char status[80];wp_delivery(&latest,status,sizeof(status));snprintf(text,sizeof(text),"Latest removal: %s",status);
            draw_rich_text(text,20,397,10,logical_h_res-40,18,1,latest.delivery==MESH_DELIVERY_FAILED?RED:UI_GREEN);}}
    if(wp_feedback[0])draw_rich_text(wp_feedback,20,397,10,logical_h_res-40,18,1,KEY_TRIANGLE_COLOR);
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,logical_v_res-UI_HINT_OFFSET,
        wp_mode==WP_EDIT?"Up/Down/Tab: field  Type: replace text  L/R: value  Enter: next/review":wp_mode==WP_CONFIRM||wp_mode==WP_DELETE?"Left/Right: Cancel or Send  Enter: choose  Esc: back":"Up/Down: select  Enter: open  Esc: back to map");
}
