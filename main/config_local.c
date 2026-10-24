#include "config_backup.h"
#include "nvs.h"
#include "display_settings.h"
#include "sound_settings.h"
#include "notification_time.h"
#include <string.h>
#define APP_SIZE (4+8*33)
static const char *keys[]={
    "display_v1","power_v1","sound_v1","text_size_v1","time_v1","channel_alerts","units_v1","units_v2"
}
;
static bool idle_valid(unsigned x,bool screen){
    return !x||x==15||x==30||x==45||x==60||(screen&&(x==120||x==300||x==900));
}
static bool blob_valid(unsigned i,const uint8_t *p,size_t n){
    if(!n)return true;
    if(i==0){
        mesh_display_settings_t d;
        if(n==8&&p[0]==3)d=(mesh_display_settings_t){
            p[1],p[2],p[3]|p[4]<<8,p[5],p[6],p[7]
        }
        ;
        else if(n==7&&(p[0]==1||p[0]==2))d=(mesh_display_settings_t){
            p[1],p[2],p[3]*60,p[4],p[5],p[6]
        }
        ;
        else return false;
        return mesh_display_valid(&d);
    }
    if(i==1){
        if(n==5&&p[0]==1)return p[1]<=1&&p[2]<=1&&p[3]<=1&&p[4]<=1;
        return n==7&&p[0]==2&&p[1]<=1&&idle_valid(p[2]|p[3]<<8,false)&&idle_valid(p[4]|p[5]<<8,false)&&p[6]<=1;
    }
    if(i==2)return n==7&&p[0]==1&&p[1]<=1&&p[2]<=1&&p[3]<=100&&p[4]<=1&&p[5]<24&&p[6]<24;
    if(i==3)return n==2&&p[0]==1&&p[1]<=2;
    if(i==4){
        mesh_time_settings_t t={
            p[1],p[2],(int16_t)(p[3]|p[4]<<8)
        }
        ;
        return n==5&&p[0]==1&&mesh_time_valid(&t);
    }
    if(i==5)return n==2&&p[0]==1;
    if(i==6)return n==2&&p[0]==1&&p[1]<=1;
    return i==7&&n==3&&p[0]==2&&p[1]<=1&&p[2]<=1;
}
static bool app_valid(const uint8_t *data,size_t size){
    if(!data||size!=APP_SIZE||memcmp(data,"APP1",4))return false;
    for(unsigned i=0;i<8;i++){
        const uint8_t *b=data+4+i*33;
        if(b[0]>32||!blob_valid(i,b+1,b[0]))return false;
    }
    return true;
}
bool mesh_config_app_capture(uint8_t *out,size_t *size){
    if(!out||!size||*size<APP_SIZE)return false;
    memset(out,0,APP_SIZE);
    memcpy(out,"APP1",4);
    nvs_handle_t h;
    esp_err_t err=nvs_open("meshdeck_mui",NVS_READONLY,&h);
    if(err!=ESP_OK&&err!=ESP_ERR_NVS_NOT_FOUND)return false;
    if(err==ESP_OK){
        for(unsigned i=0;i<8;i++){
            uint8_t *b=out+4+i*33;
            size_t n=32;
            err=nvs_get_blob(h,keys[i],b+1,&n);
            if(err!=ESP_OK&&err!=ESP_ERR_NVS_NOT_FOUND){
                nvs_close(h);
                return false;
            }
            b[0]=err==ESP_OK?n:0;
        }
        nvs_close(h);
    }
    *size=APP_SIZE;
    return app_valid(out,*size);
}
bool mesh_config_app_apply(const uint8_t *data,size_t size){
    if(!app_valid(data,size))return false;
    nvs_handle_t h;
    if(nvs_open("meshdeck_mui",NVS_READWRITE,&h)!=ESP_OK)return false;
    bool ok=true;
    for(unsigned i=0;ok&&i<8;i++){
        const uint8_t *b=data+4+i*33;
        esp_err_t err=b[0]?nvs_set_blob(h,keys[i],b+1,b[0]):nvs_erase_key(h,keys[i]);
        ok=err==ESP_OK||(!b[0]&&err==ESP_ERR_NVS_NOT_FOUND);
    }
    if(ok)ok=nvs_commit(h)==ESP_OK;
    nvs_close(h);
    uint8_t check[APP_SIZE];
    size_t n=sizeof(check);
    return ok&&mesh_config_app_capture(check,&n)&&n==size&&!memcmp(data,check,size);
}
