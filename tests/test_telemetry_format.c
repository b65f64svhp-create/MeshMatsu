#include <assert.h>
#include <string.h>
#include "../main/telemetry_format.c"
int main(void) {
    char text[128];mesh_device_metrics_t m={0};
    mesh_metrics_age(&m,1800000000,0,text,sizeof(text));assert(strstr(text,"No telemetry"));
    mesh_metrics_battery(&m,text,sizeof(text));assert(!strcmp(text,"Unavailable"));
    m=(mesh_device_metrics_t){.available=true,.cached=true,.has_battery=true,.battery=0};
    mesh_metrics_battery(&m,text,sizeof(text));assert(!strcmp(text,"0%"));
    m.battery=101;mesh_metrics_battery(&m,text,sizeof(text));assert(!strcmp(text,"External power"));
    m.battery=UINT32_MAX;mesh_metrics_battery(&m,text,sizeof(text));assert(!strcmp(text,"External power"));
    mesh_metrics_age(&m,1800000000,1000000,text,sizeof(text));assert(strstr(text,"Cached")&&strstr(text,"unknown"));
    m.cached=false;m.sample_time=1800000000;
    mesh_metrics_age(&m,1800000030,0,text,sizeof(text));assert(strstr(text,"30s ago")&&!strstr(text,"old"));
    mesh_metrics_age(&m,1800003900,0,text,sizeof(text));assert(strstr(text,"1h 5m")&&strstr(text,"old"));
    mesh_metrics_age(&m,1799999999,0,text,sizeof(text));assert(strstr(text,"ahead")&&strstr(text,"unknown"));
    m.sample_time=0;m.received_us=1000000;
    mesh_metrics_age(&m,1800000000,121000000,text,sizeof(text));assert(strstr(text,"Received 2m ago")&&strstr(text,"measurement age unknown"));
    mesh_metrics_age(&m,0,0,text,sizeof(text));assert(strstr(text,"unknown"));
    mesh_metrics_voltage(&m,text,sizeof(text));assert(!strcmp(text,"Unavailable"));
    m.has_voltage=true;m.voltage=3.82f;mesh_metrics_voltage(&m,text,sizeof(text));assert(!strcmp(text,"3.82 V"));
    m.voltage=0;mesh_metrics_voltage(&m,text,sizeof(text));assert(!strcmp(text,"0.00 V"));
    m.has_uptime=true;m.uptime=UINT32_MAX;mesh_metrics_uptime(&m,text,sizeof(text));assert(strstr(text,"49710d"));
    char tiny[4];mesh_metrics_age(&m,1800000000,2000000,tiny,sizeof(tiny));assert(tiny[3]=='\0');
    puts("PASS: telemetry age/cache/future/unknown-clock labels, external power, optional zeros, units, long uptime and bounded text");
}
