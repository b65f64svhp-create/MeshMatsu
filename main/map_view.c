#include "map_view.h"
#include <math.h>
#define PI 3.14159265358979323846
static double clamp(double n, double low, double high) { return fmax(low, fmin(high, n)); }
void mesh_map_project(double lat, double lon, double *x, double *y) {
    lat = clamp(lat, -85.05112878, 85.05112878) * PI / 180.0;
    *x = (lon + 180.0) / 360.0; *x -= floor(*x);
    *y = clamp((1.0 - asinh(tan(lat)) / PI) / 2.0, 0, 1);
}
void mesh_map_unproject(double x, double y, double *lat, double *lon) {
    x -= floor(x); *lon = x * 360.0 - 180.0;
    *lat = atan(sinh(PI * (1.0 - 2.0 * clamp(y, 0, 1)))) * 180.0 / PI;
}
void mesh_map_center(mesh_map_view_t *v, double lat, double lon, unsigned z) {
    mesh_map_project(lat, lon, &v->x, &v->y); v->zoom = z > 13 ? 13 : z;
}
void mesh_map_pan(mesh_map_view_t *v, double dx, double dy) {
    double pixels = 256.0 * (1u << v->zoom);
    v->x += dx / pixels; v->x -= floor(v->x); v->y = clamp(v->y + dy / pixels, 0, 1);
}
void mesh_map_zoom(mesh_map_view_t *v, int delta) { v->zoom = (unsigned)clamp((int)v->zoom + delta, 1, 13); }
bool mesh_map_marker(const mesh_map_view_t *v, int32_t lat, int32_t lon, int w, int h, double *x, double *y) {
    double nx, ny; mesh_map_project(lat / 1e7, lon / 1e7, &nx, &ny);
    double dx = nx - v->x; dx -= floor(dx + .5);
    double pixels = 256.0 * (1u << v->zoom);
    *x = w / 2.0 + dx * pixels; *y = h / 2.0 + (ny - v->y) * pixels;
    return *x >= 0 && *x < w && *y >= 0 && *y < h;
}

static bool map_clip(double p,double q,double *low,double *high){
    if(p==0)return q>=0;
    double r=q/p;
    if(p<0){if(r>*high)return false;if(r>*low)*low=r;}
    else{if(r<*low)return false;if(r<*high)*high=r;}
    return true;
}
size_t mesh_map_segments(const mesh_map_view_t *v,int32_t lat0,int32_t lon0,int32_t lat1,int32_t lon1,int w,int h,mesh_map_segment_t out[3]){
    if(!v||!out||w<=0||h<=0||v->zoom>13||!isfinite(v->x)||!isfinite(v->y)||
       lat0< -900000000||lat0>900000000||lat1< -900000000||lat1>900000000||lon0< -1800000000||lon0>1800000000||lon1< -1800000000||lon1>1800000000)return 0;
    double ax,ay,bx,by;mesh_map_project(lat0/1e7,lon0/1e7,&ax,&ay);mesh_map_project(lat1/1e7,lon1/1e7,&bx,&by);
    double a=ax-v->x;a-=floor(a+.5);double delta=bx-ax;delta-=floor(delta+.5);
    double pixels=256.0*(1u<<v->zoom);size_t count=0;
    for(int copy=-1;copy<=1;copy++){
        double x0=w/2.0+(a+copy)*pixels,y0=h/2.0+(ay-v->y)*pixels;
        double dx=delta*pixels,dy=(by-ay)*pixels,low=0,high=1;
        if(!map_clip(-dx,x0,&low,&high)||!map_clip(dx,w-1-x0,&low,&high)||!map_clip(-dy,y0,&low,&high)||!map_clip(dy,h-1-y0,&low,&high))continue;
        out[count++]=(mesh_map_segment_t){clamp(x0+low*dx,0,w-1),clamp(y0+low*dy,0,h-1),clamp(x0+high*dx,0,w-1),clamp(y0+high*dy,0,h-1)};
    }
    return count;
}
