#include "packet_scores.h"
#include "nvs.h"
#include <string.h>
#define SCORE_BYTES 44
static bool better(const packet_record_t *a,const packet_record_t *b){return a->valid&&(!b->valid||a->drops<b->drops||(a->drops==b->drops&&a->moves<b->moves));}
static bool valid_record(const packet_record_t *r){return !r->valid||(r->moves&&r->drops<=r->moves);}
static bool valid_scores(const packet_scores_t *s){
 if(!s||s->highest_route>PACKET_LEVELS||!valid_record(&s->run)||(s->run.valid&&s->highest_route!=PACKET_LEVELS))return false;
 for(unsigned i=0;i<PACKET_LEVELS;i++)if(!valid_record(&s->route[i])||s->route[i].valid!=(i<s->highest_route))return false;
 return true;
}
bool packet_scores_merge(packet_scores_t *s,const packet_scores_t *other){
 if(!s||!valid_scores(other))return false;
 bool changed=false;
 for(unsigned i=0;i<PACKET_LEVELS;i++)if(better(&other->route[i],&s->route[i])){s->route[i]=other->route[i];changed=true;}
 if(better(&other->run,&s->run)){s->run=other->run;changed=true;}
 if(other->highest_route>s->highest_route){s->highest_route=other->highest_route;changed=true;}
 return changed;
}
bool packet_scores_record(packet_scores_t *s,const packet_runner_t *g){
 if(!s||!g||g->level>=PACKET_LEVELS||!g->delivered||g->relays!=3||
    (g->completed_routes&((1u<<(g->level+1))-1))!=((1u<<(g->level+1))-1)||!g->route_moves||g->route_drops>g->route_moves||g->route_moves>g->moves||g->route_drops>g->drops||g->drops>g->moves)return false;
 // Earlier routes must already have been recorded by this app session or load.
 if(g->level>s->highest_route)return false;
 bool changed=false;packet_record_t r={.valid=true,.moves=g->route_moves,.drops=g->route_drops};
 if(better(&r,&s->route[g->level])){s->route[g->level]=r;changed=true;}
 if(s->highest_route<g->level+1){s->highest_route=g->level+1;changed=true;}
 if(g->level+1==PACKET_LEVELS){r.moves=g->moves;r.drops=g->drops;if(better(&r,&s->run)){s->run=r;changed=true;}}
 return changed;
}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(i*8);}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t hash(const uint8_t *p){uint32_t h=2166136261u;for(unsigned i=0;i<40;i++)h=(h^p[i])*16777619u;return h;}
esp_err_t packet_scores_load(packet_scores_t *s){
 if(!s)return ESP_ERR_INVALID_ARG;
 *s=(packet_scores_t){0};nvs_handle_t h;esp_err_t result=nvs_open("meshdeck_mui",NVS_READONLY,&h);
 if(result==ESP_ERR_NVS_NOT_FOUND)return ESP_OK;
 if(result!=ESP_OK)return result;
 uint8_t wire[SCORE_BYTES];size_t n=sizeof(wire);result=nvs_get_blob(h,"runner_v1",wire,&n);nvs_close(h);
 if(result==ESP_ERR_NVS_NOT_FOUND)return ESP_OK;
 if(result!=ESP_OK)return result;
 if(n!=SCORE_BYTES||memcmp(wire,"PKR1",4)||wire[4]!=PACKET_RULESET||wire[7]||(wire[6]&~15)||hash(wire)!=get32(wire+40))return ESP_ERR_INVALID_ARG;
 packet_scores_t parsed={.highest_route=wire[5]};
 for(unsigned i=0;i<4;i++){packet_record_t *r=i==3?&parsed.run:&parsed.route[i];r->valid=(wire[6]&(1u<<i))!=0;r->moves=get32(wire+8+i*8);r->drops=get32(wire+12+i*8);if(!r->valid&&(r->moves||r->drops))return ESP_ERR_INVALID_ARG;}
 if(!valid_scores(&parsed))return ESP_ERR_INVALID_ARG;
 *s=parsed;return ESP_OK;
}
esp_err_t packet_scores_save(const packet_scores_t *s){
 if(!valid_scores(s))return ESP_ERR_INVALID_ARG;
 uint8_t wire[SCORE_BYTES]={0};memcpy(wire,"PKR1",4);wire[4]=PACKET_RULESET;wire[5]=s->highest_route;
 for(unsigned i=0;i<4;i++){const packet_record_t *r=i==3?&s->run:&s->route[i];if(r->valid){wire[6]|=1u<<i;put32(wire+8+i*8,r->moves);put32(wire+12+i*8,r->drops);}}
 put32(wire+40,hash(wire));nvs_handle_t h;esp_err_t result=nvs_open("meshdeck_mui",NVS_READWRITE,&h);if(result!=ESP_OK)return result;
 result=nvs_set_blob(h,"runner_v1",wire,sizeof(wire));if(result==ESP_OK)result=nvs_commit(h);nvs_close(h);return result;
}
