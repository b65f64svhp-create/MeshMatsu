// Platform routing only. The menu model supplies shared labels, order and bounds.
static mesh_settings_group_t settings_menu_group(void){
    if(current_screen==SCREEN_NODE_SETTINGS_MENU)return MESH_SETTINGS_NODE;
    if(current_screen==SCREEN_DISPLAY_SETTINGS_MENU)return MESH_SETTINGS_DISPLAY;
    return MESH_SETTINGS_ROOT;
}
static size_t *settings_menu_cursor(mesh_settings_group_t group){
    if(group==MESH_SETTINGS_NODE)return &node_settings_selection;
    if(group==MESH_SETTINGS_DISPLAY)return &display_settings_selection;
    return &settings_menu_selection;
}
static void open_setting(mesh_setting_action_t action){
    switch(action){
        case SETTING_FIRST_USE: open_first_use(false);break;
        case SETTING_ABOUT: current_screen=SCREEN_ABOUT;about_action_selection=0;mesh_radio_request_metadata();break;
        case SETTING_NODE_GROUP: current_screen=SCREEN_NODE_SETTINGS_MENU;node_settings_selection=0;break;
        case SETTING_CHANNELS: current_screen=SCREEN_CHANNELS;channel_selection=0;break;
        case SETTING_SHARED_CHANNEL:
            current_screen=SCREEN_CHANNEL_IMPORT;import_slot=1;import_pending=false;import_feedback[0]='\0';mesh_radio_request_owner();break;
        case SETTING_MANUAL_CHANNEL: open_manual_channel();break;
        case SETTING_DISPLAY_GROUP: current_screen=SCREEN_DISPLAY_SETTINGS_MENU;display_settings_selection=0;break;
        case SETTING_POWER: open_power_settings();break;
        case SETTING_IDENTITY: current_screen=SCREEN_SETTINGS;break;
        case SETTING_ROLE: open_role_settings();break;
        case SETTING_RADIO: open_radio_settings();break;
        case SETTING_POSITION: open_position_settings();break;
        case SETTING_TELEMETRY: open_telemetry_settings();break;
        case SETTING_BACKUP: open_config_backup();break;
        case SETTING_NEIGHBORS: open_neighbor_info(mesh_radio_local_node());break;
        case SETTING_ARCHIVE: open_archive_settings();break;
        case SETTING_RANGE: open_range_testing(0);break;
        case SETTING_SECURITY: open_security_centre(mesh_radio_local_node());break;
        case SETTING_BEHAVIOR: open_behavior_settings();break;
        case SETTING_SCREEN: open_display_settings();break;
        case SETTING_ALERTS: open_channel_alerts();break;
        case SETTING_TEXT: open_text_settings();break;
        case SETTING_TIME: open_time_settings();break;
        case SETTING_HISTORY: open_history_settings();break;
        case SETTING_SOUND: open_sound_settings();break;
        case SETTING_UNITS: open_units_settings();break;
    }
}
static bool settings_menu_navigation(int key){
    if(current_screen!=SCREEN_SETTINGS_MENU&&current_screen!=SCREEN_NODE_SETTINGS_MENU&&current_screen!=SCREEN_DISPLAY_SETTINGS_MENU)return false;
    mesh_settings_group_t group=settings_menu_group();
    size_t *selected=settings_menu_cursor(group);
    if(key==BSP_INPUT_NAVIGATION_KEY_UP||key==BSP_INPUT_NAVIGATION_KEY_DOWN){
        size_t next=mesh_settings_step(group,*selected,key==BSP_INPUT_NAVIGATION_KEY_DOWN?1:-1);
        if(next==*selected)return false;
        *selected=next;
    }else if(key==BSP_INPUT_NAVIGATION_KEY_RETURN||key==BSP_INPUT_NAVIGATION_KEY_RIGHT){
        const mesh_setting_entry_t *entry=mesh_settings_entry(group,*selected);
        if(!entry)return false;
        open_setting(entry->action);
    }else return false;
    return true;
}
