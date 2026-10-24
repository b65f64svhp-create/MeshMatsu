#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <string.h>
#include "../main/notification_time.c"
// Clock formatting never needs storage; fail loudly if it attempts any access.
esp_err_t nvs_open(const char *s,int mode,nvs_handle_t *h){assert(false);return ESP_ERR_INVALID_ARG;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *s,void *p,size_t *size){assert(false);return ESP_ERR_INVALID_ARG;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *s,const void *p,size_t size){assert(false);return ESP_ERR_INVALID_ARG;}
esp_err_t nvs_commit(nvs_handle_t h){assert(false);return ESP_ERR_INVALID_ARG;}
void nvs_close(nvs_handle_t h){assert(false);}
static mesh_time_settings_t time_settings;
static size_t logical_h_res=800;
static int fb,font,*pax_font_sky_mono=&font;
#define UI_GREEN 1
#define UI_MUTED 2
static time_t fixture_now;
static time_t fixture_time(time_t *out){if(out)*out=fixture_now;return fixture_now;}
typedef struct {float x,y;} pax_vec2f;
static pax_vec2f pax_text_size(int *font,int size,const char *text){return (pax_vec2f){strlen(text)*size*0.62f,size};}
static unsigned draws;
static char rendered_clock[16],rendered_date[24];
static void pax_draw_text(int *buffer,unsigned color,int *font,int size,float x,float y,const char *text){
 assert(x>=304 && x+pax_text_size(font,size,text).x<=580);
 if(color==UI_GREEN)snprintf(rendered_clock,sizeof(rendered_clock),"%s",text);
 else snprintf(rendered_date,sizeof(rendered_date),"%s",text);
 draws++;
}
#define time fixture_time
#include "../main/home_clock.inc"
#undef time
static void expect(uint32_t stamp,const char *clock,const char *date){
 char c[16],d[24];assert(mesh_time_clock(&time_settings,stamp,c,sizeof(c),d,sizeof(d)));
 assert(!strcmp(c,clock)&&!strcmp(d,date));
}
int main(void){
 time_settings=mesh_time_defaults();mesh_time_apply(&time_settings);
 expect(1768478400U,"05:00am","JAN-15");expect(1784116800U,"06:00am","JUL-15");
 expect(1768460400U,"12:00am","JAN-15");
 time_settings.use_24h=1;expect(1768460400U,"00:00","JAN-15");
 time_settings.zone=7;mesh_time_apply(&time_settings);
 expect(1768435199U,"23:59","JAN-14");expect(1768435200U,"00:00","JAN-15");
 time_settings.zone=16;time_settings.offset_minutes=345;mesh_time_apply(&time_settings);expect(1768478400U,"17:45","JAN-15");
 time_settings.zone=13;mesh_time_apply(&time_settings);expect(1768478400U,"01:00","JAN-16");
 time_settings=mesh_time_defaults();mesh_time_apply(&time_settings);
 fixture_now=1768478400;assert(home_clock_needs_draw());draw_home_clock();
 assert(draws==2&&!strcmp(rendered_clock,"05:00am")&&!strcmp(rendered_date,"JAN-15"));
 assert(!home_clock_needs_draw());fixture_now+=59;assert(!home_clock_needs_draw());
 fixture_now++;assert(home_clock_needs_draw());draw_home_clock();assert(!home_clock_needs_draw()&&!strcmp(rendered_clock,"05:01am"));
 fixture_now-=120;assert(home_clock_needs_draw());draw_home_clock();assert(!strcmp(rendered_clock,"04:59am"));
 fixture_now=0;assert(home_clock_needs_draw());draw_home_clock();
 assert(!strcmp(rendered_clock,"--:--")&&!strcmp(rendered_date,"Time not set"));
 fixture_now=60;assert(!home_clock_needs_draw());
 fixture_now=1768478400;assert(home_clock_needs_draw());
 char c[16],d[24];assert(!mesh_time_clock(&time_settings,0,c,sizeof(c),d,sizeof(d)));
 time_settings.zone=255;assert(!mesh_time_clock(&time_settings,1768478400,c,sizeof(c),d,sizeof(d)));
 assert(!mesh_time_clock(NULL,1768478400,c,sizeof(c),d,sizeof(d)));
 assert(!mesh_time_clock(&time_settings,0,NULL,0,d,sizeof(d)));
 char tiny[2]={'x','y'};time_settings=mesh_time_defaults();mesh_time_apply(&time_settings);
 assert(mesh_time_clock(&time_settings,1768478400,tiny,1,d,sizeof(d))&&tiny[0]==0&&tiny[1]=='y');
 puts("PASS: Home clock uses timezone/DST/12/24h, date rollover, unavailable time, output bounds, minute changes, backward/forward clock corrections and layout bounds without storage");
}
