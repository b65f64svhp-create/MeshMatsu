#define main original_radio_tests
#include "test_direct_messages.c"
#undef main
#define MESH_DRAFT_PATH "ui-drafts.bin"
#include "draft_store.c"
static mesh_draft_t drafts[MESH_DRAFT_LIMIT];
static uint32_t draft_reply_ids[MESH_DRAFT_LIMIT];
static size_t draft_count=1,draft_slot;
#include "../main/draft_ui.inc"
int main(void){
 reset();draft_ui_load();assert(!draft_load_failed&&draft_count==1);
 strcpy(drafts[0].text,"first edit");draft_ui_touch();assert(draft_revision!=draft_saved_revision);
 int64_t started=fake_time;assert(!draft_ui_poll(started+999999,false)&&!mesh_draft_store_busy());
 assert(!draft_ui_poll(started+1000000,true)&&!mesh_draft_store_busy());
 fake_time=started+5000000;draft_ui_poll(fake_time,true);assert(mesh_draft_store_busy());uint32_t saved_job=draft_write_revision;
 strcpy(drafts[0].text,"newer edit");draft_ui_touch();assert(draft_revision!=saved_job);
 assert(draft_ui_poll(fake_time,false)&&draft_saved_revision==saved_job&&draft_saved_revision!=draft_revision);
 fake_time+=1000001;draft_ui_poll(fake_time,false);assert(mesh_draft_store_busy());draft_ui_poll(fake_time,false);assert(draft_revision==draft_saved_revision);
 mesh_draft_t recovered[4];uint32_t replies[4];size_t count=0;bool old=false;assert(mesh_draft_store_load(recovered,replies,4,&count,&old)&&count==1&&!strcmp(recovered[0].text,"newer edit"));
 // A quote-only draft is recoverable, and a draft-only peer appears in Direct.
 drafts[1]=(mesh_draft_t){.peer=42};draft_count=2;draft_slot=1;draft_reply_ids[1]=123;draft_ui_touch();assert(draft_ui_present(42,7)&&direct_ui_count()==1);uint32_t peer;assert(direct_ui_peer(0,&peer)&&peer==42);
 incoming(42,local_node_number,99,3);assert(direct_ui_count()==1); // union never duplicates an active conversation
 assert(store_message("!00000043",(const uint8_t*)"received",8,0,43,false,0));assert(direct_ui_count()==2&&direct_ui_peer(0,&peer)&&peer==43);
 draft_reply_ids[1]=0;draft_ui_touch();assert(!draft_ui_present(42,0)&&direct_ui_count()==2); // history keeps the conversation
 size_t writes=write_count;draft_slot=0;drafts[0].text[0]=0;draft_ui_touch();assert(!draft_ui_flush());assert(draft_ui_poll(fake_time,false)&&draft_revision==draft_saved_revision&&write_count==writes);
 assert(mesh_draft_store_load(recovered,replies,4,&count,&old)&&!count);
 puts("PASS: actual draft UI debounce/max-delay, old-job/new-edit isolation, quote-only indicators, draft/history peer union, immediate clearing and no radio writes");
}
