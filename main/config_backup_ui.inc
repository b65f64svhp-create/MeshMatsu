// Authenticated SD archives. Main task owns all radio requests and mutations.
enum {
    CF_MENU,CF_PASS,CF_LIST,CF_SELECT,CF_READ_SAVE,CF_READ_REVIEW,CF_REVIEW,CF_SAFETY,CF_BEFORE_APPLY,CF_WORK,CF_VERIFY,CF_DONE
}
;
static unsigned cf_mode,cf_selection,cf_field,cf_target,cf_generation,cf_job_generation;
static bool cf_active,cf_restoring,cf_confirm;
static char cf_password[97],cf_repeat[97],cf_file[64],cf_feedback[200];
static size_t cf_count;
static char cf_files[32][64];
static EXT_RAM_BSS_ATTR mesh_config_backup_t cf_saved,cf_original;
static EXT_RAM_BSS_ATTR mesh_cfg_result_t cf_result;
static void cf_wipe(void){
    mesh_keys_wipe(cf_password,sizeof(cf_password));
    mesh_keys_wipe(cf_repeat,sizeof(cf_repeat));
    mesh_keys_wipe(&cf_saved,sizeof(cf_saved));
    mesh_keys_wipe(&cf_original,sizeof(cf_original));
    mesh_radio_config_clear();
}
static void cf_close(void){
    cf_generation++;
    cf_wipe();
    cf_active=false;
    cf_mode=CF_MENU;
    cf_confirm=false;
}
static void open_config_backup(void){
    cf_close();
    cf_active=true;
    cf_selection=cf_field=0;
    cf_count=0;
    cf_feedback[0]=0;
    current_screen=SCREEN_CONFIG_BACKUP;
    mesh_radio_config_identity_validator(mesh_keys_identity_valid);
    mesh_radio_request_owner();
}
static void cf_done(const char *text){
    cf_generation++;
    cf_wipe();
    snprintf(cf_feedback,sizeof(cf_feedback),"%s",text);
    cf_mode=CF_DONE;
    ui_dirty=true;
}
static bool cf_job(mesh_cfg_job_t job,const mesh_config_backup_t *archive,const char *path,bool safety){
    if(!mesh_config_store_start(job,archive,job==CFG_JOB_LIST?NULL:cf_password,path,safety))return false;
    cf_job_generation=cf_generation;
    return true;
}
static bool cf_read(unsigned mode){
    if(!mesh_radio_config_capture()){
        mesh_radio_request_owner();
        snprintf(cf_feedback,sizeof(cf_feedback),"Wait for radio sync, permissions and any pending change, then retry.");
        return false;
    }
    cf_mode=mode;
    ui_dirty=true;
    return true;
}
static bool cf_snapshot(mesh_config_backup_t *out){
    if(!mesh_radio_config_snapshot(out))return false;
    static uint8_t bytes[MESH_CFG_DATA];
    size_t n=sizeof(bytes);
    bool ok=mesh_config_app_capture(bytes,&n)&&mesh_config_add(out,CFG_APP,0,bytes,n);
    n=mesh_radio_nicknames_export(bytes,sizeof(bytes));
    ok=ok&&n&&mesh_config_add(out,CFG_ALIASES,0,bytes,n);
    n=mesh_radio_mutes_export(bytes,sizeof(bytes));
    ok=ok&&n&&mesh_config_add(out,CFG_MUTES,0,bytes,n);
    uint32_t capacity=mesh_radio_history_limit();
    uint8_t b[]={
        capacity,capacity>>8,capacity>>16,capacity>>24
    }
    ;
    ok=ok&&mesh_config_add(out,CFG_HISTORY,0,b,4);
    mesh_keys_wipe(bytes,sizeof(bytes));
    return ok&&out->count==MESH_CFG_RECORDS;
}
static bool cf_same(const mesh_config_backup_t *a,const mesh_config_backup_t *b){
    if(a->node!=b->node||a->hardware!=b->hardware||strcmp(a->firmware,b->firmware)||a->count!=b->count)return false;
    for(unsigned i=0;i<a->count;i++){
        const mesh_cfg_record_t *r=&a->records[i],*other=mesh_config_find(b,r->kind,r->id);
        if(!other||r->size!=other->size||memcmp(r->data,other->data,r->size))return false;
    }
    return true;
}
static void cf_apply(void){
    // A second fresh dump and local snapshot must still match the safety backup.
    if(!cf_snapshot(&cf_result.archive)||!cf_same(&cf_original,&cf_result.archive)){
        mesh_keys_wipe(&cf_result.archive,sizeof(cf_result.archive));
        cf_done("Settings changed during review. Nothing restored. Reopen and review again.");
        return;
    }
    mesh_keys_wipe(&cf_result.archive,sizeof(cf_result.archive));
    const mesh_cfg_record_t *r=&cf_saved.records[cf_target];
    bool ok=false;
    if(r->kind<=CFG_CHANNEL||r->kind==CFG_FIXED){
        ok=mesh_radio_config_restore(&cf_saved,r);
        if(ok){
            cf_wipe();
            cf_mode=CF_VERIFY;
            return;
        }
    }
    else if(r->kind==CFG_APP)ok=mesh_config_app_apply(r->data,r->size);
    else if(r->kind==CFG_ALIASES)ok=mesh_radio_nicknames_import(r->data,r->size);
    else if(r->kind==CFG_MUTES)ok=mesh_radio_mutes_import(r->data,r->size);
    else if(r->kind==CFG_HISTORY&&r->size==4){
        uint32_t n=(uint32_t)r->data[0]|(uint32_t)r->data[1]<<8|(uint32_t)r->data[2]<<16|(uint32_t)r->data[3]<<24;
        ok=mesh_radio_set_history_limit(n);
    }
    bool app=r->kind==CFG_APP;
    cf_done(ok?(app?"App preferences saved and read back. Restart MeshMatsu to apply them. Encrypted safety backup is on SD.":"Selected local section restored and verified. Encrypted safety backup is on SD."):
    app?"App preference restore did not complete. Some values may have changed. Use the safety backup to recover, then restart MeshMatsu.":"Not restored: section unsupported, radio busy, or channel/privacy rules refused it. A safety backup is on SD.");
}
static void cf_submit(void){
    if(strlen(cf_password)<12){
        snprintf(cf_feedback,sizeof(cf_feedback),"Use at least 12 characters. Spaces are allowed.");
        return;
    }
    if(!cf_restoring&&strcmp(cf_password,cf_repeat)){
        snprintf(cf_feedback,sizeof(cf_feedback),"Passphrases do not match.");
        return;
    }
    cf_feedback[0]=0;
    if(cf_restoring){
        if(cf_job(CFG_JOB_OPEN,NULL,cf_file,false))cf_mode=CF_WORK;
        else snprintf(cf_feedback,sizeof(cf_feedback),"Wait for the current SD operation.");
    }
    else cf_read(CF_READ_SAVE);
}
static void cf_tick(void){
    bool visible=current_screen==SCREEN_CONFIG_BACKUP&&!display_and_lights_off;
    if(!visible&&cf_active)cf_close();
    if(visible&&!cf_active)open_config_backup();
    if(cf_active&&esp_timer_get_time()-last_activity_us>=120000000&&cf_mode!=CF_MENU&&cf_mode!=CF_DONE)cf_done("Backup screen locked after two minutes without input. Reopen to continue.");
    if(mesh_config_store_take(&cf_result)){
        if(cf_active&&cf_job_generation==cf_generation){
            if(!cf_result.ok)cf_done(cf_result.job==CFG_JOB_OPEN?"Cannot unlock: wrong passphrase, unsupported format or damaged file. Nothing restored.":"SD backup operation failed. Check the card and free space. Nothing restored.");
            else if(cf_result.job==CFG_JOB_LIST){
                cf_count=cf_result.count;
                memcpy(cf_files,cf_result.files,sizeof(cf_files));
                cf_selection=0;
                cf_mode=CF_LIST;
            }
            else if(cf_result.job==CFG_JOB_OPEN){
                cf_saved=cf_result.archive;
                cf_selection=0;
                cf_mode=CF_SELECT;
            }
            else if(cf_mode==CF_SAFETY){
                if(!cf_read(CF_BEFORE_APPLY))cf_done("Safety backup saved. Radio unavailable; nothing restored. Reopen to retry.");
            }
            else{
                char text[200];
                snprintf(text,sizeof(text),"Full configuration saved and verified: %.63s. Copy it from SD / meshmatsu / backups to your Mac.",cf_result.path);
                cf_done(text);
            }
            ui_dirty=true;
        }
        mesh_keys_wipe(&cf_result,sizeof(cf_result));
    }
    if(!cf_active)return;
    if(cf_mode==CF_READ_SAVE||cf_mode==CF_READ_REVIEW||cf_mode==CF_BEFORE_APPLY){
        mesh_cfg_state_t state=mesh_radio_config_state();
        if(state==CFG_READY){
            if(cf_mode==CF_BEFORE_APPLY){
                cf_apply();
                return;
            }
            if(!cf_snapshot(&cf_original)){
                cf_done("A complete fresh radio and app snapshot was unavailable. Nothing saved or restored.");
                return;
            }
            if(cf_mode==CF_READ_REVIEW){
                cf_confirm=false;
                cf_mode=CF_REVIEW;
            }
            else{
                bool ok=cf_job(CFG_JOB_SAVE,&cf_original,NULL,false);
                cf_wipe();
                if(ok)cf_mode=CF_WORK;
                else cf_done("Could not start the encrypted SD backup.");
            }
            ui_dirty=true;
        }
        else if(state!=CFG_READING)cf_done("Radio did not supply every expected configuration section. Nothing saved or restored. Retry after sync.");
    }
    else if(cf_mode==CF_VERIFY){
        mesh_cfg_state_t state=mesh_radio_config_state();
        if(state!=CFG_SAVING)cf_done(state==CFG_SAVED?"Selected radio section restored and verified by a complete fresh radio dump. Safety backup is on SD.":"Radio restore is unconfirmed. The radio may have changed. Check its settings; use the safety backup to recover. No automatic resend.");
    }
}
static bool config_backup_navigation(int key){
    if(current_screen!=SCREEN_CONFIG_BACKUP)return false;
    if(key==BSP_INPUT_NAVIGATION_KEY_F1||key==BSP_INPUT_NAVIGATION_KEY_F2||key==BSP_INPUT_NAVIGATION_KEY_F3||key==BSP_INPUT_NAVIGATION_KEY_F4||key==BSP_INPUT_NAVIGATION_KEY_F5||key==BSP_INPUT_NAVIGATION_KEY_F6)return false;
    if(key==BSP_INPUT_NAVIGATION_KEY_ESC){
        if(cf_mode==CF_MENU)return false;
        open_config_backup();
        return true;
    }
    bool enter=key==BSP_INPUT_NAVIGATION_KEY_RETURN,up=key==BSP_INPUT_NAVIGATION_KEY_UP,down=key==BSP_INPUT_NAVIGATION_KEY_DOWN;
    if(cf_mode==CF_MENU){
        if(up&&cf_selection)cf_selection--;
        else if(down&&cf_selection<2)cf_selection++;
        else if(enter||key==BSP_INPUT_NAVIGATION_KEY_RIGHT){
            if(cf_selection==2){
                cf_close();
                open_key_backup();
                return true;
            }
            cf_restoring=cf_selection==1;
            cf_field=0;
            cf_feedback[0]=0;
            if(cf_restoring){
                if(cf_job(CFG_JOB_LIST,NULL,NULL,false))cf_mode=CF_WORK;
                else snprintf(cf_feedback,sizeof(cf_feedback),"Wait for the current SD operation.");
            }
            else cf_mode=CF_PASS;
        }
    }
    else if(cf_mode==CF_LIST){
        if(up&&cf_selection)cf_selection--;
        else if(down&&cf_selection+1<cf_count)cf_selection++;
        else if(enter&&cf_count){
            snprintf(cf_file,64,"%s",cf_files[cf_selection]);
            cf_restoring=true;
            cf_field=0;
            cf_mode=CF_PASS;
        }
    }
    else if(cf_mode==CF_PASS){
        unsigned last=cf_restoring?1:2;
        if(up&&cf_field)cf_field--;
        else if(down&&cf_field<last)cf_field++;
        else if(enter){
            if(cf_field<last)cf_field++;
            else cf_submit();
        }
    }
    else if(cf_mode==CF_SELECT){
        if(up&&cf_selection)cf_selection--;
        else if(down&&cf_selection+1<cf_saved.count)cf_selection++;
        else if(enter&&cf_saved.count){
            cf_target=cf_selection;
            cf_read(CF_READ_REVIEW);
        }
    }
    else if(cf_mode==CF_REVIEW){
        if(key==BSP_INPUT_NAVIGATION_KEY_LEFT)cf_confirm=false;
        else if(key==BSP_INPUT_NAVIGATION_KEY_RIGHT)cf_confirm=true;
        else if(enter){
            if(!cf_confirm){
                open_config_backup();
                return true;
            }
            const mesh_cfg_record_t *r=&cf_saved.records[cf_target];
            if(r->kind==CFG_FIXED&&(r->size!=9||r->data[0]!=1)){
                snprintf(cf_feedback,sizeof(cf_feedback),"No fixed coordinates were saved. Live GPS fixes are separate from configuration.");
                return true;
            }
            if((r->kind<=CFG_CHANNEL||r->kind==CFG_FIXED)&&(cf_saved.node!=cf_original.node||cf_saved.hardware!=cf_original.hardware||strcmp(cf_saved.firmware,cf_original.firmware)||r->size>MESH_CFG_RADIO_MAX)){
                snprintf(cf_feedback,sizeof(cf_feedback),"Radio restore needs the same node, hardware and firmware, and a section <=211 bytes. Local sections can transfer between devices.");
                return true;
            }
            if(cf_job(CFG_JOB_SAVE,&cf_original,NULL,true))cf_mode=CF_SAFETY;
            else cf_done("Could not start the safety backup. Nothing restored.");
        }
    }
    else if(cf_mode==CF_DONE&&enter)open_config_backup();
    return true;
}
static bool config_backup_key(char key){
    if(current_screen!=SCREEN_CONFIG_BACKUP)return false;
    if(key=='\r'||key=='\n')return config_backup_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
    if(cf_mode==CF_PASS){
        unsigned last=cf_restoring?1:2;
        if(key=='\t')cf_field=(cf_field+1)%(last+1);
        else if(cf_field<last){
            char *p=cf_field?cf_repeat:cf_password;
            size_t n=strlen(p);
            if((key=='\b'||key==127)&&n)p[n-1]=0;
            else if(key>=32&&key<=126&&n<96){
                p[n]=key;
                p[n+1]=0;
            }
            cf_feedback[0]=0;
        }
    }
    return true;
}
