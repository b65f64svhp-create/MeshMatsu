static void bk_row(unsigned row,unsigned selected,int y,const char *label,const char *detail) {
    pax_draw_rect(&fb,row==selected?UI_SELECTED:UI_PANEL,20,y,logical_h_res-40,44);
    pax_draw_rect(&fb,row==selected?UI_GREEN:UI_BORDER,20,y,3,44);
    pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,13,34,y+6,label);
    if(detail)pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,34,y+26,detail);
}
static void draw_key_backup(void) {
    char text[180];
    if(bk_mode==BK_MENU){
        const char *labels[]={"BACK UP IDENTITY KEYS","BACK UP CHANNEL KEYS","BACK UP BOTH","RESTORE FROM SD CARD"};
        const char *details[]={"Public/private pair for encrypted direct messages","Keys for all enabled channels; includes inherited keys","Identity pair and enabled channel keys in one file","Unlock a backup and choose which keys to restore"};
        for(unsigned i=0;i<4;i++)bk_row(i,bk_selection,90+i*54,labels[i],details[i]);
        draw_rich_text("Encrypted files: SD / meshmatsu / backups. Keep a copy on your Mac and remember your passphrase.",20,320,12,logical_h_res-40,22,3,UI_MUTED);
    }else if(bk_mode==BK_PASS){
        draw_rich_text(bk_restoring?"Unlock the selected backup. Nothing changes until you choose keys and confirm the restore.":"Choose a passphrase of 12-96 characters. It is not saved. A forgotten passphrase cannot be recovered.",20,86,13,logical_h_res-40,24,3,UI_MUTED);
        unsigned last=bk_restoring?1:2;
        for(unsigned i=0;i<=last;i++){
            const char *label=i==last?(bk_restoring?"UNLOCK BACKUP":"CREATE ENCRYPTED BACKUP"):i?"CONFIRM PASSPHRASE":"PASSPHRASE";
            size_t n=strlen(i?bk_repeat:bk_password);snprintf(text,sizeof(text),"%u characters entered (hidden)",(unsigned)n);
            bk_row(i,bk_field,180+i*55,label,i==last?"Enter to continue":text);
        }
    }else if(bk_mode==BK_LIST){
        draw_rich_text("Choose a backup. Keys stay encrypted until you enter its passphrase.",20,86,13,logical_h_res-40,22,2,UI_MUTED);
        if(!bk_count)draw_rich_text("No .mmk backups found in SD / meshmatsu / backups. Create a backup first, or copy one into that folder.",20,150,15,logical_h_res-40,26,4,UI_GREEN);
        size_t first=bk_selection>=5?bk_selection-4:0;
        for(size_t i=first;i<bk_count&&i<first+5;i++)bk_row(i,bk_selection,140+(i-first)*47,bk_files[i],NULL);
    }else if(bk_mode==BK_SELECT){
        char stamp[64],clock[32],date[32];
        if(mesh_time_clock(&time_settings,bk_saved.timestamp,clock,sizeof(clock),date,sizeof(date)))snprintf(stamp,sizeof(stamp),"Saved: %.20s %.20s",date,clock);
        else snprintf(stamp,sizeof(stamp),"Save time unavailable");
        snprintf(text,sizeof(text),"Backup: %s / !%08lx",bk_saved.name[0]?bk_saved.name:"unnamed radio",(unsigned long)bk_saved.node);
        draw_rich_text(text,20,86,13,logical_h_res-40,22,2,UI_GREEN);
        pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,118,stamp);
        unsigned count=bk_choices(),first=bk_selection>=5?bk_selection-4:0;
        for(unsigned i=first;i<count&&i<first+5;i++){
            int target=bk_choice(i);if(target<0)snprintf(text,sizeof(text),"RESTORE IDENTITY KEY PAIR");
            else snprintf(text,sizeof(text),"RESTORE SLOT %d KEY: %s",target,bk_saved.channel[target].name[0]?bk_saved.channel[target].name:"preset channel");
            bk_row(i,bk_selection,140+(i-first)*47,text,target<0?"Changes the radio's direct-message identity":"Replaces only this slot's key; role/name/settings stay");
        }
    }else if(bk_mode==BK_REVIEW){
        if(bk_target<0){char old[17]="unavailable",next[17]="unavailable";
            mesh_keys_fingerprint(bk_original.public_key,old);mesh_keys_fingerprint(bk_saved.public_key,next);
            snprintf(text,sizeof(text),"Restore identity: %s / !%08lx",bk_saved.name[0]?bk_saved.name:"unnamed radio",(unsigned long)bk_saved.node);
            draw_rich_text(text,20,88,14,logical_h_res-40,25,2,UI_GREEN);
            snprintf(text,sizeof(text),"Current fingerprint: %s\nBackup fingerprint:  %s",old,next);draw_rich_text(text,20,150,13,logical_h_res-40,26,3,WHITE);
            draw_rich_text(bk_saved.node!=bk_original.node?"This backup is from another radio. Use it for recovery; keep the original device off while using the same identity.":"Replace the current public/private key pair with this backup? Other nodes may need to refresh your identity.",20,230,13,logical_h_res-40,24,3,KEY_TRIANGLE_COLOR);
        }else{
            snprintf(text,sizeof(text),"Restore slot %d: %s",bk_target,bk_original.channel[bk_target].name);draw_rich_text(text,20,90,16,logical_h_res-40,26,2,UI_GREEN);
            snprintf(text,sizeof(text),"Key from backup channel: %s",bk_saved.channel[bk_target].name);draw_rich_text(text,20,150,13,logical_h_res-40,24,2,WHITE);
            draw_rich_text("Replaces this slot's encryption key only. Other radios must use the same key. The slot's name, role, radio settings and location privacy remain as they are.",20,210,13,logical_h_res-40,24,4,KEY_TRIANGLE_COLOR);
        }
        draw_rich_text("First saves an encrypted safety backup using the same passphrase, then verifies fresh radio readback.",20,310,11,logical_h_res-40,20,2,UI_MUTED);
        for(unsigned i=0;i<2;i++){int width=((int)logical_h_res-52)/2,x=20+i*(width+12);bool selected=bk_confirm==(i!=0);pax_draw_rect(&fb,selected?UI_GREEN:UI_PANEL,x,360,width,36);pax_draw_text(&fb,selected?BLACK:UI_GREEN,pax_font_sky_mono,13,x+12,371,i?"CONFIRM RESTORE":"CANCEL");}
    }else{
        const char *message=bk_mode==BK_DONE?bk_feedback:bk_mode==BK_SAFETY?"Saving and verifying the encrypted safety backup. Radio keys have not changed yet...":bk_mode==BK_VERIFY?"Change sent once. Waiting for fresh radio confirmation...":bk_mode==BK_CAPTURE_SAVE||bk_mode==BK_CAPTURE_REVIEW||bk_mode==BK_BEFORE_APPLY?"Reading a fresh key snapshot from the local radio...":"Working on encrypted SD files. This may take a little while; messaging continues.";
        draw_rich_text(message,20,110,16,logical_h_res-40,28,7,UI_GREEN);
        if(bk_mode==BK_DONE)pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,12,20,350,"Enter: return to Backup & Restore");
    }
    if(bk_feedback[0]&&bk_mode!=BK_DONE)draw_rich_text(bk_feedback,20,376,10,logical_h_res-40,18,2,KEY_TRIANGLE_COLOR);
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,logical_v_res-UI_HINT_OFFSET,bk_mode==BK_REVIEW?"Left/Right: Cancel or Confirm  Enter: choose  Esc: cancel":"Up/Down: choose  Tab: field  Enter: continue  Esc: back");
}
