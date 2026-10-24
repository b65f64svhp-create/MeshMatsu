// Home is a passive dashboard: rendering never marks messages read or polls nodes.
static void draw_home_dashboard(void) {
    mesh_radio_status_t radio=mesh_radio_status();
    size_t channel_unread=mesh_radio_unread_total(false),direct_unread=mesh_radio_unread_total(true);
    size_t enabled=0,active=0,favorites=0,located=0;
    while(enabled_channel_at(enabled,NULL))enabled++;
    time_t now=time(NULL);
    for(size_t i=0;i<mesh_radio_node_count();i++) {
        mesh_node_t node;if(!mesh_radio_get_node(i,&node))continue;
        if(node.last_heard && now>=(time_t)node.last_heard && now-node.last_heard<=7200)active++;
        if(node.favorite_known && node.is_favorite)favorites++;
        if(node.position_available)located++;
    }
    char owner[40]={0},short_name[5]={0},identity[64],summary[80],unread[32];
    bool known=mesh_radio_get_owner(owner,sizeof(owner),short_name,sizeof(short_name));
    pax_draw_rect(&fb,UI_PANEL,20,96,logical_h_res-40,86);
    pax_draw_rect(&fb,radio.synchronized&&!radio.reconnecting?UI_GREEN:KEY_TRIANGLE_COLOR,20,96,3,86);
    draw_rich_text(known&&owner[0]?owner:"Your mesh dashboard",34,104,18,logical_h_res-340,22,1,UI_GREEN);
    if(mesh_radio_local_node())snprintf(identity,sizeof(identity),"YOUR NODE  !%08lx",(unsigned long)mesh_radio_local_node());
    else snprintf(identity,sizeof(identity),"Connecting to your radio...");
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,34,134,identity);
    if(radio.reconnecting)snprintf(summary,sizeof(summary),"Reconnecting automatically / attempt %lu",(unsigned long)radio.reconnect_attempts);
    else if(!radio.frames_received)snprintf(summary,sizeof(summary),"Waiting for the first radio report");
    else snprintf(summary,sizeof(summary),"Last radio activity: %us ago",mesh_radio_rx_age_seconds());
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,34,157,summary);
    int ux=(int)logical_h_res-254;
    if(channel_unread+direct_unread)snprintf(unread,sizeof(unread),"%u unread",(unsigned)(channel_unread+direct_unread));
    else snprintf(unread,sizeof(unread),"All caught up");
    pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,channel_unread+direct_unread?26:19,ux,108,unread);
    snprintf(summary,sizeof(summary),"%u channel / %u direct",(unsigned)channel_unread,(unsigned)direct_unread);
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,11,ux,146,summary);
    int gap=12,card_w=((int)logical_h_res-40-2*gap)/3;
    const char *names[]={"CHANNELS","DIRECT MESSAGES","NODES","MAPS","SETTINGS","EXIT TO LAUNCHER"};
    char values[6][40],details[6][64];
    snprintf(values[0],sizeof(values[0]),"%u unread",(unsigned)channel_unread);
    snprintf(details[0],sizeof(details[0]),"%u enabled channels",(unsigned)enabled);
    snprintf(values[1],sizeof(values[1]),"%u unread",(unsigned)direct_unread);
    snprintf(details[1],sizeof(details[1]),"%u conversations",(unsigned)mesh_radio_direct_peer_count());
    snprintf(values[2],sizeof(values[2]),"%u known",(unsigned)mesh_radio_node_count());
    snprintf(details[2],sizeof(details[2]),"%u active / %u favorites",(unsigned)active,(unsigned)favorites);
    snprintf(values[3],sizeof(values[3]),"%u positioned",(unsigned)located);
    snprintf(details[3],sizeof(details[3]),"Explore offline maps");
    snprintf(values[4],sizeof(values[4]),"Configure");snprintf(details[4],sizeof(details[4]),"Radio, channels & display");
    snprintf(values[5],sizeof(values[5]),"Launcher");snprintf(details[5],sizeof(details[5]),"Save messages and close");
    const int icons[]={1,2,3,4,-1,-1};
    for(size_t i=0;i<6;i++) {
        int x=20+(i%3)*(card_w+gap),y=198+(i/3)*102;bool selected=home_selection==i;
        pax_draw_rect(&fb,selected?UI_SELECTED:UI_PANEL,x,y,card_w,88);
        pax_draw_rect(&fb,selected?UI_GREEN:UI_BORDER,x,y,3,88);
        if(selected)pax_outline_rect(&fb,UI_GREEN,x,y,card_w,88);
        int offset=14;
        if(icons[i]>=0){draw_footer_icon(icons[i],x+18,y+17,selected?UI_GREEN:UI_MUTED);offset=32;}
        pax_draw_text(&fb,selected?UI_GREEN:UI_MUTED,pax_font_sky_mono,12,x+offset,y+11,names[i]);
        pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,18,x+14,y+33,values[i]);
        pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,x+14,y+65,details[i]);
    }
    pax_draw_text(&fb,home_exit_failed||draft_exit_discard||archive_exit_discard?KEY_TRIANGLE_COLOR:UI_MUTED,pax_font_sky_mono,11,20,logical_v_res-UI_HINT_OFFSET,
        archive_exit_discard?"Older messages are not saved. Choose Exit again to discard the unsaved archive queue.":home_exit_waiting?"Finishing saved data and radio confirmation before closing...":draft_exit_discard?"Draft not saved. Choose Exit again to leave unsaved edits.":home_exit_discard?"Telemetry not saved. Choose Exit again to leave without recent telemetry.":home_exit_failed?"Saved data could not be written. Retry Exit before powering off.":"Arrows/Tab: choose  Enter: open  Shape keys: quick access");
}
