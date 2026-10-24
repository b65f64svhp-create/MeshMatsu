// Position drafts never write to the radio until an explicit Save action.
static mesh_position_config_t position_draft;
static bool position_loaded, position_edited, position_pending, fixed_pending;
static size_t position_field, fixed_field;
static uint32_t fixed_revision;
static char fixed_lat[18], fixed_lon[18];
static bool fixed_lat_edited, fixed_lon_edited;
static char position_feedback[96], fixed_feedback[96];
static void open_position_settings(void) {
    current_screen = SCREEN_POSITION_SETTINGS; position_field = 0;
    position_loaded = mesh_radio_get_position_config(&position_draft);
    position_edited = position_pending = false; position_feedback[0] = '\0';
    mesh_radio_request_position_config(); mesh_radio_request_owner();
}
static void open_fixed_settings(void) {
    current_screen = SCREEN_FIXED_POSITION; fixed_field = 0; fixed_pending = false;
    fixed_lat[0] = fixed_lon[0] = '\0'; fixed_feedback[0] = '\0';
    fixed_lat_edited = fixed_lon_edited = false;
    mesh_node_t own;
    if (mesh_radio_get_node_by_number(mesh_radio_local_node(), &own) && own.position_available) {
        snprintf(fixed_lat, sizeof(fixed_lat), "%.7f", own.latitude_i / 1e7);
        snprintf(fixed_lon, sizeof(fixed_lon), "%.7f", own.longitude_i / 1e7);
    }
    mesh_position_config_t current;
    fixed_revision = mesh_radio_get_position_config(&current) ? current.revision : 0;
    mesh_radio_request_position_config(); mesh_radio_request_owner();
}
static const char *location_operation_feedback(void) {
    switch (mesh_radio_location_save().state) {
        case MESH_LOCATION_SAVING: return "Saving; waiting for radio confirmation...";
        case MESH_LOCATION_SAVED: return "Saved and confirmed by the radio.";
        case MESH_LOCATION_FAILED: return "Not confirmed. Reopen to check the radio settings.";
        case MESH_LOCATION_TIMEOUT: return "Confirmation timed out. Reopen to check before retrying.";
        default: return "";
    }
}
static bool position_navigation(int key) {
    if (current_screen != SCREEN_POSITION_SETTINGS && current_screen != SCREEN_FIXED_POSITION) return false;
    bool fixed = current_screen == SCREEN_FIXED_POSITION;
    size_t *field = fixed ? &fixed_field : &position_field;
    if (key == BSP_INPUT_NAVIGATION_KEY_UP) { if (*field) (*field)--; }
    else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) { if (*field < (fixed ? 2 : 5)) (*field)++; }
    else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN || key == BSP_INPUT_NAVIGATION_KEY_RIGHT || key == BSP_INPUT_NAVIGATION_KEY_LEFT) {
        if (mesh_radio_location_save().state == MESH_LOCATION_SAVING) return true;
        if (fixed) {
            if (key == BSP_INPUT_NAVIGATION_KEY_RETURN && *field == 2) {
                int32_t lat, lon;
                if (!mesh_location_parse(fixed_lat, true, &lat) || !mesh_location_parse(fixed_lon, false, &lon))
                    snprintf(fixed_feedback,sizeof(fixed_feedback),"Enter latitude -90..90 and longitude -180..180.");
                else if (mesh_radio_set_fixed_position(lat, lon, fixed_revision)) { fixed_pending = true; fixed_feedback[0] = '\0'; }
                else snprintf(fixed_feedback,sizeof(fixed_feedback),"Not saved. Reopen this page after radio sync and retry.");
            }
        } else if (*field == 3 && key != BSP_INPUT_NAVIGATION_KEY_LEFT) open_fixed_settings();
        else if (*field == 4 && key != BSP_INPUT_NAVIGATION_KEY_LEFT) { current_screen = SCREEN_CHANNELS; channel_selection = 0; }
        else if (*field == 5 && key == BSP_INPUT_NAVIGATION_KEY_RETURN) {
            if (position_loaded && mesh_radio_set_position_config(&position_draft)) { position_pending=true;position_feedback[0]='\0'; }
            else snprintf(position_feedback,sizeof(position_feedback),"Not saved. Reopen after sync; fixed mode needs a location.");
        } else if (*field < 3 && position_loaded && key != BSP_INPUT_NAVIGATION_KEY_RETURN) {
            if (*field == 0) {
                const uint32_t values[] = {0,300,900,1800,3600,7200,21600,86400};
                size_t i=0;for(size_t n=0;n<8;n++)if(values[n]==position_draft.broadcast_secs)i=n;
                position_draft.broadcast_secs=values[(i+8+(key==BSP_INPUT_NAVIGATION_KEY_RIGHT?1:-1))%8];
            } else if (*field == 1) position_draft.smart=!position_draft.smart;
            else position_draft.fixed=!position_draft.fixed;
            position_edited=true;position_pending=false;
            snprintf(position_feedback,sizeof(position_feedback),"Unsaved changes. Select Save to apply.");
        }
    } else return false;
    return true;
}
static bool position_key(char key) {
    if (current_screen != SCREEN_POSITION_SETTINGS && current_screen != SCREEN_FIXED_POSITION) return false;
    if (key=='\t') {
        if(current_screen==SCREEN_FIXED_POSITION)fixed_field=(fixed_field+1)%3;
        else position_field=(position_field+1)%6;
    } else if(key=='\r'||key=='\n') position_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
    else if(current_screen==SCREEN_FIXED_POSITION && fixed_field<2 && mesh_radio_location_save().state!=MESH_LOCATION_SAVING) {
        char *text=fixed_field?fixed_lon:fixed_lat;bool *edited=fixed_field?&fixed_lon_edited:&fixed_lat_edited;
        if(!fixed_lat_edited && !fixed_lon_edited) {
            mesh_position_config_t current;
            if(mesh_radio_get_position_config(&current))fixed_revision=current.revision;
        }
        size_t n=strlen(text);
        if(key=='\b'||key==0x7f){if(n)text[n-1]='\0';*edited=true;}
        else if((key>='0'&&key<='9')||key=='.'||key=='-'||key=='+') {
            if(!*edited){text[0]='\0';n=0;*edited=true;}
            if(n+1<sizeof(fixed_lat)){text[n]=key;text[n+1]='\0';}
        }
        fixed_pending=false;fixed_feedback[0]='\0';
    }
    return true;
}
