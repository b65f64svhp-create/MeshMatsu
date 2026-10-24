#include "telemetry_format.h"
#include <stdio.h>
static void metric_duration(uint64_t seconds, char *out, size_t size) {
    if (seconds < 60) snprintf(out,size,"%lus",(unsigned long)seconds);
    else if (seconds < 3600) snprintf(out,size,"%lum",(unsigned long)(seconds/60));
    else if (seconds < 86400) snprintf(out,size,"%luh %lum",(unsigned long)(seconds/3600),(unsigned long)(seconds%3600/60));
    else snprintf(out,size,"%lud %luh",(unsigned long)(seconds/86400),(unsigned long)(seconds%86400/3600));
}
void mesh_metrics_age(const mesh_device_metrics_t *m, int64_t now_unix, int64_t now_us, char *out, size_t size) {
    if (!m || !m->available) {snprintf(out,size,"No telemetry received yet");return;}
    if (m->cached) {snprintf(out,size,"Cached report / measurement age unknown");return;}
    char age[32];
    if (m->sample_time && now_unix >= 1700000000) {
        if (now_unix < m->sample_time) {snprintf(out,size,"Report time ahead of clock / age unknown");return;}
        uint64_t elapsed=(uint64_t)(now_unix-m->sample_time);metric_duration(elapsed,age,sizeof(age));
        snprintf(out,size,"Report: %s ago%s",age,elapsed>=3600?" / old reading":"");
    } else if (now_us >= m->received_us) {
        metric_duration((uint64_t)(now_us-m->received_us)/1000000,age,sizeof(age));
        snprintf(out,size,"Received %s ago / measurement age unknown",age);
    } else snprintf(out,size,"Measurement age unknown");
}
void mesh_metrics_battery(const mesh_device_metrics_t *m, char *out, size_t size) {
    if(!m || !m->has_battery)snprintf(out,size,"Unavailable");
    else if(m->battery>100)snprintf(out,size,"External power");
    else snprintf(out,size,"%lu%%",(unsigned long)m->battery);
}
void mesh_metrics_voltage(const mesh_device_metrics_t *m, char *out, size_t size) {
    if(!m || !m->has_voltage)snprintf(out,size,"Unavailable");
    else if(m->voltage<100)snprintf(out,size,"%.2f V",m->voltage);
    else snprintf(out,size,"%.3g V",m->voltage);
}
void mesh_metrics_uptime(const mesh_device_metrics_t *m, char *out, size_t size) {
    if(!m || !m->has_uptime)snprintf(out,size,"Unavailable");
    else metric_duration(m->uptime,out,size);
}
