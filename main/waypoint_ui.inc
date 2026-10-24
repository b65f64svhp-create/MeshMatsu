// W from Maps opens saved waypoints; N creates at the visible map centre.
enum { WP_LIST,WP_VIEW,WP_EDIT,WP_CONFIRM,WP_DELETE };
static unsigned wp_mode,wp_field,wp_action;
static size_t wp_selection;
static bool wp_confirm,wp_existing,wp_text_replaced;
static mesh_waypoint_t wp_selected,wp_draft;
static unsigned wp_expiry; // 0 keep original/never,1 hour,24 hours,7 days,never.
static char wp_feedback[120];
static mesh_waypoint_t wp_last_send;
static bool wp_have_send;
static const uint32_t wp_icons[]={0x1f4cd,0x26fa,0x26a0,0x1f4e1,0x1f3e0};
static void open_waypoints(bool create) {
    current_screen=SCREEN_WAYPOINTS;wp_mode=WP_LIST;wp_selection=0;wp_feedback[0]=0;
    if(create){
        memset(&wp_draft,0,sizeof(wp_draft));wp_draft.id=mesh_radio_new_waypoint_id();
        double latitude,longitude;mesh_map_unproject(map_view.x,map_view.y,&latitude,&longitude);
        wp_draft.latitude_i=(int32_t)llround(latitude*1e7);wp_draft.longitude_i=(int32_t)llround(longitude*1e7);
        wp_draft.icon=wp_icons[0];wp_draft.locked_to=mesh_radio_local_node();wp_draft.expire=INT32_MAX;
        wp_draft.channel=message_channel;mesh_channel_t c;
        if(!mesh_radio_get_channel(wp_draft.channel,&c)||!c.role)for(uint8_t i=0;i<8;i++)if(mesh_radio_get_channel(i,&c)&&c.role){wp_draft.channel=i;break;}
        wp_mode=WP_EDIT;wp_existing=false;wp_field=0;wp_expiry=0;wp_text_replaced=false;
    }
}
static void wp_edit_selected(void){
    if(!mesh_radio_waypoint_editable(&wp_selected)){snprintf(wp_feedback,sizeof(wp_feedback),"Locked to another node. Only that node can change it.");return;}
    wp_draft=wp_selected;wp_existing=true;wp_mode=WP_EDIT;wp_field=0;wp_expiry=0;wp_text_replaced=false;wp_feedback[0]=0;
}
static void wp_send(bool remove) {
    if(wp_existing){mesh_waypoint_t latest;if(!mesh_radio_find_waypoint(wp_selected.id,wp_selected.peer_node,wp_selected.channel,&latest)||latest.packet_id!=wp_selected.packet_id||!mesh_radio_waypoint_editable(&latest)){
        snprintf(wp_feedback,sizeof(wp_feedback),"Waypoint changed while editing. Reopen it before sending.");return;}}
    mesh_waypoint_t outgoing=wp_draft;
    if(!remove&&wp_expiry){
        if(wp_expiry==4)outgoing.expire=INT32_MAX;
        else {uint32_t now=mesh_radio_time();
            time_t local=time(NULL);if(local>=1700000000&&(uint64_t)local<=UINT32_MAX)now=(uint32_t)local;
            uint32_t duration=wp_expiry==1?3600:wp_expiry==2?86400:604800;
            if(now<1700000000||now>UINT32_MAX-duration){snprintf(wp_feedback,sizeof(wp_feedback),"Set a valid device time before using timed expiry.");return;}
            outgoing.expire=now+duration;}}
    if(mesh_radio_send_waypoint(&outgoing,remove)){
        wp_feedback[0]=0;
        wp_have_send=mesh_radio_find_waypoint(outgoing.id,outgoing.peer_node,outgoing.channel,&wp_last_send);
        if(remove){wp_mode=WP_LIST;}
        else {mesh_radio_find_waypoint(outgoing.id,outgoing.peer_node,outgoing.channel,&wp_selected);wp_mode=WP_VIEW;}
    }else snprintf(wp_feedback,sizeof(wp_feedback),"Not queued. Check radio, channel, lock or a pending send.");
}
static bool waypoint_navigation(int key) {
    if(current_screen!=SCREEN_WAYPOINTS)return false;
    if(key==BSP_INPUT_NAVIGATION_KEY_F1||key==BSP_INPUT_NAVIGATION_KEY_F2||key==BSP_INPUT_NAVIGATION_KEY_F3||key==BSP_INPUT_NAVIGATION_KEY_F4||key==BSP_INPUT_NAVIGATION_KEY_F5||key==BSP_INPUT_NAVIGATION_KEY_F6)return false;
    if(key==BSP_INPUT_NAVIGATION_KEY_ESC){
        if(wp_mode==WP_LIST)return false;
        wp_mode=wp_mode==WP_CONFIRM?WP_EDIT:wp_mode==WP_DELETE?WP_VIEW:WP_LIST;wp_feedback[0]=0;return true;}
    bool enter=key==BSP_INPUT_NAVIGATION_KEY_RETURN,forward=key==BSP_INPUT_NAVIGATION_KEY_RIGHT,backward=key==BSP_INPUT_NAVIGATION_KEY_LEFT;
    if(wp_mode==WP_LIST){size_t n=mesh_radio_waypoint_count();
        if(key==BSP_INPUT_NAVIGATION_KEY_UP&&wp_selection)wp_selection--;else if(key==BSP_INPUT_NAVIGATION_KEY_DOWN&&wp_selection<n)wp_selection++;
        else if(enter){if(!wp_selection)open_waypoints(true);else if(mesh_radio_get_waypoint(wp_selection-1,&wp_selected)){wp_mode=WP_VIEW;wp_action=0;wp_feedback[0]=0;}}}
    else if(wp_mode==WP_VIEW){
        if(key==BSP_INPUT_NAVIGATION_KEY_UP&&wp_action)wp_action--;else if(key==BSP_INPUT_NAVIGATION_KEY_DOWN&&wp_action<2)wp_action++;
        else if(enter){if(!wp_action){map_filters.waypoints_hidden=false;mesh_map_center(&map_view,wp_selected.latitude_i/1e7,wp_selected.longitude_i/1e7,map_view.zoom);map_waypoint_id=wp_selected.id;map_waypoint_peer=wp_selected.peer_node;map_waypoint_channel=wp_selected.channel;map_selected_node=0;current_screen=SCREEN_MAP;}
            else if(wp_action==1)wp_edit_selected();else if(mesh_radio_waypoint_editable(&wp_selected)){wp_draft=wp_selected;wp_existing=true;wp_confirm=false;wp_mode=WP_DELETE;}else snprintf(wp_feedback,sizeof(wp_feedback),"Locked to another node; removal cannot be shared.");}}
    else if(wp_mode==WP_EDIT){
        if(key==BSP_INPUT_NAVIGATION_KEY_UP&&wp_field){wp_field--;wp_text_replaced=false;}else if(key==BSP_INPUT_NAVIGATION_KEY_DOWN&&wp_field<7){wp_field++;wp_text_replaced=false;}
        else if(forward||backward){
            if(wp_field==3&&!wp_existing){for(unsigned tries=0;tries<8;tries++){wp_draft.channel=(wp_draft.channel+(forward?1:7))%8;mesh_channel_t c;if(mesh_radio_get_channel(wp_draft.channel,&c)&&c.role)break;}}
            else if(wp_field==4)wp_expiry=(wp_expiry+(forward?1:4))%5;
            else if(wp_field==5)wp_draft.locked_to=wp_draft.locked_to?0:mesh_radio_local_node();
            else if(wp_field==6){size_t i;for(i=0;i<5;i++)if(wp_icons[i]==wp_draft.icon)break;wp_draft.icon=wp_icons[(i+(forward?1:4))%5];}
        }else if(enter){if(wp_field<7){wp_field++;wp_text_replaced=false;}else if(wp_draft.name[0]&&wp_draft.id){wp_mode=WP_CONFIRM;wp_confirm=false;wp_feedback[0]=0;}else snprintf(wp_feedback,sizeof(wp_feedback),"Enter a name first; the radio identity must be available.");}}
    else if(wp_mode==WP_CONFIRM||wp_mode==WP_DELETE){if(forward)wp_confirm=true;else if(backward)wp_confirm=false;else if(enter){if(wp_confirm)wp_send(wp_mode==WP_DELETE);else wp_mode=wp_mode==WP_DELETE?WP_VIEW:WP_EDIT;}}
    return true;
}
static bool waypoint_key(char key) {
    if(current_screen==SCREEN_MAP&&(key=='w'||key=='W'||key=='n'||key=='N')){open_waypoints(key=='n'||key=='N');return true;}
    if(current_screen!=SCREEN_WAYPOINTS)return false;
    if(key=='\r'||key=='\n')return waypoint_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
    if(wp_mode==WP_EDIT){
        if(key=='\t'){wp_field=(wp_field+1)%8;wp_text_replaced=false;}
        else if(wp_field==2&&key==' '){double lat,lon;mesh_map_unproject(map_view.x,map_view.y,&lat,&lon);wp_draft.latitude_i=(int32_t)llround(lat*1e7);wp_draft.longitude_i=(int32_t)llround(lon*1e7);snprintf(wp_feedback,sizeof(wp_feedback),"Using the map centre. Review before sending.");}
        else if(wp_field<2){char *field=wp_field?wp_draft.description:wp_draft.name;size_t cap=wp_field?sizeof(wp_draft.description):sizeof(wp_draft.name),n=strlen(field);
            if((key=='\b'||key==127)&&n){do{n--;}while(n&&((unsigned char)field[n]&0xc0)==0x80);field[n]=0;wp_text_replaced=true;}
            else if(key>=32&&key<=126){if(!wp_text_replaced){field[0]=0;n=0;wp_text_replaced=true;}if(n+1<cap){field[n]=key;field[n+1]=0;wp_feedback[0]=0;}else snprintf(wp_feedback,sizeof(wp_feedback),"Text limit reached for this field.");}
        }
    }
    return true;
}
