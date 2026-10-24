#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "../main/location_utils.c"
int main(void) {
    int32_t value;assert(mesh_location_parse("-118.8000000",false,&value)&&value==-1188000000);
    assert(mesh_location_parse("0",true,&value)&&!value);assert(mesh_location_parse("90",true,&value)&&value==900000000);
    const char *bad[]={"","-",".","nan","inf","1e2"," 2","2 ","1.23456789","90.0000001","-91","--2","1.2.3"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(!mesh_location_parse(bad[i],true,&value));
    mesh_node_t a={.position_available=true},b={.position_available=true,.longitude_i=10000000};double meters,bearing;
    assert(mesh_location_distance(&a,&b,&meters,&bearing)&&fabs(meters-111194.93)<1&&fabs(bearing-90)<1e-8&&!strcmp(mesh_location_compass(bearing),"E"));
    b.latitude_i=10000000;b.longitude_i=0;mesh_location_distance(&a,&b,&meters,&bearing);assert(fabs(bearing)<1e-8);
    a.longitude_i=1799000000;b.longitude_i=-1799000000;b.latitude_i=0;mesh_location_distance(&a,&b,&meters,&bearing);assert(meters<23000&&bearing>89&&bearing<91);
    b=a;mesh_location_distance(&a,&b,&meters,&bearing);assert(!meters&&isfinite(bearing));b.position_available=false;assert(!mesh_location_distance(&a,&b,&meters,&bearing));
    assert(mesh_location_precision_step(15,true,true)==-1&&mesh_location_precision_step(19,true,false)==32);
    assert(!strcmp(mesh_location_precision_label(0),"Off"));
    puts("PASS: decimal validation/bounds, signed/zero coordinates, distance/bearing, antimeridian, missing positions and precision choices");
}
