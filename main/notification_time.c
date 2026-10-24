#include "notification_time.h"
#include "nvs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static const struct { const char *name,*tz; } zones[] = {
 {"Edmonton / Mountain", "MST7MDT,M3.2.0,M11.1.0"},
 {"Vancouver / Pacific", "PST8PDT,M3.2.0,M11.1.0"},
 {"Regina / Saskatchewan", "CST6"},
 {"Winnipeg / Central", "CST6CDT,M3.2.0,M11.1.0"},
 {"Toronto / Eastern", "EST5EDT,M3.2.0,M11.1.0"},
 {"Halifax / Atlantic", "AST4ADT,M3.2.0,M11.1.0"},
 {"St. John's / Newfoundland", "NST3:30NDT,M3.2.0,M11.1.0"},
 {"UTC", "UTC0"},
 {"London", "GMT0BST,M3.5.0/1,M10.5.0/2"},
 {"Berlin / Central Europe", "CET-1CEST,M3.5.0/2,M10.5.0/3"},
 {"Tokyo", "JST-9"}, {"Kolkata", "IST-5:30"},
 {"Sydney", "AEST-10AEDT,M10.1.0/2,M4.1.0/3"},
 {"Auckland", "NZST-12NZDT,M9.5.0/2,M4.1.0/3"},
 {"Honolulu", "HST10"}, {"Anchorage", "AKST9AKDT,M3.2.0,M11.1.0"},
 {"Fixed UTC offset", NULL}
};
mesh_time_settings_t mesh_time_defaults(void) { return (mesh_time_settings_t){0,0,-420}; }
size_t mesh_time_zone_count(void) { return sizeof(zones)/sizeof(zones[0]); }
const char *mesh_time_zone_name(uint8_t zone) { return zone < mesh_time_zone_count() ? zones[zone].name : "Unknown"; }
bool mesh_time_valid(const mesh_time_settings_t *s) {
 return s && s->zone < mesh_time_zone_count() && s->use_24h <= 1 && s->offset_minutes >= -720 && s->offset_minutes <= 840 && s->offset_minutes % 15 == 0;
}
void mesh_time_apply(const mesh_time_settings_t *s) {
 if (!mesh_time_valid(s)) return;
 char fixed[24]; const char *tz=zones[s->zone].tz;
 if (!tz) { int minutes=-s->offset_minutes; snprintf(fixed,sizeof(fixed),"UTC%s%d:%02d",minutes<0?"-":"",abs(minutes)/60,abs(minutes)%60);tz=fixed; }
 setenv("TZ",tz,1);tzset();
}
static esp_err_t load(const char *key,uint8_t *data,size_t size) {
 nvs_handle_t h;esp_err_t result=nvs_open("meshdeck_mui",NVS_READONLY,&h);if(result!=ESP_OK)return result;
 size_t length=size;result=nvs_get_blob(h,key,data,&length);nvs_close(h);
 return result==ESP_OK && length!=size ? ESP_ERR_INVALID_ARG : result;
}
static esp_err_t save(const char *key,const uint8_t *data,size_t size) {
 nvs_handle_t h;esp_err_t result=nvs_open("meshdeck_mui",NVS_READWRITE,&h);if(result!=ESP_OK)return result;
 result=nvs_set_blob(h,key,data,size);if(result==ESP_OK)result=nvs_commit(h);nvs_close(h);return result;
}
esp_err_t mesh_time_load(mesh_time_settings_t *s) {
 if(!s)return ESP_ERR_INVALID_ARG;
 uint8_t data[5];esp_err_t result=load("time_v1",data,sizeof(data));if(result!=ESP_OK)return result;
 mesh_time_settings_t value={data[1],data[2],(int16_t)((uint16_t)data[3]|(uint16_t)data[4]<<8)};
 if(data[0]!=1||!mesh_time_valid(&value))return ESP_ERR_INVALID_ARG;
 *s=value;
 return ESP_OK;
}
esp_err_t mesh_time_save(const mesh_time_settings_t *s) {
 if(!mesh_time_valid(s))return ESP_ERR_INVALID_ARG;
 uint8_t data[]={1,s->zone,s->use_24h,(uint8_t)s->offset_minutes,(uint8_t)((uint16_t)s->offset_minutes>>8)};
 return save("time_v1",data,sizeof(data));
}
void mesh_time_stamp(const mesh_time_settings_t *s,uint32_t timestamp,char *out,size_t size) {
 if(timestamp<1700000000U){snprintf(out,size,"NO TIME");return;}
 time_t value=timestamp;struct tm local;
 if(!localtime_r(&value,&local)){snprintf(out,size,"NO TIME");return;}
 char month[4]={0};strftime(month,sizeof(month),"%b",&local);
 for(size_t i=0;month[i];i++)if(month[i]>='a'&&month[i]<='z')month[i]-='a'-'A';
 if(s->use_24h)snprintf(out,size,"%s-%02d %02d:%02d",month,local.tm_mday,local.tm_hour,local.tm_min);
 else snprintf(out,size,"%s-%02d %02d:%02d%s",month,local.tm_mday,local.tm_hour%12?local.tm_hour%12:12,local.tm_min,local.tm_hour<12?"am":"pm");
}
bool mesh_time_clock(const mesh_time_settings_t *s, uint32_t timestamp,
                     char *clock, size_t clock_size, char *date, size_t date_size) {
 if (!clock || !clock_size || !date || !date_size) return false;
 char stamp[32] = "NO TIME";
 if (mesh_time_valid(s)) mesh_time_stamp(s, timestamp, stamp, sizeof(stamp));
 char *separator = strchr(stamp, ' ');
 if (!strcmp(stamp, "NO TIME") || !separator) {
  snprintf(clock, clock_size, "--:--");
  snprintf(date, date_size, "Time not set");
  return false;
 }
 *separator = '\0';
 snprintf(clock, clock_size, "%s", separator + 1);
 snprintf(date, date_size, "%s", stamp);
 return true;
}
esp_err_t mesh_channel_alerts_load(uint8_t *mask) {
 if(!mask)return ESP_ERR_INVALID_ARG;
 uint8_t data[2];esp_err_t result=load("channel_alerts",data,sizeof(data));if(result!=ESP_OK)return result;
 if(data[0]!=1)return ESP_ERR_INVALID_ARG;
 *mask=data[1];
 return ESP_OK;
}
esp_err_t mesh_channel_alerts_save(uint8_t mask) { const uint8_t data[]={1,mask};return save("channel_alerts",data,sizeof(data)); }
bool mesh_channel_alert_enabled(uint8_t mask,uint8_t channel) { return channel<8 && (mask&(1u<<channel))!=0; }
