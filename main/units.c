#include "units.h"
#include "nvs.h"
#include <math.h>
#include <stdio.h>
const char *mesh_units_name(uint8_t u){return u==MESH_UNITS_IMPERIAL?"Imperial (ft / mi)":"Metric (m / km)";}
void mesh_units_distance(uint8_t u,double meters,char *out,size_t size){
 if(!out||!size)return;
 if(!isfinite(meters)||meters<0){snprintf(out,size,"unavailable");return;}
 if(u==MESH_UNITS_IMPERIAL){if(meters<160.9344)snprintf(out,size,"%.0f ft",meters/0.3048);else snprintf(out,size,"%.2f mi",meters/1609.344);}
 else {if(meters<1000)snprintf(out,size,"%.0f m",meters);else snprintf(out,size,"%.2f km",meters/1000);}
}
const char *mesh_units_precision(uint8_t u,int bits){
 static const char *metric[]={"Approx. 23.3 km","Approx. 11.7 km","Approx. 5.8 km","Approx. 2.9 km","Approx. 1.5 km","Approx. 729 m","Approx. 364 m","Approx. 182 m","Approx. 91 m","Approx. 45 m"};
 static const char *imperial[]={"Approx. 14.5 mi","Approx. 7.3 mi","Approx. 3.6 mi","Approx. 1.8 mi","Approx. 0.9 mi","Approx. 0.45 mi","Approx. 0.23 mi","Approx. 0.11 mi","Approx. 299 ft","Approx. 148 ft"};
 if(bits<0)return "Keep current radio setting";
 if(!bits)return "Off";
 if(bits==32)return "Full precision";
 return bits>=10&&bits<=19?(u==MESH_UNITS_IMPERIAL?imperial:metric)[bits-10]:"Custom radio precision";
}
esp_err_t mesh_units_load(uint8_t *unit){
 if(!unit)return ESP_ERR_INVALID_ARG;
 nvs_handle_t h;esp_err_t result=nvs_open("meshdeck_mui",NVS_READONLY,&h);if(result!=ESP_OK)return result;
 uint8_t b[2];size_t n=sizeof(b);result=nvs_get_blob(h,"units_v1",b,&n);nvs_close(h);
 if(result!=ESP_OK)return result;
 if(n!=2||b[0]!=1||b[1]>MESH_UNITS_IMPERIAL)return ESP_ERR_INVALID_ARG;
 *unit=b[1];return ESP_OK;
}
esp_err_t mesh_units_save(uint8_t unit){
 if(unit>MESH_UNITS_IMPERIAL)return ESP_ERR_INVALID_ARG;
 nvs_handle_t h;esp_err_t result=nvs_open("meshdeck_mui",NVS_READWRITE,&h);if(result!=ESP_OK)return result;
 uint8_t b[]={1,unit};result=nvs_set_blob(h,"units_v1",b,sizeof(b));if(result==ESP_OK)result=nvs_commit(h);nvs_close(h);return result;
}
const char *mesh_units_temperature_name(uint8_t unit){return unit==MESH_TEMP_FAHRENHEIT?"Fahrenheit (F)":"Celsius (C)";}
double mesh_units_temperature_value(uint8_t unit,double celsius){return unit==MESH_TEMP_FAHRENHEIT?celsius*1.8+32:celsius;}
void mesh_units_temperature(uint8_t unit,double celsius,char *out,size_t size){
 if(!out||!size)return;
 if(!isfinite(celsius)){snprintf(out,size,"Unavailable");return;}
 snprintf(out,size,"%.1f %s",mesh_units_temperature_value(unit,celsius),unit==MESH_TEMP_FAHRENHEIT?"F":"C");
}
esp_err_t mesh_units_load_all(uint8_t *distance,uint8_t *temperature){
 if(!distance||!temperature)return ESP_ERR_INVALID_ARG;
 nvs_handle_t h;esp_err_t result=nvs_open("meshdeck_mui",NVS_READONLY,&h);if(result!=ESP_OK)return result;
 uint8_t b[3];size_t n=sizeof(b);result=nvs_get_blob(h,"units_v2",b,&n);nvs_close(h);
 if(result==ESP_ERR_NVS_NOT_FOUND){uint8_t legacy=0;result=mesh_units_load(&legacy);if(result==ESP_OK){*distance=legacy;*temperature=MESH_TEMP_CELSIUS;}return result;}
 if(result!=ESP_OK)return result;
 if(n!=3||b[0]!=2||b[1]>1||b[2]>1)return ESP_ERR_INVALID_ARG;
 *distance=b[1];*temperature=b[2];return ESP_OK;
}
esp_err_t mesh_units_save_all(uint8_t distance,uint8_t temperature){
 if(distance>1||temperature>1)return ESP_ERR_INVALID_ARG;
 nvs_handle_t h;esp_err_t result=nvs_open("meshdeck_mui",NVS_READWRITE,&h);if(result!=ESP_OK)return result;
 uint8_t b[]={2,distance,temperature};result=nvs_set_blob(h,"units_v2",b,sizeof(b));if(result==ESP_OK)result=nvs_commit(h);nvs_close(h);return result;
}
