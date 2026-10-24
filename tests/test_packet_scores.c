#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "packet_runner.c"
#include "packet_scores.c"
static uint8_t saved_wire[128],pending_wire[128];static size_t saved_size,pending_size;
static int read_failure,set_failure,commit_failure,open_failure;
static unsigned writes,commits,reads;
int nvs_open(const char *name,int mode,nvs_handle_t *h){assert(!strcmp(name,"meshdeck_mui"));*h=1;return open_failure;}
int nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *n){assert(!strcmp(key,"runner_v1"));reads++;if(read_failure)return read_failure;if(!saved_size)return ESP_ERR_NVS_NOT_FOUND;memcpy(out,saved_wire,saved_size<*n?saved_size:*n);*n=saved_size;return 0;}
int nvs_set_blob(nvs_handle_t h,const char *key,const void *in,size_t n){assert(!strcmp(key,"runner_v1")&&n==44);writes++;if(set_failure)return set_failure;memcpy(pending_wire,in,n);pending_size=n;return 0;}
int nvs_commit(nvs_handle_t h){commits++;if(commit_failure)return commit_failure;memcpy(saved_wire,pending_wire,pending_size);saved_size=pending_size;return 0;}
void nvs_close(nvs_handle_t h){}
static packet_runner_t winner(unsigned level,unsigned moves,unsigned drops){return (packet_runner_t){.level=level,.moves=moves+100*level,.drops=drops,.route_moves=moves,.route_drops=drops,.delivered=true,.relays=3,.completed_routes=(1u<<(level+1))-1};}
int main(void){packet_scores_t s,loaded;assert(!packet_scores_load(&s)&&!s.highest_route&&!s.run.valid);
 packet_runner_t g=winner(0,40,2);assert(packet_scores_record(&s,&g)&&s.route[0].moves==40&&s.highest_route==1);assert(!packet_scores_record(&s,&g));g=winner(0,80,1);assert(packet_scores_record(&s,&g));g=winner(0,90,1);assert(!packet_scores_record(&s,&g));g=winner(0,70,1);assert(packet_scores_record(&s,&g));g=winner(0,100,0);assert(packet_scores_record(&s,&g));assert(!packet_scores_save(&s)&&writes==1&&commits==1&&saved_size==44);assert(!packet_scores_load(&loaded)&&loaded.route[0].moves==100&&!loaded.route[0].drops);
 g=winner(2,30,0);assert(!packet_scores_record(&s,&g));g=winner(1,60,0);assert(packet_scores_record(&s,&g)&&s.highest_route==2);g=winner(2,30,0);assert(packet_scores_record(&s,&g)&&s.highest_route==3&&s.run.valid&&s.run.moves==230);g.moves=220;assert(packet_scores_record(&s,&g)&&s.run.moves==220);
 commit_failure=7;assert(packet_scores_save(&s)==7);assert(!packet_scores_load(&loaded)&&loaded.highest_route==1&&loaded.route[0].moves==100);commit_failure=0;assert(!packet_scores_save(&s)&&!packet_scores_load(&loaded)&&loaded.run.moves==220);
 packet_scores_t merged={0};assert(packet_scores_merge(&merged,&loaded)&&merged.highest_route==3);g=winner(0,10,0);assert(packet_scores_record(&merged,&g));assert(!packet_scores_merge(&merged,&loaded)&&merged.route[0].moves==10);assert(packet_scores_merge(&s,&merged)&&s.route[0].moves==10);
 g.delivered=false;assert(!packet_scores_record(&s,&g));g=winner(0,1,2);assert(!packet_scores_record(&s,&g));g=winner(0,10,0);g.completed_routes=0;assert(!packet_scores_record(&s,&g));g=winner(3,10,0);assert(!packet_scores_record(&s,&g));assert(!packet_scores_record(NULL,&g));
 uint8_t good[44];memcpy(good,saved_wire,44);saved_wire[10]^=1;assert(packet_scores_load(&loaded)!=0&&!loaded.highest_route);memcpy(saved_wire,good,44);saved_wire[4]++;assert(packet_scores_load(&loaded)!=0);memcpy(saved_wire,good,44);saved_wire[7]=1;put32(saved_wire+40,hash(saved_wire));assert(packet_scores_load(&loaded)!=0);memcpy(saved_wire,good,44);saved_wire[6]|=128;put32(saved_wire+40,hash(saved_wire));assert(packet_scores_load(&loaded)!=0);memcpy(saved_wire,good,44);put32(saved_wire+8,0);put32(saved_wire+40,hash(saved_wire));assert(packet_scores_load(&loaded)!=0);memcpy(saved_wire,good,44);saved_size=43;assert(packet_scores_load(&loaded)!=0);saved_size=45;assert(packet_scores_load(&loaded)!=0);saved_size=44;memcpy(saved_wire,good,44);
 read_failure=7;assert(packet_scores_load(&loaded)==7&&!loaded.highest_route);read_failure=0;open_failure=ESP_ERR_NVS_NOT_FOUND;assert(!packet_scores_load(&loaded)&&!loaded.highest_route);open_failure=7;assert(packet_scores_load(&loaded)==7&&packet_scores_save(&s)==7);open_failure=0;set_failure=7;unsigned before=commits;assert(packet_scores_save(&s)==7&&commits==before);set_failure=0;
 s.highest_route=4;before=writes;assert(packet_scores_save(&s)!=0&&writes==before);assert(packet_scores_load(NULL)!=0&&packet_scores_save(NULL)!=0);
 puts("PASS: route/run ranking, progress and completion guards, idempotent ties, record merge, NVS restart persistence, failed commits preserve saved scores, version/checksum/semantic/length corruption and read/open/write failures");return 0;}
