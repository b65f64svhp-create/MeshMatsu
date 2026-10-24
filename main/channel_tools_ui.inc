// Channel management is an explicit review flow; QR generation never writes the radio.
enum { CT_MENU=1,CT_QR,CT_LINK,CT_IMPORT,CT_KEY,CT_ROLE,CT_CONFIRM,CT_WAIT };
enum { CT_CHANGE_KEY=1,CT_CHANGE_ROLE,CT_DISABLE,CT_ERASE,CT_IMPORT_CHANNEL };
static unsigned ct_mode,ct_selection,ct_action,ct_field;
static uint8_t ct_role,ct_slot,ct_entry;
static bool ct_confirm,ct_show_key,ct_qr_valid;
static uint32_t ct_revision;
static char ct_url[MESH_CHANNEL_URL_MAX],ct_key[67],ct_feedback[120];
static EXT_RAM_BSS_ATTR mesh_channel_share_t ct_share;
static uint8_t ct_qr[qrcodegen_BUFFER_LEN_FOR_VERSION(20)],ct_qr_work[qrcodegen_BUFFER_LEN_FOR_VERSION(20)];
static char ct_share_status[140];
static int64_t ct_share_retry_at;
static void ct_refresh_share(void) {
    mesh_channel_t current={0};
    bool loaded=mesh_radio_get_channel(ct_slot,&current)&&current.loaded;
    if(loaded&&!current.role){
        ct_qr_valid=false;
        snprintf(ct_share_status,sizeof(ct_share_status),"Slot %u is disabled. Esc: back. Use Change Channel Role or Import Channel Link before sharing.",ct_slot);
        return;
    }
    char latest[512];
    if(!loaded||!mesh_radio_channel_url(ct_slot,latest,sizeof(latest))){
        ct_qr_valid=false;
        snprintf(ct_share_status,sizeof(ct_share_status),"Reading channel %u from the radio... The QR will appear when its settings are ready.",ct_slot);
        int64_t now=esp_timer_get_time();
        if(now>=ct_share_retry_at){
            ct_share_retry_at=now+3000000;
            mesh_radio_request_channel(ct_slot);
            // An inherited key may need the primary's snapshot too.
            if(loaded&&current.role==2)for(uint8_t i=0;i<MESH_CHANNEL_LIMIT;i++){
                mesh_channel_t primary;
                if(!mesh_radio_get_channel(i,&primary)||primary.role==1)mesh_radio_request_channel(i);
            }
        }
        return;
    }
    // Recover even when the loaded URL is identical to the previous cached URL.
    if(!ct_qr_valid||strcmp(latest,ct_url)){
        snprintf(ct_url,sizeof(ct_url),"%s",latest);
        ct_qr_valid=qrcodegen_encodeText(ct_url,ct_qr_work,ct_qr,qrcodegen_Ecc_MEDIUM,1,20,qrcodegen_Mask_AUTO,true);
    }
    snprintf(ct_share_status,sizeof(ct_share_status),"%s",ct_qr_valid?"":"Could not create the QR. Enter: retry. Esc: back.");
}
static void open_channel_tools(void) {
    current_screen=SCREEN_CHANNEL_TOOLS;ct_mode=CT_MENU;ct_selection=ct_action=ct_field=0;
    ct_slot=(uint8_t)channel_selection;ct_show_key=ct_confirm=ct_qr_valid=false;ct_feedback[0]=0;ct_share_status[0]=0;ct_share_retry_at=0;
    mesh_channel_t channel;if(mesh_radio_get_channel(ct_slot,&channel))ct_revision=channel.revision;
    memset(ct_key,0,sizeof(ct_key));memset(ct_url,0,sizeof(ct_url));mesh_radio_request_owner();
    for(uint8_t i=0;i<8;i++)mesh_radio_request_channel(i);
}
static void ct_review(unsigned action) {ct_action=action;ct_confirm=false;ct_mode=CT_CONFIRM;ct_feedback[0]=0;}
static void ct_start_change(void) {
    bool ok=false;
    if(ct_action==CT_CHANGE_KEY)ok=mesh_radio_replace_channel_key(ct_slot,ct_key,ct_revision);
    else if(ct_action==CT_CHANGE_ROLE)ok=mesh_radio_set_channel_role(ct_slot,ct_role,false,ct_revision);
    else if(ct_action==CT_DISABLE||ct_action==CT_ERASE)ok=mesh_radio_set_channel_role(ct_slot,0,ct_action==CT_ERASE,ct_revision);
    else if(ct_action==CT_IMPORT_CHANNEL&&ct_entry<ct_share.count){mesh_channel_t current={0};mesh_radio_get_channel(ct_slot,&current);
        ok=mesh_radio_manage_channel(ct_slot,current.role?current.role:ct_slot?2:1,ct_share.settings[ct_entry],ct_share.sizes[ct_entry],ct_revision);}
    if(ok){ct_mode=CT_WAIT;ct_feedback[0]=0;ct_show_key=false;memset(ct_key,0,sizeof(ct_key));}
    else snprintf(ct_feedback,sizeof(ct_feedback),"Not sent. Wait for radio; check slot order, primary role and position privacy. Reopen to retry.");
}
static void ct_begin_import(void) {
    if(!mesh_radio_parse_channel_url(ct_url,&ct_share)){snprintf(ct_feedback,sizeof(ct_feedback),"Invalid Meshtastic channel link. Check the complete URL.");return;}
    ct_entry=0;ct_field=0;ct_mode=CT_IMPORT;ct_feedback[0]=0;
    ct_slot=1;for(uint8_t i=1;i<8;i++){mesh_channel_t c;if(mesh_radio_get_channel(i,&c)&&!c.role){ct_slot=i;break;}}
}
static bool channel_tools_navigation(int key) {
    if(current_screen==SCREEN_CHANNEL_EDIT&&channel_field==4&&(key==BSP_INPUT_NAVIGATION_KEY_RETURN||key==BSP_INPUT_NAVIGATION_KEY_RIGHT)){open_channel_tools();return true;}
    if(current_screen!=SCREEN_CHANNEL_TOOLS)return false;
    if(key==BSP_INPUT_NAVIGATION_KEY_F1||key==BSP_INPUT_NAVIGATION_KEY_F2||key==BSP_INPUT_NAVIGATION_KEY_F3||key==BSP_INPUT_NAVIGATION_KEY_F4||key==BSP_INPUT_NAVIGATION_KEY_F5||key==BSP_INPUT_NAVIGATION_KEY_F6)return false;
    if(key==BSP_INPUT_NAVIGATION_KEY_ESC){
        if(ct_mode==CT_MENU)return false;
        if(ct_mode==CT_CONFIRM)ct_mode=ct_action==CT_CHANGE_KEY?CT_KEY:ct_action==CT_CHANGE_ROLE?CT_ROLE:ct_action==CT_IMPORT_CHANNEL?CT_IMPORT:CT_MENU;
        else ct_mode=CT_MENU;
        ct_show_key=false;ct_feedback[0]=0;return true;
    }
    bool forward=key==BSP_INPUT_NAVIGATION_KEY_DOWN||key==BSP_INPUT_NAVIGATION_KEY_RIGHT;
    bool backward=key==BSP_INPUT_NAVIGATION_KEY_UP||key==BSP_INPUT_NAVIGATION_KEY_LEFT;
    bool enter=key==BSP_INPUT_NAVIGATION_KEY_RETURN;
    if(ct_mode==CT_MENU){
        if(key==BSP_INPUT_NAVIGATION_KEY_DOWN&&ct_selection<6)ct_selection++;else if(key==BSP_INPUT_NAVIGATION_KEY_UP&&ct_selection)ct_selection--;
        else if(enter||key==BSP_INPUT_NAVIGATION_KEY_RIGHT){
            mesh_channel_t c;if(!mesh_radio_get_channel((uint8_t)channel_selection,&c))return true;
            ct_slot=(uint8_t)channel_selection;ct_revision=c.revision;ct_feedback[0]=0;
            if(ct_selection==0){ct_qr_valid=false;ct_share_retry_at=0;ct_mode=CT_QR;ct_refresh_share();}
            else if(ct_selection==1){ct_mode=CT_LINK;ct_url[0]=0;}
            else if(ct_selection==2||ct_selection==3){ct_mode=CT_KEY;ct_key[0]=0;ct_show_key=false;if(ct_selection==3&&!mesh_radio_generate_channel_key(ct_key,sizeof(ct_key)))snprintf(ct_feedback,sizeof(ct_feedback),"Could not generate a key.");}
            else if(ct_selection==4){ct_mode=CT_ROLE;ct_role=c.role;}
            else ct_review(ct_selection==5?CT_DISABLE:CT_ERASE);
        }
    }else if(ct_mode==CT_CONFIRM){if(forward)ct_confirm=true;else if(backward)ct_confirm=false;else if(enter){if(ct_confirm)ct_start_change();else ct_mode=CT_MENU;}}
    else if(ct_mode==CT_QR&&enter){ct_share_retry_at=0;ct_refresh_share();}
    else if(ct_mode==CT_LINK){if(enter)ct_begin_import();}
    else if(ct_mode==CT_KEY){if(enter){if(mesh_radio_channel_key_valid(ct_key))ct_review(CT_CHANGE_KEY);else snprintf(ct_feedback,sizeof(ct_feedback),"Use Base64 or 32/64 hexadecimal digits.");}}
    else if(ct_mode==CT_ROLE){if(forward)ct_role=(ct_role+1)%3;else if(backward)ct_role=(ct_role+2)%3;else if(enter)ct_review(CT_CHANGE_ROLE);}
    else if(ct_mode==CT_IMPORT){
        if(key==BSP_INPUT_NAVIGATION_KEY_UP&&ct_field)ct_field--;else if(key==BSP_INPUT_NAVIGATION_KEY_DOWN&&ct_field<2)ct_field++;
        else if(key==BSP_INPUT_NAVIGATION_KEY_LEFT||key==BSP_INPUT_NAVIGATION_KEY_RIGHT){
            if(!ct_field)ct_entry=(ct_entry+(forward?1:ct_share.count-1))%ct_share.count;
            else if(ct_field==1){if(forward&&ct_slot<7)ct_slot++;else if(!forward&&ct_slot)ct_slot--;}
        }else if(enter){if(ct_field<2)ct_field++;else {mesh_channel_t c;if(mesh_radio_get_channel(ct_slot,&c)){ct_revision=c.revision;ct_review(CT_IMPORT_CHANNEL);}else snprintf(ct_feedback,sizeof(ct_feedback),"Slot not loaded. Wait and retry.");}}
    }else if(ct_mode==CT_WAIT&&enter){mesh_channel_operation_t op=mesh_radio_channel_operation();if(op.state!=MESH_FAVORITE_SAVING){ct_mode=CT_MENU;mesh_channel_t c;if(mesh_radio_get_channel(ct_slot,&c))ct_revision=c.revision;}}
    return true;
}
static bool channel_tools_key(char key) {
    if(current_screen==SCREEN_CHANNEL_EDIT&&(key=='\r'||key=='\n')&&channel_field==4){open_channel_tools();return true;}
    if(current_screen==SCREEN_CHANNELS&&(key=='i'||key=='I')){open_channel_tools();ct_mode=CT_LINK;return true;}
    if(current_screen==SCREEN_CHANNEL_EDIT&&(key=='\t') )return false;
    if(current_screen!=SCREEN_CHANNEL_TOOLS)return false;
    if(key=='\r'||key=='\n')return channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
    if(ct_mode==CT_LINK||ct_mode==CT_KEY){char *text=ct_mode==CT_LINK?ct_url:ct_key;size_t cap=ct_mode==CT_LINK?sizeof(ct_url):sizeof(ct_key),n=strlen(text);
        if(ct_mode==CT_KEY&&key==' '){ct_show_key=!ct_show_key;return true;}
        if((key=='\b'||key==127)&&n)text[n-1]=0;else if(key>=33&&key<=126&&n+1<cap){text[n]=key;text[n+1]=0;}
        ct_feedback[0]=0;
    }else if(ct_mode==CT_IMPORT&&key=='\t')ct_field=(ct_field+1)%3;
    return true;
}
