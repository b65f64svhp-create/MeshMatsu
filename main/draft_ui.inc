// App-task draft tracking; flash writes use an immutable background snapshot.
static uint32_t draft_hashes[MESH_DRAFT_LIMIT],draft_revision,draft_saved_revision,draft_write_revision;
static int64_t draft_due_us,draft_dirty_since_us;
static bool draft_load_failed,draft_save_failed,draft_recovered,draft_exit_discard;
static uint32_t draft_fingerprint(size_t slot){
 uint32_t hash=2166136261u;
 const uint8_t *reply=(const uint8_t*)&draft_reply_ids[slot];
 for(unsigned i=0;i<4;i++)hash=(hash^reply[i])*16777619u;
 for(const unsigned char *p=(const unsigned char*)drafts[slot].text;*p;p++)hash=(hash^*p)*16777619u;
 return hash;
}
static void draft_ui_touch(void){
 uint32_t hash=draft_fingerprint(draft_slot);if(hash==draft_hashes[draft_slot])return;
 draft_hashes[draft_slot]=hash;
 if(draft_revision==draft_saved_revision)draft_dirty_since_us=esp_timer_get_time();
 draft_revision++;draft_due_us=esp_timer_get_time()+1000000;
 if(!drafts[draft_slot].text[0])draft_due_us=esp_timer_get_time();
 draft_exit_discard=false;
}
static void draft_ui_load(void){
 size_t count=0;bool recovered=false;
 if(!mesh_draft_store_load(drafts,draft_reply_ids,MESH_DRAFT_LIMIT,&count,&recovered))draft_load_failed=true;
 else {draft_count=count?count:1;draft_recovered=recovered;}
 for(size_t i=0;i<draft_count;i++)draft_hashes[i]=draft_fingerprint(i);
}
static bool draft_ui_present(uint32_t peer,uint8_t channel){
 if(peer)channel=0;
 for(size_t i=0;i<draft_count;i++)if(drafts[i].peer==peer&&drafts[i].channel==channel)return drafts[i].text[0]||draft_reply_ids[i];
 return false;
}
static uint32_t direct_ui_peers[MESH_MESSAGE_HISTORY_MAX+MESH_DRAFT_LIMIT];
static size_t direct_ui_total;
static uint32_t direct_ui_history_revision,direct_ui_draft_revision;
static bool direct_ui_cached;
static void direct_ui_add(uint32_t peer){
 if(!peer)return;
 for(size_t i=0;i<direct_ui_total;i++)if(direct_ui_peers[i]==peer)return;
 if(direct_ui_total<sizeof(direct_ui_peers)/sizeof(*direct_ui_peers))direct_ui_peers[direct_ui_total++]=peer;
}
static void direct_ui_refresh(void){
 uint32_t revision=mesh_radio_history_revision();
 if(direct_ui_cached&&direct_ui_history_revision==revision&&direct_ui_draft_revision==draft_revision)return;
 direct_ui_total=0;
 for(size_t i=mesh_radio_message_count();i>0;i--){mesh_message_t message;if(mesh_radio_get_message(i-1,&message))direct_ui_add(message.peer_node);}
 for(size_t i=0;i<draft_count;i++)if(drafts[i].text[0]||draft_reply_ids[i])direct_ui_add(drafts[i].peer);
 direct_ui_history_revision=revision;direct_ui_draft_revision=draft_revision;direct_ui_cached=true;
}
static bool direct_ui_peer(size_t index,uint32_t *peer){
 direct_ui_refresh();if(index>=direct_ui_total)return false;if(peer)*peer=direct_ui_peers[index];return true;
}
static size_t direct_ui_count(void){direct_ui_refresh();return direct_ui_total;}
// 1=saved, 0=waiting, -1=failed. A newer edit never becomes saved by an old job.
static int draft_ui_flush(void){
 if(mesh_draft_store_busy())return 0;
 if(draft_revision==draft_saved_revision)return 1;
 int64_t now=esp_timer_get_time();
 if(draft_load_failed||(draft_save_failed&&now<draft_due_us))return -1;
 if(!mesh_draft_store_start(drafts,draft_reply_ids,draft_count)){draft_save_failed=true;draft_due_us=now+5000000;return -1;}
 draft_write_revision=draft_revision;draft_dirty_since_us=now;return 0;
}
static bool draft_ui_poll(int64_t now,bool input_busy){
 bool changed=false,ok=false;
 if(mesh_draft_store_poll(&ok)){
  changed=true;draft_save_failed=!ok;
  if(ok){draft_saved_revision=draft_write_revision;draft_recovered=false;}
  else draft_due_us=now+5000000;
 }
 if(!draft_load_failed&&draft_revision!=draft_saved_revision&&!mesh_draft_store_busy()&&
    (now>=draft_due_us||(!draft_save_failed&&now-draft_dirty_since_us>=5000000))&&
    (!input_busy||now-draft_dirty_since_us>=5000000))draft_ui_flush();
 return changed;
}
