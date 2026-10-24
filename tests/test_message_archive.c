#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvs.h"
static uint8_t prefs[4];static bool have_prefs;static int prefs_error;
esp_err_t nvs_open(const char *s,int mode,nvs_handle_t *h){*h=1;return prefs_error;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *n){if(!have_prefs)return ESP_ERR_NVS_NOT_FOUND;if(*n<4)return ESP_ERR_INVALID_ARG;memcpy(out,prefs,4);*n=4;return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *in,size_t n){assert(n==4);memcpy(prefs,in,n);have_prefs=true;return prefs_error;}
esp_err_t nvs_commit(nvs_handle_t h){return prefs_error;}
void nvs_close(nvs_handle_t h){}
#define MESH_ARCHIVE_ROOT "archive-sd"
#define MESH_ARCHIVE_OUTBOX "archive-pending.bin"
#include "message_search.c"
#include "message_archive.c"
static int64_t now=1000000;
static void drain(void) {for(int i=0;i<3;i++){now+=500000;mesh_archive_poll(now);}assert(!atomic_load(&ar_state));}
static mesh_message_t message(unsigned id,uint32_t peer,uint8_t channel) {
 mesh_message_t m={.packet_id=id,.timestamp=1800000000+id,.peer_node=peer,.channel=channel,.reply_id=id==1?99:0,.outgoing=(id%2)==0,.delivery=id%2?MESH_DELIVERY_NONE:MESH_DELIVERY_ACKNOWLEDGED};
 strcpy(m.from,"!11223344");snprintf(m.text,sizeof(m.text),"Message %u: it's working 🙂",id);return m;
}
static void capture(unsigned id,uint32_t peer,uint8_t channel) {mesh_message_t m=message(id,peer,channel);mesh_archive_capture(&m,NULL,0,"Original name");}
static void page(size_t n,const char *query,bool all,uint32_t peer,uint8_t channel){assert(mesh_archive_request(n,query,all,peer,channel));drain();assert(mesh_archive_page()->ready);}
int main(void) {
 mesh_archive_init();drain();assert(!mesh_archive_status().enabled&&!mesh_archive_status().count);
 capture(1,42,1);assert(!ar_queue_count);assert(!mesh_archive_configure(true,1001));assert(mesh_archive_configure(true,5000));drain();
 mesh_message_t first=message(1,42,1);mesh_reaction_t reactions[2]={{.reply_id=1,.peer_node=42,.channel=1,.sender=77},{.reply_id=1,.peer_node=0,.channel=1}};
 strcpy(reactions[0].emoji,"👍");strcpy(reactions[1].emoji,"wrong scope");mesh_archive_capture(&first,reactions,2,"Original name");
 strcpy(first.text,"mutated");strcpy(reactions[0].emoji,"mutated");drain();assert(mesh_archive_status().count==1&&mesh_archive_status().bytes>320&&!mesh_archive_status().pending);
 page(0,"WORKING",false,42,0);assert(ar_page.count==1&&strstr(ar_page.rows[0].message.text,"it's working 🙂")&&ar_page.rows[0].reaction_count==1&&!strcmp(ar_page.rows[0].reactions,"👍")&&ar_page.rows[0].message.reply_id==99);
 capture(1,42,1);drain();assert(mesh_archive_status().count==1); // crash/restart replay does not duplicate an identity
 for(unsigned i=2;i<=40;i++)capture(i,i%2?42:0,i%2?1:3);drain();assert(mesh_archive_status().count==40);
 page(0,"",true,0,0);assert(ar_page.matches==40&&ar_page.count==16&&ar_page.rows[0].message.packet_id==40&&ar_page.rows[15].message.packet_id==25);
 page(1,"",true,0,0);assert(ar_page.rows[0].message.packet_id==24&&ar_page.rows[15].message.packet_id==9);
 page(2,"",true,0,0);assert(ar_page.count==8&&ar_page.rows[7].message.packet_id==1);
 page(2,"ORIGINAL",false,0,3);assert(ar_page.page==1&&ar_page.matches==20&&ar_page.count==4&&ar_page.rows[3].message.packet_id==2);
 page(0,"absent",true,0,0);assert(!ar_page.count&&!ar_page.matches);
 // Card absent: commit a checked internal holding area, permit clean exit,
 // recreate app state, then transfer automatically when the card returns.
 assert(!rename(MESH_ARCHIVE_ROOT,"card-away"));FILE *f=fopen(MESH_ARCHIVE_ROOT,"wb");assert(f);fclose(f);
 capture(41,42,1);drain();assert(mesh_archive_status().failed&&mesh_archive_status().pending==1&&!ar_queue_count&&mesh_archive_flush(now)==1);
 ar_need_sync=false;ar_retry_at=0;mesh_archive_init();drain();assert(ar_status.pending==1&&ar_status.enabled); // persistent settings and outbox survive restart
 assert(!remove(MESH_ARCHIVE_ROOT)&&!rename("card-away",MESH_ARCHIVE_ROOT));now+=31000000;drain();assert(ar_status.count==41&&!ar_status.pending);
 // Current-file corruption recovers a verified previous complete snapshot.
 capture(42,42,1);drain();f=fopen(AR_PATH,"r+b");assert(f);assert(!fseek(f,40,SEEK_SET));fputc(0xff,f);fclose(f);
 page(0,"",true,0,0);assert(ar_page.ready&&ar_status.count==41);
 capture(42,42,1);drain();assert(ar_status.count==42);
 // Newer current OR backup must prevent reads, rewrite and Clear.
 f=fopen(AR_PATH ".bak","wb");assert(f);uint8_t future[16]={0};memcpy(future,"MSA1",4);ar_put32(future+4,2);assert(fwrite(future,1,16,f)==16);fclose(f);
 capture(43,42,1);drain();assert(ar_status.protected_file&&ar_status.pending==1);assert(mesh_archive_clear());drain();assert(ar_status.protected_file&&ar_status.pending==1);
 assert(!remove(AR_PATH ".bak"));now+=31000000;drain();assert(ar_status.count==43&&!ar_status.pending);
 // Scale to the advertised bound in batches, then reduce retention exactly.
 for(unsigned start=44;start<=10080;start+=500){unsigned end=start+500;if(end>10081)end=10081;for(unsigned i=start;i<end;i++)capture(i,42,1);drain();}
 assert(ar_status.count==5000);assert(mesh_archive_configure(true,10000));drain();
 for(unsigned start=10081;start<=16080;start+=500){for(unsigned i=start;i<start+500;i++)capture(i,42,1);drain();}
 assert(ar_status.count==10000);page(0,"",true,0,0);assert(ar_page.matches==10000&&ar_page.rows[0].message.packet_id==16080);
 assert(mesh_archive_configure(true,2000));drain();assert(ar_status.count==2000);page(124,"",true,0,0);assert(ar_page.count==16&&ar_page.rows[15].message.packet_id==14081);
 // Retention cannot keep an old snapshot containing evicted records.
 assert(ar_validate(AR_PATH ".bak",AR_MAX,NULL)==AR_MISSING);
 page(0,"!0000002a",true,0,0);assert(ar_page.matches==2000);
 // Internal write failure keeps owned snapshots in memory and reports failure.
 assert(!mkdir(MESH_ARCHIVE_OUTBOX ".tmp",0700));capture(17000,42,1);drain();assert(ar_status.failed&&ar_queue_count==1&&mesh_archive_flush(now)==-1);
 assert(!rmdir(MESH_ARCHIVE_OUTBOX ".tmp"));now+=31000000;drain();assert(!ar_queue_count&&ar_status.count==2000);
 assert(mesh_archive_clear());drain();assert(!ar_status.count&&!ar_status.pending&&ar_page.ready&&!ar_page.count);
 assert(ar_validate(AR_PATH,AR_MAX,NULL)==AR_MISSING&&ar_validate(AR_PATH ".bak",AR_MAX,NULL)==AR_MISSING);
 assert(mesh_archive_configure(false,2000));capture(18000,42,1);drain();assert(!ar_status.count);
 puts("PASS: archive canonical UTF-8/reply/delivery/reaction roundtrip, immutable snapshots, identity dedup, scoped search/pages, 10000 retention, limit trim, SD outage/restart recovery, verified backup recovery, future-version protection, write failure retry, clear and disable");
 return 0;
}
