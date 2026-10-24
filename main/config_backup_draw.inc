static void draw_config_backup(void){
    char text[200];
    if(cf_mode==CF_MENU){
        const char *names[]={
            "BACK UP FULL CONFIGURATION","RESTORE A CONFIGURATION SECTION","KEY-ONLY BACKUP & RESTORE"
        }
        ;
        const char *details[]={
            "Radio settings, modules, channels, app preferences and nicknames","Unlock, review and restore one section at a time","Existing identity/channel-key recovery, including other radios"
        }
        ;
        for(unsigned i=0;i<3;i++)bk_row(i,cf_selection,90+i*60,names[i],details[i]);
        draw_rich_text("Encrypted .mmc files: SD / meshmatsu / backups. Messages, telemetry, maps, game records and the radio node database are separate data. Keep a copy on your computer.",20,290,13,logical_h_res-40,24,4,UI_MUTED);
    }
    else if(cf_mode==CF_PASS){
        draw_rich_text(cf_restoring?"Unlock the selected configuration backup. You will choose and review a section before any restore.":"Choose a 12-96 character passphrase. It is not saved and cannot be recovered if forgotten.",20,90,13,logical_h_res-40,24,3,UI_MUTED);
        unsigned last=cf_restoring?1:2;
        for(unsigned i=0;i<=last;i++){
            snprintf(text,sizeof(text),"%u characters entered (hidden)",(unsigned)strlen(i?cf_repeat:cf_password));
            bk_row(i,cf_field,180+i*55,i==last?(cf_restoring?"UNLOCK":"CREATE ENCRYPTED BACKUP"):i?"CONFIRM PASSPHRASE":"PASSPHRASE",i==last?"Enter to continue":text);
        }
    }
    else if(cf_mode==CF_LIST){
        draw_rich_text("Choose a configuration backup (.mmc). Key-only files (.mmk) are in the separate key recovery menu.",20,86,13,logical_h_res-40,22,2,UI_MUTED);
        if(!cf_count)draw_rich_text("No configuration backups found in SD / meshmatsu / backups.",20,150,16,logical_h_res-40,28,3,UI_GREEN);
        size_t first=cf_selection>=5?cf_selection-4:0;
        for(size_t i=first;i<cf_count&&i<first+5;i++)bk_row(i,cf_selection,140+(i-first)*47,cf_files[i],NULL);
    }
    else if(cf_mode==CF_SELECT){
        snprintf(text,sizeof(text),"%.39s / !%08lx / %.32s",cf_saved.name,(unsigned long)cf_saved.node,cf_saved.firmware);
        draw_rich_text(text,20,86,13,logical_h_res-40,22,2,UI_GREEN);
        draw_rich_text("Choose one section to restore. Radio sections require the same radio, hardware and firmware version.",20,126,10,logical_h_res-40,18,2,UI_MUTED);
        size_t first=cf_selection>=5?cf_selection-4:0;
        for(size_t i=first;i<cf_saved.count&&i<first+5;i++){
            mesh_config_label(&cf_saved.records[i],text,sizeof(text));
            bk_row(i,cf_selection,170+(i-first)*43,text,NULL);
        }
    }
    else if(cf_mode==CF_REVIEW){
        const mesh_cfg_record_t *r=&cf_saved.records[cf_target];
        mesh_config_label(r,text,sizeof(text));
        draw_rich_text(text,20,88,16,logical_h_res-40,28,2,UI_GREEN);
        snprintf(text,sizeof(text),"From !%08lx / %.32s\nTo   !%08lx / %.32s",(unsigned long)cf_saved.node,cf_saved.firmware,(unsigned long)cf_original.node,cf_original.firmware);
        draw_rich_text(text,20,150,12,logical_h_res-40,24,3,WHITE);
        const char *detail=r->kind==CFG_OWNER?"Restores radio names, user ID and licensed status. Licensed mode may change channel encryption. Security keys use their separate section.":r->kind==CFG_FIXED?"Restores the saved fixed latitude and longitude. Live GPS fixes are not backed up. Restore fixed coordinates before enabling fixed mode if the radio has no location.":r->kind==CFG_APP?"Replaces app display, alert, sound, timezone and unit preferences. Restart MeshMatsu afterward to apply them.":r->kind==CFG_ALIASES?"Replaces all local node nicknames with the saved list. Radio names stay unchanged.":r->kind==CFG_MUTES?"Replaces the per-node notification mute list. Messages and unread counts stay.":r->kind==CFG_HISTORY?"Changes the message capacity. A smaller limit can discard older messages; those messages are not included in this backup.":r->kind==CFG_CHANNEL?"Replaces this slot's full channel settings and key. Promoting a primary demotes the old primary. Channel ordering/privacy rules still apply.":r->kind==CFG_RADIO&&r->id==8?"Replaces security settings, identity keys and admin permissions. Restores that disable serial access or enable managed mode are refused.":"Replaces the complete selected radio section. The radio may restart. Device-role changes can also change firmware defaults in other sections.";
        draw_rich_text(detail,20,232,12,logical_h_res-40,23,4,KEY_TRIANGLE_COLOR);
        draw_rich_text("First saves a verified encrypted safety backup, then checks a fresh snapshot. No automatic resend after an unconfirmed radio restore.",20,328,10,logical_h_res-40,18,2,UI_MUTED);
        for(unsigned i=0;i<2;i++){
            int w=(logical_h_res-52)/2,x=20+i*(w+12);
            bool selected=cf_confirm==(i!=0);
            pax_draw_rect(&fb,selected?UI_GREEN:UI_PANEL,x,366,w,32);
            pax_draw_text(&fb,selected?BLACK:UI_GREEN,pax_font_sky_mono,12,x+12,376,i?"CONFIRM RESTORE":"CANCEL");
        }
    }
    else{
        const char *text=cf_mode==CF_DONE?cf_feedback:cf_mode==CF_VERIFY?"Change sent once. Waiting for a complete fresh radio readback...":cf_mode==CF_SAFETY?"Saving and checking the encrypted safety backup. Nothing restored yet...":cf_mode==CF_READ_SAVE||cf_mode==CF_READ_REVIEW||cf_mode==CF_BEFORE_APPLY?"Reading every radio configuration section, then capturing local preferences...":"Working on encrypted SD files. Messaging continues.";
        draw_rich_text(text,20,110,16,logical_h_res-40,28,8,UI_GREEN);
    }
    if(cf_feedback[0]&&cf_mode!=CF_DONE)draw_rich_text(cf_feedback,20,398,9,logical_h_res-40,10,2,KEY_TRIANGLE_COLOR);
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,logical_v_res-UI_HINT_OFFSET,cf_mode==CF_REVIEW?"Left/Right: Cancel or Confirm  Enter: choose  Esc: cancel":"Up/Down: choose  Tab: field  Enter: continue  Esc: back");
}
