#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "node_browser.h"
#include "../main/location_utils.c"
#include "../main/node_browser.c"
static mesh_node_t database[MESH_NODE_LIMIT+1];
static size_t total,own_reads;
static uint32_t local=1;
size_t mesh_radio_node_count(void){return total;}
uint32_t mesh_radio_local_node(void){return local;}
bool mesh_radio_get_node(size_t i,mesh_node_t *out){if(i>=total)return false;*out=database[i];return true;}
bool mesh_radio_get_node_by_number(uint32_t id,mesh_node_t *out){own_reads++;for(size_t i=0;i<total;i++)if(database[i].number==id){*out=database[i];return true;}return false;}
static const time_t now=1800000000;
static mesh_node_t node(uint32_t id,const char *name,int32_t lat,int32_t lon){mesh_node_t n={.number=id,.position_available=true,.latitude_i=lat,.longitude_i=lon,.position_time=now};snprintf(n.name,sizeof(n.name),"%s",name);return n;}
static void refresh(mesh_node_browser_t *v){mesh_node_browser_refresh(v,now,0);}
int main(void){
 total=7;database[0]=node(1,"Me",0,0);database[1]=node(2,"Near",0,100000);database[2]=node(3,"Far",0,10000000);database[3]=node(4,"A missing fix",0,0);database[3].position_available=false;database[4]=node(5,"Old fix",0,1000000);database[4].position_time=now-3601;database[5]=node(6,"Same position",0,0);database[6]=node(7,"Invalid fix",900000001,0);
 mesh_node_browser_t v={.sort=MESH_NODES_DISTANCE};refresh(&v);assert(v.count==6&&v.own_position_available&&!v.own_position_old&&own_reads==1);
 assert(v.nodes[0].number==6&&v.distances[0]==0&&!v.position_old[0]);assert(v.nodes[1].number==2&&v.nodes[2].number==5&&v.nodes[3].number==3);assert(v.position_old[2]&&isnan(v.distances[4])&&isnan(v.distances[5]));
 v.selected=3;database[2].longitude_i=50000;assert(mesh_node_browser_refresh(&v,now,5)==1&&v.nodes[1].number==3); // selected ID and distance follow a moving node
 // Known distances precede unknown even with favorite pinning; known favorites retain the user's preference.
 database[3].favorite_known=database[3].is_favorite=true;database[4].favorite_known=database[4].is_favorite=true;v.favorites_first=true;refresh(&v);assert(v.nodes[0].number==5&&v.nodes[4].number==4&&isnan(v.distances[4]));
 v.favorites_first=false;v.filter=MESH_NODES_NEARBY;refresh(&v);assert(v.count==4&&mesh_node_browser_radius(&v)==25000);
 // Inclusive radius test straddles the actual calculated great-circle distance.
 v.nearby_meters=5000;database[4].longitude_i=449660;refresh(&v);assert(v.count==4);database[4].longitude_i=449661;refresh(&v);assert(v.count==3);
 database[1].is_ignored=true;refresh(&v);assert(v.count==2);database[1].is_ignored=false;
 strcpy(v.query,"Near");refresh(&v);assert(v.count==1&&v.nodes[0].number==2);strcpy(v.query,"absent");refresh(&v);assert(!v.count&&!v.selected);v.query[0]=0;
 v.nearby_meters=0;uint32_t expected[]={50000,100000,250000,5000,10000,25000};for(unsigned i=0;i<6;i++){mesh_node_browser_step_radius(&v,true);assert(mesh_node_browser_radius(&v)==expected[i]);}for(unsigned i=0;i<6;i++)mesh_node_browser_step_radius(&v,false);assert(mesh_node_browser_radius(&v)==25000);v.nearby_meters=UINT32_MAX;assert(mesh_node_browser_radius(&v)==25000);
 // Missing local location never means (0,0), changes no other filters and recovers when a fix arrives.
 database[0].position_available=false;refresh(&v);assert(!v.count&&!v.own_position_available);v.filter=MESH_NODES_ALL;refresh(&v);assert(v.count==6);for(size_t i=0;i<v.count;i++)assert(isnan(v.distances[i]));database[0].position_available=true;v.filter=MESH_NODES_NEARBY;refresh(&v);assert(v.count==4);
 database[0].position_time=0;refresh(&v);assert(v.own_position_old);for(size_t i=0;i<v.count;i++)assert(v.position_old[i]);database[0].position_time=now+1;refresh(&v);assert(v.own_position_old);database[0].position_time=now;database[1].position_time=now-3600;refresh(&v);for(size_t i=0;i<v.count;i++)if(v.nodes[i].number==2)assert(!v.position_old[i]);database[1].position_time=now+1;refresh(&v);for(size_t i=0;i<v.count;i++)if(v.nodes[i].number==2)assert(v.position_old[i]);
 mesh_node_browser_refresh(&v,0,0);for(size_t i=0;i<v.count;i++)assert(v.position_old[i]);
 local=999;refresh(&v);assert(!v.count&&!v.own_position_available);local=1;
 // Antimeridian and polar distances remain finite; same-position ties are deterministic by name/ID.
 total=3;database[0]=node(1,"Me",0,1799000000);database[1]=node(2,"Across dateline",0,-1799000000);database[2]=node(3,"Farther",0,1795000000);v.nearby_meters=25000;refresh(&v);assert(v.count==1&&v.nodes[0].number==2&&v.distances[0]>22000&&v.distances[0]<23000);
 database[0]=node(1,"Me",900000000,0);database[1]=node(2,"Polar",900000000,1800000000);refresh(&v);assert(v.count==1&&isfinite(v.distances[0])&&v.distances[0]<1);
 v.filter=MESH_NODES_ALL;v.sort=MESH_NODES_DISTANCE;total=MESH_NODE_LIMIT+1;database[0]=node(1,"Me",0,0);for(size_t i=1;i<total;i++)database[i]=node((uint32_t)(1000+total-i),"Same",0,0);refresh(&v);assert(v.count==MESH_NODE_LIMIT);for(size_t i=1;i<v.count;i++)assert(v.nodes[i-1].number<v.nodes[i].number&&v.distances[i]==0);v.selected=v.nodes[v.count-1].number;total=2;refresh(&v);assert(v.selected==database[1].number);
 puts("PASS: nearest/unknown ordering, favorite precedence, cached distance alignment, moving-node selection, nearby bounds/presets/search/ignore, missing/invalid/zero/old/future positions, antimeridian/poles, deterministic ties and 128-row bounds; only passive getters linked");
}
