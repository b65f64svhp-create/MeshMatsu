#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "packet_runner.c"
static unsigned state_id(const packet_runner_t *g){return (g->y*PACKET_COLS+g->x)|((g->chaser_y*PACKET_COLS+g->chaser_x)<<7)|(g->shield<<14)|(g->shields_taken<<15)|(g->relays<<17)|(g->turn<<19);}
static packet_runner_t state_game(unsigned id,unsigned level){unsigned p=id&127,c=(id>>7)&127;return (packet_runner_t){.level=level,.x=p%PACKET_COLS,.y=p/PACKET_COLS,.chaser_x=c%PACKET_COLS,.chaser_y=c/PACKET_COLS,.chaser_ready=true,.shield=(id>>14)&1,.shields_taken=(id>>15)&3,.relays=(id>>17)&3,.turn=(id>>19)&7,.completed_routes=(1u<<level)-1};}
static void solve(packet_runner_t *g){
 enum {STATES=1<<22};unsigned *parent=malloc(STATES*sizeof(*parent)),*queue=malloc(STATES*sizeof(*queue));uint8_t *direction=malloc(STATES);assert(parent&&queue&&direction);memset(parent,255,STATES*sizeof(*parent));
 const int dx[]={1,0,-1,0,0},dy[]={0,1,0,-1,0};unsigned start=state_id(g),head=0,tail=0,goal=UINT_MAX;parent[start]=start;queue[tail++]=start;
 while(head<tail&&goal==UINT_MAX){unsigned id=queue[head++];packet_runner_t before=state_game(id,g->level);
  for(unsigned d=0;d<5;d++){packet_runner_t next=before;packet_runner_move(&next,dx[d],dy[d]);if(next.dropped||next.bumped)continue;unsigned nid=state_id(&next);if(parent[nid]!=UINT_MAX)continue;parent[nid]=id;direction[nid]=d;assert(tail<STATES);queue[tail++]=nid;if(next.delivered){goal=nid;break;}}
 }
 if(goal==UINT_MAX)fprintf(stderr,"No solution route %u, states %u\n",g->level+1,tail);assert(goal!=UINT_MAX);
 unsigned length=0;for(unsigned id=goal;id!=start;id=parent[id])queue[length++]=direction[id];unsigned steps=length;
 while(length){unsigned d=queue[--length];packet_runner_move(g,dx[d],dy[d]);assert(!g->dropped&&!g->bumped);}
 assert(g->delivered&&g->relays==3&&!g->drops);printf("Route %u solved: %u turns, %u explored states\n",g->level+1,steps,tail);fflush(stdout);free(parent);free(queue);free(direction);
}
int main(void){
 unsigned progress=0;const char *secret="PPaCkEt";for(unsigned i=0;i<strlen(secret);i++)assert(packet_runner_secret(&progress,secret[i])==(i+1==strlen(secret)));assert(!progress);
 for(unsigned l=0;l<PACKET_LEVELS;l++){unsigned relays=0,shields=0,goals=0,starts=0;for(int y=0;y<PACKET_ROWS;y++)for(int x=0;x<PACKET_COLS;x++){char t=packet_runner_tile(l,x,y);assert(t&&strchr("S.#R!DO",t));relays+=t=='R';shields+=t=='O';goals+=t=='D';starts+=t=='S';}assert(relays==2&&shields==2&&goals==1&&starts==1);
  for(unsigned turn=0;turn<PACKET_ENEMY_PERIOD;turn++)for(unsigned e=0;e<PACKET_ENEMIES;e++){packet_runner_t g={.level=l,.turn=turn};int x,y;assert(packet_runner_enemy(&g,e,&x,&y)&&packet_runner_tile(l,x,y)=='.');}
 }
 packet_runner_t g;packet_runner_reset(&g);packet_runner_move(&g,1,0);assert(g.shield&&g.picked_shield&&g.shields_taken==1&&g.moves==1);unsigned cx=g.chaser_x,cy=g.chaser_y;packet_runner_move(&g,-1,0);assert(g.turn==2&&abs(g.chaser_x-(int)cx)+abs(g.chaser_y-(int)cy)==1);cx=g.chaser_x;cy=g.chaser_y;packet_runner_move(&g,0,0);assert(g.turn==3&&g.chaser_x==cx&&g.chaser_y==cy);
 g=(packet_runner_t){.x=7,.y=1,.shield=true};packet_runner_move(&g,1,0);assert(g.shielded&&!g.shield&&!g.dropped&&g.x==8&&!g.drops);packet_runner_move(&g,0,0);assert(g.dropped&&g.drops==1&&g.route_drops==1&&!g.x&&!g.relays&&!g.shields_taken&&!g.shield&&!g.turn);
 g=(packet_runner_t){.x=11,.y=6,.shield=true};packet_runner_move(&g,1,0);assert(g.shield&&!g.picked_shield&&!(g.shields_taken&2));g.shield=false;packet_runner_move(&g,0,0);assert(g.shield&&g.picked_shield&&(g.shields_taken&2));
 g=(packet_runner_t){.x=5,.y=0,.relays=3};packet_runner_move(&g,1,0);assert(g.dropped&&g.intercepted&&!g.relays&&!g.x&&g.drops==1);
 g=(packet_runner_t){.x=7,.y=0,.shield=true};packet_runner_move(&g,-1,0);assert(g.shielded&&g.intercepted&&!g.dropped&&g.x==6&&!g.shield); // pre-move crossing collision
 g=(packet_runner_t){.x=14,.y=0,.turn=1,.chaser_x=16,.chaser_y=0,.chaser_ready=true};packet_runner_move(&g,1,0);assert(g.dropped&&g.intercepted); // chaser steps onto player
 packet_runner_reset(&g);packet_runner_move(&g,-1,0);assert(g.bumped&&!g.advanced&&!g.moves&&!g.turn);packet_runner_move(&g,INT_MAX,INT_MIN);assert(!g.moves);packet_runner_move(&g,0,0);assert(g.turn==1&&g.moves==1);packet_runner_retry(&g);assert(g.drops==1&&g.route_drops==1&&g.moves==1&&g.route_moves==1&&!g.attempt_moves&&g.retried);packet_runner_retry(&g);assert(g.drops==1);packet_runner_next(&g);assert(!g.level);
 g=(packet_runner_t){.x=15,.y=5};packet_runner_move(&g,1,0);assert(!g.delivered);g.relays=3;packet_runner_move(&g,0,0);assert(g.delivered&&g.completed_routes==1);packet_runner_retry(&g);assert(g.delivered);unsigned moves=g.moves;packet_runner_move(&g,-1,0);assert(g.moves==moves&&!g.advanced);
 packet_runner_reset(&g);for(unsigned l=0;l<PACKET_LEVELS;l++){assert(g.level==l);solve(&g);assert(g.completed_routes==(1u<<(l+1))-1);packet_runner_next(&g);}assert(!g.level&&!g.moves&&!g.drops&&!g.completed_routes);
 g.moves=g.route_moves=UINT_MAX;packet_runner_move(&g,1,0);assert(g.moves==UINT_MAX&&g.route_moves==UINT_MAX);g.drops=g.route_drops=UINT_MAX;packet_runner_retry(&g);assert(g.drops==UINT_MAX&&g.route_drops==UINT_MAX);assert(packet_runner_tile(3,0,0)=='#');
 puts("PASS: chaser timing/walls/capture, patrol crossings, shield pickup/non-waste/single-hit/reset, active retry penalty, frozen walls/results, saturating counters, complete zero-drop winning paths for all routes");
}
