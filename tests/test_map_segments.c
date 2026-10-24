#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../main/map_view.c"
static void check(mesh_map_segment_t *s,size_t n,int w,int h){for(size_t i=0;i<n;i++)assert(isfinite(s[i].x0)&&isfinite(s[i].y0)&&isfinite(s[i].x1)&&isfinite(s[i].y1)&&s[i].x0>=0&&s[i].x0<w&&s[i].x1>=0&&s[i].x1<w&&s[i].y0>=0&&s[i].y0<h&&s[i].y1>=0&&s[i].y1<h);}
int main(void){mesh_map_view_t v;mesh_map_segment_t out[3];mesh_map_center(&v,0,0,8);
 size_t n=mesh_map_segments(&v,0,-1000000,0,1000000,760,244,out);assert(n==1&&out[0].x0<380&&out[0].x1>380&&out[0].y0==122);check(out,n,760,244);
 n=mesh_map_segments(&v,0,-100000000,0,100000000,760,244,out);assert(n==1&&fabs(out[0].x0)<1e-6&&fabs(out[0].x1-759)<1e-6); // both endpoints outside, line crosses viewport
 assert(!mesh_map_segments(&v,100000000,-100000000,100000000,100000000,760,244,out));
 mesh_map_center(&v,0,179.99,8);n=mesh_map_segments(&v,0,1799800000,0,-1799800000,760,244,out);assert(n==1&&fabs(out[0].x1-out[0].x0)<10);check(out,n,760,244);
 mesh_map_center(&v,0,0,1);n=mesh_map_segments(&v,0,1790000000,0,-1790000000,760,244,out);assert(n==2);for(size_t i=0;i<n;i++)assert(fabs(out[i].x1-out[i].x0)<4);check(out,n,760,244); // two short world copies, never a line across the world
 mesh_map_center(&v,80,0,3);n=mesh_map_segments(&v,900000000,0,700000000,0,760,244,out);assert(n==1);check(out,n,760,244);
 v.zoom=99;assert(!mesh_map_segments(&v,0,0,0,0,760,244,out));v.zoom=3;assert(!mesh_map_segments(&v,900000001,0,0,0,760,244,out)&&!mesh_map_segments(&v,0,0,0,0,0,244,out)&&!mesh_map_segments(&v,0,0,0,0,760,244,NULL));
 // Pan/zoom clipping stays bounded for wide random geographic segments.
 for(int zoom=1;zoom<=13;zoom++)for(int i=0;i<500;i++){mesh_map_center(&v,(i%160)-80,(i*17%360)-180,zoom);n=mesh_map_segments(&v,((i*137)%1800000001)-900000000,((i*123457)%2000000001)-1000000000,((i*1117)%1800000001)-900000000,((i*77111)%2000000001)-1000000000,760,244,out);assert(n<=3);check(out,n,760,244);}
 puts("PASS: clipped map segments, offscreen crossing/hidden lines, pan/zoom/polar bounds and short antimeridian/world-wrap copies");}
