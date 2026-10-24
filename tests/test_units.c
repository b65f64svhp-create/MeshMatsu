#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "units.c"
static uint8_t stored[3],pending[3],legacy[2];static size_t length,pending_length,legacy_length;static int failure;static bool pending_v2;
int nvs_open(const char *ns,int mode,nvs_handle_t *h){assert(!strcmp(ns,"meshdeck_mui"));*h=1;return 0;}
int nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *n){bool v2=!strcmp(key,"units_v2");assert(v2||!strcmp(key,"units_v1"));size_t len=v2?length:legacy_length;if(!len)return ESP_ERR_NVS_NOT_FOUND;memcpy(out,v2?stored:legacy,len<*n?len:*n);*n=len;return 0;}
int nvs_set_blob(nvs_handle_t h,const char *key,const void *in,size_t n){pending_v2=!strcmp(key,"units_v2");assert((pending_v2&&n==3)||(!strcmp(key,"units_v1")&&n==2));memcpy(pending,in,n);pending_length=n;return 0;}
int nvs_commit(nvs_handle_t h){if(failure)return failure;memcpy(pending_v2?stored:legacy,pending,pending_length);if(pending_v2)length=pending_length;else legacy_length=pending_length;return 0;}
void nvs_close(nvs_handle_t h){}
static void check(uint8_t unit,double meters,const char *expected){char out[40];mesh_units_distance(unit,meters,out,sizeof(out));assert(!strcmp(out,expected));}
int main(void){
 check(0,0,"0 m");check(0,999,"999 m");check(0,1000,"1.00 km");check(0,5000,"5.00 km");
 check(1,0.3048,"1 ft");check(1,100,"328 ft");check(1,160.9344,"0.10 mi");check(1,1609.344,"1.00 mi");check(1,5000,"3.11 mi");check(1,NAN,"unavailable");check(0,-1,"unavailable");
 char tiny[2]={'x','y'};mesh_units_distance(1,100,tiny,1);assert(!tiny[0]&&tiny[1]=='y');mesh_units_distance(0,100,NULL,0);
 assert(!strcmp(mesh_units_precision(1,19),"Approx. 148 ft"));assert(!strcmp(mesh_units_precision(0,15),"Approx. 729 m"));assert(!strcmp(mesh_units_precision(1,0),"Off"));assert(!strcmp(mesh_units_precision(1,32),"Full precision"));
 uint8_t unit=0;assert(mesh_units_load(&unit)!=0&&unit==0);assert(!mesh_units_save(1)&&!mesh_units_load(&unit)&&unit==1);
 failure=7;assert(mesh_units_save(0)==7);unit=0;assert(!mesh_units_load(&unit)&&unit==1);failure=0;
 legacy[0]=99;unit=0;assert(mesh_units_load(&unit)!=0&&unit==0);legacy[0]=1;legacy[1]=2;assert(mesh_units_load(&unit)!=0);assert(mesh_units_save(2)!=0);assert(mesh_units_load(NULL)!=0);
 uint8_t temp=1;legacy[0]=1;legacy[1]=1;assert(!mesh_units_load_all(&unit,&temp)&&unit==1&&temp==0); // v47 migration
 assert(!mesh_units_save_all(0,1));unit=1;temp=0;assert(!mesh_units_load_all(&unit,&temp)&&!unit&&temp==1);
 assert(!mesh_units_save_all(1,0));assert(!mesh_units_load_all(&unit,&temp)&&unit==1&&!temp);
 failure=7;assert(mesh_units_save_all(0,1)==7);assert(!mesh_units_load_all(&unit,&temp)&&unit==1&&!temp);failure=0;
 stored[2]=2;unit=0;temp=1;assert(mesh_units_load_all(&unit,&temp)!=0&&!unit&&temp==1);stored[2]=0;
 assert(mesh_units_save_all(2,0)!=0&&mesh_units_save_all(0,2)!=0&&mesh_units_load_all(NULL,&temp)!=0);
 char t[40];mesh_units_temperature(0,-20,t,sizeof(t));assert(!strcmp(t,"-20.0 C"));mesh_units_temperature(1,-20,t,sizeof(t));assert(!strcmp(t,"-4.0 F"));mesh_units_temperature(1,0,t,sizeof(t));assert(!strcmp(t,"32.0 F"));mesh_units_temperature(0,NAN,t,sizeof(t));assert(!strcmp(t,"Unavailable"));
 assert(mesh_units_temperature_value(1,100)==212&&mesh_units_temperature_value(0,100)==100);
 puts("PASS: metric/imperial conversions/boundaries, invalid distances, bounded output, precision labels, persisted units and failed/corrupt saves");return 0;
}
