#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../main/map_view.c"
int main(void) {
    double x,y,lat,lon;mesh_map_project(55.17,-118.8,&x,&y);mesh_map_unproject(x,y,&lat,&lon);
    assert(fabs(lat-55.17)<1e-8&&fabs(lon+118.8)<1e-8);
    mesh_map_view_t view;mesh_map_center(&view,55.17,-118.8,10);
    assert(mesh_map_marker(&view,551700000,-1188000000,760,266,&x,&y)&&fabs(x-380)<1e-6&&fabs(y-133)<1e-6);
    double cx=view.x,cy=view.y;mesh_map_pan(&view,96,0);
    mesh_map_marker(&view,551700000,-1188000000,760,266,&x,&y);assert(fabs(x-284)<1e-6&&cy==view.y);
    mesh_map_zoom(&view,1);assert(view.zoom==11&&view.x!=cx&&view.y==cy);
    mesh_map_zoom(&view,100);assert(view.zoom==13);mesh_map_zoom(&view,-100);assert(view.zoom==1);
    mesh_map_center(&view,0,179.99,7);assert(mesh_map_marker(&view,0,-1799900000,760,266,&x,&y)&&fabs(x-380)<2);
    mesh_map_pan(&view,1e9,-1e9);assert(view.x>=0&&view.x<1&&view.y==0);
    mesh_map_project(90,180,&x,&y);assert(isfinite(x)&&isfinite(y)&&y>=0&&y<=1);
    puts("PASS: Mercator projection/roundtrip, pan, centered zoom, zoom limits, antimeridian wrap, polar clamp and node placement");
}
