#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "first_use.c"
static uint8_t durable[8],pending[8];static size_t stored,pending_n;static int fail_open,fail_read,fail_write,fail_commit;static unsigned closes;
esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h){assert(!strcmp(ns,"meshdeck_mui"));*h=1;return fail_open;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *n){assert(!strcmp(key,"setup_v1"));if(fail_read)return fail_read;if(!stored)return ESP_ERR_NVS_NOT_FOUND;if(stored>*n)return ESP_ERR_INVALID_ARG;memcpy(out,durable,stored);*n=stored;return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *in,size_t n){assert(!strcmp(key,"setup_v1")&&n==2);if(fail_write)return fail_write;memcpy(pending,in,n);pending_n=n;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){if(fail_commit)return fail_commit;memcpy(durable,pending,pending_n);stored=pending_n;return ESP_OK;}
void nvs_close(nvs_handle_t h){closes++;}
int main(void){
 bool completed=true;assert(mesh_first_use_load(NULL)==ESP_ERR_INVALID_ARG);assert(mesh_first_use_load(&completed)==ESP_ERR_NVS_NOT_FOUND&&!completed);
 fail_open=9;completed=true;assert(mesh_first_use_load(&completed)==9&&!completed&&mesh_first_use_complete()==9);fail_open=0;
 fail_write=8;assert(mesh_first_use_complete()==8&&!stored);fail_write=0;fail_commit=7;assert(mesh_first_use_complete()==7&&!stored);fail_commit=0;
 assert(mesh_first_use_complete()==ESP_OK&&mesh_first_use_load(&completed)==ESP_OK&&completed);
 durable[0]=99;assert(mesh_first_use_load(&completed)==ESP_ERR_INVALID_ARG&&!completed);durable[0]=1;durable[1]=0;assert(mesh_first_use_load(&completed)==ESP_ERR_INVALID_ARG&&!completed);durable[1]=1;stored=1;assert(mesh_first_use_load(&completed)==ESP_ERR_INVALID_ARG&&!completed);stored=3;assert(mesh_first_use_load(&completed)==ESP_ERR_INVALID_ARG&&!completed);
 fail_read=6;assert(mesh_first_use_load(&completed)==6&&!completed);assert(closes>8);
 puts("PASS: missing, malformed and failed first-use preferences remain incomplete; completion persists only on successful commit");return 0;
}
