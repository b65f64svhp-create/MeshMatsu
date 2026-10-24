// Local-radio DeviceConfig editor; shares the backend's verified single-write operation.
static mesh_device_config_t behavior_baseline;
static uint32_t behavior_interval_draft,behavior_mode_draft;
static size_t behavior_field;
static bool behavior_loaded,behavior_edited,behavior_pending;
static char behavior_feedback[112];
static const uint32_t behavior_intervals[]={3600,10800,21600,43200,86400,172800,259200};
static const char *behavior_mode_name(uint32_t mode){
    const char *names[]={"ALL MESHES","ALL - SKIP DECODING","LOCAL CHANNELS ONLY","KNOWN NODES ONLY","NO REBROADCAST","CORE MESSAGES ONLY"};
    return mode<6?names[mode]:"UNKNOWN CURRENT MODE";
}
static const char *behavior_explanation(uint32_t mode){
    const char *text[]={
        "Forward eligible packets, including other meshes using matching radio settings. Normal role rules still apply.",
        "Repeater only: forward without decoding. Selecting this on another role would act like All Meshes.",
        "Forward packets on your primary and secondary channels; ignore traffic from foreign meshes.",
        "Like Local Channels Only, also requiring a sender present in the radio's node database.",
        "Do not forward other nodes' traffic. Available for Tracker, Sensor and TAK Tracker roles.",
        "Forward core traffic: text, node info, position, telemetry and routing. Other app ports are excluded."
    };
    return mode<6?text[mode]:"This value is not supported by this editor. Select a supported mode before saving.";
}
static void open_behavior_settings(void){
    current_screen=SCREEN_RADIO_BEHAVIOR;behavior_field=0;behavior_loaded=behavior_edited=behavior_pending=false;behavior_feedback[0]=0;
    mesh_radio_request_device();mesh_radio_request_owner();
}
static void behavior_refresh(void){
    mesh_device_config_t actual;
    bool loaded=mesh_radio_get_device(&actual);
    if(loaded&&!behavior_edited&&!behavior_pending){behavior_baseline=actual;behavior_interval_draft=actual.node_info_broadcast_secs;behavior_mode_draft=actual.rebroadcast_mode;behavior_loaded=true;}
    if(!loaded)mesh_radio_request_device();
    if(behavior_pending){
        mesh_role_save_t save=mesh_radio_role_save();
        if(save.state!=MESH_ROLE_SAVING){
            behavior_pending=false;
            if(save.state==MESH_ROLE_SAVED){behavior_edited=false;snprintf(behavior_feedback,sizeof(behavior_feedback),"Radio behavior confirmed.");}
            else snprintf(behavior_feedback,sizeof(behavior_feedback),"Not confirmed. Reopen to read the radio before retrying.");
        }
    }
}
static bool behavior_navigation(int key){
    if(current_screen!=SCREEN_RADIO_BEHAVIOR)return false;
    behavior_refresh();
    bool right=key==BSP_INPUT_NAVIGATION_KEY_RIGHT,left=key==BSP_INPUT_NAVIGATION_KEY_LEFT;
    if(key==BSP_INPUT_NAVIGATION_KEY_UP){if(behavior_field)behavior_field--;}
    else if(key==BSP_INPUT_NAVIGATION_KEY_DOWN){if(behavior_field<2)behavior_field++;}
    else if(key==BSP_INPUT_NAVIGATION_KEY_RETURN){
        if(behavior_field<2)behavior_field++;
        else if(behavior_loaded&&!behavior_pending){
            mesh_device_config_t actual;
            if(!mesh_radio_get_device(&actual)||actual.revision!=behavior_baseline.revision){snprintf(behavior_feedback,sizeof(behavior_feedback),"Radio values changed. Reopen before saving.");}
            else if(actual.rebroadcast_mode==behavior_mode_draft&&actual.node_info_broadcast_secs==behavior_interval_draft){snprintf(behavior_feedback,sizeof(behavior_feedback),"No changes to save.");}
            else if(behavior_interval_draft<3600){snprintf(behavior_feedback,sizeof(behavior_feedback),"Choose an announcement interval of at least 1 hour.");}
            else if(mesh_radio_set_behavior(behavior_mode_draft,behavior_interval_draft,behavior_baseline.revision)){
                behavior_pending=true;snprintf(behavior_feedback,sizeof(behavior_feedback),"Saving once; waiting for radio readback...");
            }else {mesh_radio_request_owner();snprintf(behavior_feedback,sizeof(behavior_feedback),"Not saved. Check role, permissions or another pending save.");}
        }
    }else if((left||right)&&behavior_field<2&&behavior_loaded&&!behavior_pending&&mesh_radio_role_save().state!=MESH_ROLE_SAVING){
        if(!behavior_field){
            size_t selected=0;for(size_t i=0;i<sizeof(behavior_intervals)/sizeof(*behavior_intervals);i++)if(behavior_intervals[i]==behavior_interval_draft)selected=i;
            selected=(selected+(right?1:6))%7;behavior_interval_draft=behavior_intervals[selected];
        }else{
            uint32_t next=behavior_mode_draft<6?behavior_mode_draft:0;
            for(unsigned i=0;i<6;i++){next=(next+(right?1:5))%6;if(mesh_radio_behavior_supported(behavior_baseline.role,next)){behavior_mode_draft=next;break;}}
        }
        behavior_edited=true;snprintf(behavior_feedback,sizeof(behavior_feedback),"Unsaved changes. Select Save to apply.");
    }else return false;
    return true;
}
static bool behavior_key(char key){
    if(current_screen!=SCREEN_RADIO_BEHAVIOR)return false;
    if(key=='\r'||key=='\n')behavior_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
    else if(key=='\t')behavior_field=(behavior_field+1)%3;
    return true;
}
