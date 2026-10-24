#include "config_backup.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *keys[]={"display_v1","power_v1","sound_v1","text_size_v1","time_v1","channel_alerts","units_v1","units_v2"};
static uint8_t data[8][32];static size_t sizes[8];static bool fail_write,fail_commit;
static unsigned key(const char *k){for(unsigned i=0;i<8;i++)if(!strcmp(keys[i],k))return i;assert(false);return 0;}
esp_err_t nvs_open(const char *s,int mode,nvs_handle_t *h){assert(!strcmp(s,"meshdeck_mui"));*h=1;return 0;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *k,void *out,size_t *size){unsigned i=key(k);if(!sizes[i])return ESP_ERR_NVS_NOT_FOUND;if(*size<sizes[i])return ESP_ERR_INVALID_ARG;memcpy(out,data[i],sizes[i]);*size=sizes[i];return 0;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *k,const void *p,size_t n){if(fail_write)return 1;unsigned i=key(k);assert(n<=32);memcpy(data[i],p,n);sizes[i]=n;return 0;}
esp_err_t nvs_erase_key(nvs_handle_t h,const char *k){sizes[key(k)]=0;return 0;}
esp_err_t nvs_commit(nvs_handle_t h){return fail_commit?1:0;}void nvs_close(nvs_handle_t h){}
int main(void){
 uint8_t empty[300],saved[300],check[300];size_t n=sizeof(empty);assert(mesh_config_app_capture(empty,&n));
 uint8_t display[]={3,50,60,120,0,1,20,3},power[]={2,0,30,0,15,0,1},sound[]={1,1,1,50,1,22,7},time[]={1,0,1,0,0};
 assert(!nvs_set_blob(1,keys[0],display,sizeof(display))&&!nvs_set_blob(1,keys[1],power,sizeof(power))&&!nvs_set_blob(1,keys[2],sound,sizeof(sound))&&!nvs_set_blob(1,keys[4],time,sizeof(time)));
 size_t sn=sizeof(saved);assert(mesh_config_app_capture(saved,&sn));assert(mesh_config_app_apply(empty,n)&&!sizes[0]&&!sizes[4]);assert(mesh_config_app_apply(saved,sn));size_t cn=sizeof(check);assert(mesh_config_app_capture(check,&cn)&&cn==sn&&!memcmp(check,saved,sn));
 saved[4+1+1]=0;assert(!mesh_config_app_apply(saved,sn));saved[4+1+1]=50;
 fail_write=true;assert(!mesh_config_app_apply(saved,sn));fail_write=false;fail_commit=true;assert(!mesh_config_app_apply(saved,sn));
 assert(!mesh_config_app_apply(saved,sn-1));
 puts("PASS: app display v3/power/sound/time capture, absent-key removal, restore/readback, invalid brightness/length, write and commit failure");
}
