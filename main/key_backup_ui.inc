// Passphrases and decrypted backups live only while this screen is active.
enum {BK_MENU,BK_PASS,BK_LIST,BK_SELECT,BK_REVIEW,BK_CAPTURE_SAVE,BK_CAPTURE_REVIEW,BK_SAFETY,BK_BEFORE_APPLY,BK_WORK,BK_VERIFY,BK_DONE};
static unsigned bk_mode,bk_selection,bk_field,bk_action;
static uint32_t bk_generation,bk_job_generation;
static bool bk_active,bk_confirm,bk_restoring,bk_identity,bk_channels,bk_verify_identity;
static int bk_target=-1;
static char bk_password[MESH_KEY_PASSWORD_MAX+1],bk_repeat[MESH_KEY_PASSWORD_MAX+1],bk_feedback[180],bk_file[64],bk_safety_file[64];
static char bk_files[MESH_KEY_FILE_LIMIT][64];static size_t bk_count;
static mesh_key_backup_t bk_saved,bk_original;
static mesh_key_result_t bk_result;
static void bk_mode_set(unsigned mode){bk_mode=mode;ui_dirty=true;}
static void bk_wipe(void) {
    mesh_keys_wipe(bk_password,sizeof(bk_password));mesh_keys_wipe(bk_repeat,sizeof(bk_repeat));
    mesh_keys_wipe(&bk_saved,sizeof(bk_saved));mesh_keys_wipe(&bk_original,sizeof(bk_original));mesh_radio_keys_clear();
}
static void bk_close(void){bk_generation++;bk_wipe();bk_active=false;bk_mode=BK_MENU;bk_confirm=false;}
static void open_key_backup(void) {
    bk_close();bk_active=true;bk_selection=bk_field=0;bk_count=0;bk_feedback[0]=bk_file[0]=bk_safety_file[0]=0;
    current_screen=SCREEN_KEY_BACKUP;mesh_radio_request_owner();
}
static void bk_done(const char *message){bk_generation++;bk_wipe();snprintf(bk_feedback,sizeof(bk_feedback),"%s",message);bk_mode_set(BK_DONE);}
static unsigned bk_choices(void){unsigned n=bk_saved.identity;for(unsigned i=0;i<8;i++)if(bk_saved.channel_mask&(1u<<i))n++;return n;}
static int bk_choice(unsigned row) {
    if(bk_saved.identity){if(!row)return -1;row--;}
    for(unsigned i=0;i<8;i++)if(bk_saved.channel_mask&(1u<<i)){if(!row)return (int)i;row--;}
    return -2;
}
static bool bk_store_start(mesh_key_job_t job,const mesh_key_backup_t *backup,const char *password,const char *path,bool safety) {
    if(!mesh_key_store_start(job,backup,password,path,safety))return false;
    bk_job_generation=bk_generation;return true;
}
static bool bk_read(unsigned mode) {
    if(!mesh_radio_keys_request()){mesh_radio_request_owner();snprintf(bk_feedback,sizeof(bk_feedback),"Wait for radio permissions and any pending changes, then retry.");return false;}
    bk_mode_set(mode);return true;
}
static void bk_password_submit(void) {
    size_t length=strlen(bk_password);
    if(length<12){snprintf(bk_feedback,sizeof(bk_feedback),"Use at least 12 characters. Spaces are allowed.");return;}
    if(!bk_restoring&&strcmp(bk_password,bk_repeat)){snprintf(bk_feedback,sizeof(bk_feedback),"Passphrases do not match.");return;}
    bk_feedback[0]=0;
    if(bk_restoring){if(bk_store_start(MESH_KEY_JOB_OPEN,NULL,bk_password,bk_file,false))bk_mode_set(BK_WORK);
        else snprintf(bk_feedback,sizeof(bk_feedback),"Could not start. Wait for the current SD operation.");}
    else bk_read(BK_CAPTURE_SAVE);
}
static bool bk_same_target(const mesh_key_backup_t *now) {
    if(now->node!=bk_original.node)return false;
    if(bk_target<0)return now->identity&&bk_original.identity&&!memcmp(now->private_key,bk_original.private_key,32)&&!memcmp(now->public_key,bk_original.public_key,32);
    unsigned bit=1u<<bk_target;
    const mesh_backup_channel_t *a=&now->channel[bk_target],*b=&bk_original.channel[bk_target];
    return (now->channel_mask&bit)&&(bk_original.channel_mask&bit)&&a->role==b->role&&a->length==b->length&&!strcmp(a->name,b->name)&&!memcmp(a->key,b->key,a->length);
}
static void bk_apply(void) {
    mesh_key_backup_t fresh={0};
    if(!mesh_radio_keys_snapshot(&fresh,true,true)||!bk_same_target(&fresh)){mesh_keys_wipe(&fresh,sizeof(fresh));bk_done("Radio or selected keys changed during review. Nothing restored. Reopen and review again.");return;}
    bool ok=false;bk_verify_identity=bk_target<0;
    if(bk_target<0)ok=mesh_radio_keys_restore_identity(bk_saved.public_key,bk_saved.private_key,mesh_radio_keys_revision());
    else{mesh_channel_t channel;if(mesh_radio_get_channel(bk_target,&channel))ok=mesh_radio_keys_restore_channel(bk_target,&bk_saved.channel[bk_target],channel.revision);}
    mesh_keys_wipe(&fresh,sizeof(fresh));bk_wipe();
    if(ok)bk_mode_set(BK_VERIFY);
    else bk_done("Not sent. Radio busy, changed or privacy rules rejected the channel key. Reopen to review.");
}
static void bk_tick(void) {
    bool visible=current_screen==SCREEN_KEY_BACKUP&&!display_and_lights_off;
    if(!visible&&bk_active)bk_close();
    if(visible&&!bk_active)open_key_backup();
    if(bk_active&&esp_timer_get_time()-last_activity_us>=120000000&&bk_mode!=BK_MENU&&bk_mode!=BK_DONE)bk_done("Key screen locked after two minutes without input. Reopen the backup to continue.");
    if(mesh_key_store_take(&bk_result)){
        if(bk_active&&bk_job_generation==bk_generation){
            if(!bk_result.ok)bk_done(bk_result.job==MESH_KEY_JOB_OPEN?"Could not unlock backup: incorrect passphrase, unsupported file or damaged file. Nothing changed.":"SD operation failed. Check the card and free space. No radio keys changed.");
            else if(bk_result.job==MESH_KEY_JOB_LIST){bk_count=bk_result.count;memcpy(bk_files,bk_result.files,sizeof(bk_files));bk_selection=0;bk_mode_set(BK_LIST);}
            else if(bk_result.job==MESH_KEY_JOB_OPEN){bk_saved=bk_result.backup;bk_selection=0;bk_mode_set(BK_SELECT);}
            else if(bk_mode==BK_SAFETY){snprintf(bk_safety_file,sizeof(bk_safety_file),"%.63s",bk_result.path);if(!bk_read(BK_BEFORE_APPLY))bk_done("Safety backup saved, but radio unavailable. Nothing restored. Reopen to retry.");}
            else{char message[180];snprintf(message,sizeof(message),"Backup saved and verified: %.63s. Copy it from meshmatsu/backups on the SD card to your Mac.",bk_result.path);bk_done(message);}
        }
        mesh_keys_wipe(&bk_result,sizeof(bk_result));
    }
    if(!bk_active)return;
    if(bk_mode==BK_CAPTURE_SAVE||bk_mode==BK_CAPTURE_REVIEW||bk_mode==BK_BEFORE_APPLY){
        mesh_radio_keys_state_t state=mesh_radio_keys_state();
        if(state==MESH_KEYS_READY){
            if(bk_mode==BK_BEFORE_APPLY){bk_apply();return;}
            if(bk_mode==BK_CAPTURE_REVIEW){
                if(!mesh_radio_keys_snapshot(&bk_original,true,true)){bk_done("Current radio keys unavailable. Nothing restored.");return;}
                if(bk_target>=0&&!(bk_original.channel_mask&(1u<<bk_target))){bk_done("Destination slot is disabled. Enable or import that channel in Channels first, then retry restoring its key.");return;}
                bk_confirm=false;bk_mode_set(BK_REVIEW);
            }else{
                mesh_key_backup_t backup={0};
                if(!mesh_radio_keys_snapshot(&backup,bk_identity,bk_channels)){bk_done("Requested keys are unavailable on this radio. Nothing saved.");return;}
                bool ok=bk_store_start(MESH_KEY_JOB_SAVE,&backup,bk_password,NULL,false);
                mesh_keys_wipe(&backup,sizeof(backup));bk_wipe();
                if(ok)bk_mode_set(BK_WORK);else bk_done("Could not start the SD backup. Nothing saved.");
            }
        }else if(state!=MESH_KEYS_READING)bk_done("Radio did not provide a complete fresh key snapshot. Nothing changed. Reopen to retry.");
    }else if(bk_mode==BK_VERIFY){
        if(bk_verify_identity){mesh_radio_keys_state_t state=mesh_radio_keys_state();
            if(state!=MESH_KEYS_SAVING)bk_done(state==MESH_KEYS_SAVED?"Identity keys restored and verified by the radio. The safety backup is also on your SD card.":"Restore not confirmed by the radio. Check its current identity before trying again. A safety backup is on the SD card.");
        }else{mesh_channel_operation_t op=mesh_radio_channel_operation();
            if(op.state!=MESH_FAVORITE_SAVING)bk_done(op.state==MESH_FAVORITE_SAVED?"Channel key restored and verified by the radio. Other radios need the matching channel key.":"Channel key restore not confirmed. Check current channel settings. A safety backup is on the SD card.");}
    }
}
static bool key_backup_navigation(int key) {
    if(current_screen!=SCREEN_KEY_BACKUP)return false;
    if(key==BSP_INPUT_NAVIGATION_KEY_F1||key==BSP_INPUT_NAVIGATION_KEY_F2||key==BSP_INPUT_NAVIGATION_KEY_F3||key==BSP_INPUT_NAVIGATION_KEY_F4||key==BSP_INPUT_NAVIGATION_KEY_F5||key==BSP_INPUT_NAVIGATION_KEY_F6)return false;
    if(key==BSP_INPUT_NAVIGATION_KEY_ESC){if(bk_mode==BK_MENU)return false;bk_close();open_key_backup();return true;}
    bool enter=key==BSP_INPUT_NAVIGATION_KEY_RETURN;
    bool up=key==BSP_INPUT_NAVIGATION_KEY_UP,down=key==BSP_INPUT_NAVIGATION_KEY_DOWN;
    if(bk_mode==BK_MENU){
        if(up&&bk_selection)bk_selection--;else if(down&&bk_selection<3)bk_selection++;
        else if(enter||key==BSP_INPUT_NAVIGATION_KEY_RIGHT){
            bk_action=bk_selection;bk_restoring=bk_action==3;bk_feedback[0]=0;bk_field=0;
            if(bk_restoring){if(bk_store_start(MESH_KEY_JOB_LIST,NULL,NULL,NULL,false))bk_mode_set(BK_WORK);else snprintf(bk_feedback,sizeof(bk_feedback),"Wait for the current SD operation.");}
            else{bk_identity=bk_action!=1;bk_channels=bk_action!=0;bk_mode_set(BK_PASS);}
        }
    }else if(bk_mode==BK_LIST){
        if(up&&bk_selection)bk_selection--;else if(down&&bk_selection+1<bk_count)bk_selection++;
        else if(enter&&bk_count){snprintf(bk_file,sizeof(bk_file),"%s",bk_files[bk_selection]);bk_restoring=true;bk_field=0;bk_mode_set(BK_PASS);}
    }else if(bk_mode==BK_PASS){
        unsigned last=bk_restoring?1:2;
        if(up&&bk_field)bk_field--;else if(down&&bk_field<last)bk_field++;
        else if(enter){if(bk_field<last)bk_field++;else bk_password_submit();}
    }else if(bk_mode==BK_SELECT){
        if(up&&bk_selection)bk_selection--;else if(down&&bk_selection+1<bk_choices())bk_selection++;
        else if(enter){bk_target=bk_choice(bk_selection);if(bk_target>=-1)bk_read(BK_CAPTURE_REVIEW);}
    }else if(bk_mode==BK_REVIEW){
        if(key==BSP_INPUT_NAVIGATION_KEY_LEFT)bk_confirm=false;
        else if(key==BSP_INPUT_NAVIGATION_KEY_RIGHT)bk_confirm=true;
        else if(enter){
            if(!bk_confirm){bk_wipe();open_key_backup();return true;}
            mesh_key_backup_t safety=bk_original;
            if(bk_target<0){safety.channel_mask=0;memset(safety.channel,0,sizeof(safety.channel));}
            else{safety.identity=false;mesh_keys_wipe(safety.private_key,32);memset(safety.public_key,0,32);safety.channel_mask=1u<<bk_target;for(unsigned i=0;i<8;i++)if((int)i!=bk_target)mesh_keys_wipe(&safety.channel[i],sizeof(safety.channel[i]));}
            bool ok=bk_store_start(MESH_KEY_JOB_SAVE,&safety,bk_password,NULL,true);mesh_keys_wipe(&safety,sizeof(safety));
            if(ok)bk_mode_set(BK_SAFETY);else bk_done("Could not start the safety backup. Nothing restored.");
        }
    }else if(bk_mode==BK_DONE&&enter)open_key_backup();
    return true;
}
static bool key_backup_key(char key) {
    if(current_screen!=SCREEN_KEY_BACKUP)return false;
    if(key=='\r'||key=='\n')return key_backup_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
    if(bk_mode==BK_PASS){unsigned last=bk_restoring?1:2;
        if(key=='\t')bk_field=(bk_field+1)%(last+1);
        else if(bk_field<last){char *p=bk_field?bk_repeat:bk_password;size_t n=strlen(p);
            if((key=='\b'||key==127)&&n)p[n-1]=0;
            else if(key>=32&&key<=126&&n<MESH_KEY_PASSWORD_MAX){p[n]=key;p[n+1]=0;}
            bk_feedback[0]=0;}
    }
    return true;
}
