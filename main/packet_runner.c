#include "packet_runner.h"
#include <limits.h>

static const char routes[PACKET_LEVELS][PACKET_ROWS][PACKET_COLS + 1] = {
    {
        "SO...#............",
        ".##..#..!...###...",
        ".....#...R...#....",
        "..R..........#....",
        "...####..!...#....",
        ".............#..D.",
        "............O.....",
    },
    {
        "S....#............",
        ".###.#..####..!...",
        ".O.#....R..#......",
        ".!.#####...#.####.",
        ".......#...#O.R...",
        "..####.#...####...",
        "................D.",
    },
    {
        "SO.#.......#......",
        ".!.#..###..#..R...",
        "...#....#.....##..",
        "...###..#..!..#...",
        ".R......####..#...",
        "..####....O...#D..",
        "..........!.......",
    },
};

char packet_runner_tile(unsigned level, int x, int y) {
    if (level >= PACKET_LEVELS || x < 0 || x >= PACKET_COLS || y < 0 || y >= PACKET_ROWS) return '#';
    return routes[level][y][x];
}
static unsigned collectible_bit(unsigned level,int x,int y,char wanted){
    unsigned bit=1;
    if(packet_runner_tile(level,x,y)!=wanted)return 0;
    for(int row=0;row<PACKET_ROWS;row++)for(int col=0;col<PACKET_COLS;col++){
        if(row==y&&col==x)return bit;
        if(packet_runner_tile(level,col,row)==wanted)bit<<=1;
    }
    return 0;
}
unsigned packet_runner_relay_bit(unsigned level,int x,int y){return collectible_bit(level,x,y,'R');}
unsigned packet_runner_shield_bit(unsigned level,int x,int y){return collectible_bit(level,x,y,'O');}
static const uint8_t chaser_starts[PACKET_LEVELS][2]={{16,0},{16,0},{16,6}};
static void start_chaser(packet_runner_t *game){
    if(game->level>=PACKET_LEVELS)return;
    game->chaser_x=chaser_starts[game->level][0];game->chaser_y=chaser_starts[game->level][1];game->chaser_ready=true;
}
static void reset_route(packet_runner_t *game){
    game->x=game->y=game->relays=game->turn=game->shields_taken=0;game->attempt_moves=0;
    game->delivered=game->bumped=game->dropped=game->intercepted=game->shield=game->shielded=game->picked_shield=game->advanced=game->retried=false;
    start_chaser(game);
}
static void increment(unsigned *value){if(*value<UINT_MAX)(*value)++;}
void packet_runner_retry(packet_runner_t *game){
    if(game->delivered||game->level>=PACKET_LEVELS)return;
    if(game->attempt_moves){increment(&game->drops);increment(&game->route_drops);}
    reset_route(game);game->retried=true;
}
void packet_runner_reset(packet_runner_t *game){*game=(packet_runner_t){0};reset_route(game);}
// Five-cell patrols bounce at either end; all are on open floor, never relays/start/delivery.
static const uint8_t patrols[PACKET_LEVELS][PACKET_PATROLS][2] = {
    {{6,0},{7,6}}, {{12,2},{8,6}}, {{4,0},{2,4}}
};
bool packet_runner_enemy(const packet_runner_t *game, unsigned enemy, int *x, int *y) {
    if (!game || game->level >= PACKET_LEVELS || enemy >= PACKET_ENEMIES || !x || !y) return false;
    if(enemy==PACKET_CHASER){*x=game->chaser_ready?game->chaser_x:chaser_starts[game->level][0];*y=game->chaser_ready?game->chaser_y:chaser_starts[game->level][1];return true;}
    unsigned phase = (game->turn + enemy * 4) % PACKET_ENEMY_PERIOD;
    *x = patrols[game->level][enemy][0] + (phase <= 4 ? phase : 8 - phase);
    *y = patrols[game->level][enemy][1];
    return true;
}
static bool enemy_at(const packet_runner_t *game, int x, int y) {
    for (unsigned i=0; i<PACKET_ENEMIES; i++) {
        int ex,ey;
        if (packet_runner_enemy(game,i,&ex,&ey) && ex==x && ey==y) return true;
    }
    return false;
}
// Flood from the player's new tile; deterministic Up/Right/Down/Left ties.
// Jammers can cross interference, but never walls. The chaser moves on even turns.
static void chase(packet_runner_t *game,int target_x,int target_y){
    if(game->turn%2)return;
    enum {CELLS=PACKET_COLS*PACKET_ROWS};int16_t distance[CELLS];uint8_t queue[CELLS];
    for(unsigned i=0;i<CELLS;i++)distance[i]=-1;
    int start=target_y*PACKET_COLS+target_x,head=0,tail=0;distance[start]=0;queue[tail++]=start;
    const int dx[]={0,1,0,-1},dy[]={-1,0,1,0};
    while(head<tail){int at=queue[head++],x=at%PACKET_COLS,y=at/PACKET_COLS;
        for(unsigned i=0;i<4;i++){int nx=x+dx[i],ny=y+dy[i];if(packet_runner_tile(game->level,nx,ny)=='#')continue;int next=ny*PACKET_COLS+nx;
            if(distance[next]<0){distance[next]=distance[at]+1;queue[tail++]=next;}
        }
    }
    int best=distance[game->chaser_y*PACKET_COLS+game->chaser_x],bx=game->chaser_x,by=game->chaser_y;
    for(unsigned i=0;i<4;i++){int nx=game->chaser_x+dx[i],ny=game->chaser_y+dy[i];if(packet_runner_tile(game->level,nx,ny)=='#')continue;int d=distance[ny*PACKET_COLS+nx];
        if(d>=0&&(best<0||d<best)){best=d;bx=nx;by=ny;}
    }
    game->chaser_x=bx;game->chaser_y=by;
}
void packet_runner_move(packet_runner_t *game,int dx,int dy){
    game->advanced=false;
    if(game->delivered||game->level>=PACKET_LEVELS||
       !((dx==0&&(dy==0||dy==1||dy== -1))||(dy==0&&(dx==1||dx== -1))))return;
    int x=game->x+dx,y=game->y+dy;char tile=packet_runner_tile(game->level,x,y);
    game->bumped=game->dropped=game->intercepted=game->shielded=game->picked_shield=game->retried=false;
    if(tile=='#'){game->bumped=true;return;}
    if(!game->chaser_ready)start_chaser(game);
    increment(&game->moves);increment(&game->route_moves);increment(&game->attempt_moves);game->advanced=true;
    unsigned pickup=packet_runner_shield_bit(game->level,x,y);
    if(pickup&&!(game->shields_taken&pickup)&&!game->shield){game->shields_taken|=pickup;game->shield=true;game->picked_shield=true;}
    bool hit=enemy_at(game,x,y);
    game->turn=(game->turn+1)%PACKET_ENEMY_PERIOD;chase(game,x,y);hit=hit||enemy_at(game,x,y);
    if(tile=='!'||hit){
        game->intercepted=hit;
        if(game->shield){game->shield=false;game->shielded=true;}
        else{increment(&game->drops);increment(&game->route_drops);reset_route(game);game->advanced=game->dropped=true;game->intercepted=hit;return;}
    }
    game->x=x;game->y=y;game->relays|=packet_runner_relay_bit(game->level,x,y);
    if(tile=='D'&&game->relays==3){game->delivered=true;game->completed_routes|=1u<<game->level;}
}
void packet_runner_next(packet_runner_t *game){
    if(!game->delivered||game->level>=PACKET_LEVELS)return;
    if(game->level+1==PACKET_LEVELS)packet_runner_reset(game);
    else{game->level++;game->route_moves=game->route_drops=0;reset_route(game);}
}
bool packet_runner_secret(unsigned *progress, char key) {
    const char secret[] = "packet";
    if (*progress >= sizeof(secret) - 1) *progress = 0;
    if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
    if (key == secret[*progress]) (*progress)++;
    else *progress = key == secret[0] ? 1 : 0;
    if (*progress == sizeof(secret) - 1) { *progress = 0; return true; }
    return false;
}
