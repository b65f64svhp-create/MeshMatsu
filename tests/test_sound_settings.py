"""Exercise sound persistence, invalid storage, and local quiet-hour boundaries."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
harness = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sound_settings.h"
#include "nvs.h"
static uint8_t stored[8], pending[8];
static size_t stored_size;
static bool exists;
static int open_error, get_error, set_error, commit_error;
static unsigned opens, get_calls, sets, commits, closes;
int nvs_open(const char *ns,int mode,nvs_handle_t *h) {
 assert(!strcmp(ns,"meshdeck_mui"));opens++;*h=1;
 return open_error ? open_error : (!exists && mode==NVS_READONLY ? ESP_ERR_NVS_NOT_FOUND : ESP_OK);
}
int nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size) {
 assert(h==1&&!strcmp(key,"sound_v1"));get_calls++;
 if(get_error)return get_error;
 assert(stored_size<=*size);memcpy(out,stored,stored_size);*size=stored_size;return ESP_OK;
}
int nvs_set_blob(nvs_handle_t h,const char *key,const void *in,size_t size) {
 assert(h==1&&!strcmp(key,"sound_v1")&&size==7);sets++;
 if(set_error)return set_error;
 memcpy(pending,in,size);return ESP_OK;
}
int nvs_commit(nvs_handle_t h) {
 assert(h==1);commits++;if(commit_error)return commit_error;
 memcpy(stored,pending,7);stored_size=7;exists=true;return ESP_OK;
}
void nvs_close(nvs_handle_t h){assert(h==1);closes++;}
static time_t utc(unsigned hour,unsigned minute,unsigned second) {
 return (time_t)1792281600 + hour*3600+minute*60+second; // 2026-10-18 UTC.
}
int main(void) {
 setenv("TZ","UTC0",1);tzset();
 mesh_sound_settings_t settings=mesh_sound_defaults();
 assert(!settings.message_sounds&&!settings.game_sounds&&settings.volume==35&&!settings.quiet_enabled);
 assert(settings.quiet_start_hour==22&&settings.quiet_end_hour==7&&mesh_sound_valid(&settings));
 assert(!mesh_sound_valid(NULL)&&mesh_sound_load(NULL)==ESP_ERR_INVALID_ARG&&mesh_sound_save(NULL)==ESP_ERR_INVALID_ARG);
 mesh_sound_settings_t unchanged=settings;
 assert(mesh_sound_load(&settings)==ESP_ERR_NVS_NOT_FOUND&&!memcmp(&settings,&unchanged,sizeof(settings)));
 assert(!mesh_sound_allowed(&settings,false,utc(12,0,0))&&!mesh_sound_allowed(&settings,true,utc(12,0,0)));
 for(unsigned volume=0;volume<=100;volume++)for(unsigned bits=0;bits<8;bits++) {
  mesh_sound_settings_t value={bits&1,(bits>>1)&1,volume,(bits>>2)&1,22,7};
  assert(mesh_sound_save(&value)==ESP_OK);
  mesh_sound_settings_t loaded={0};assert(mesh_sound_load(&loaded)==ESP_OK&&!memcmp(&loaded,&value,sizeof(value)));
  assert(mesh_sound_allowed(&value,false,utc(12,0,0))==(bool)(volume&&value.message_sounds));
  assert(mesh_sound_allowed(&value,true,utc(12,0,0))==(bool)(volume&&value.game_sounds));
 }
 for(unsigned i=0;i<7;i++) {
  uint8_t old=stored[i];stored[i]=255;settings=unchanged;
  assert(mesh_sound_load(&settings)==ESP_ERR_INVALID_ARG&&!memcmp(&settings,&unchanged,sizeof(settings)));stored[i]=old;
 }
 stored_size=6;assert(mesh_sound_load(&settings)==ESP_ERR_INVALID_ARG);stored_size=7;
 for(unsigned i=0;i<sizeof(settings);i++) {
  settings=unchanged;((uint8_t *)&settings)[i]=255;unsigned before=opens;
  assert(!mesh_sound_valid(&settings)&&mesh_sound_save(&settings)==ESP_ERR_INVALID_ARG&&opens==before);
 }
 settings=unchanged;settings.message_sounds=settings.game_sounds=settings.quiet_enabled=1;
 assert(!mesh_sound_quiet_at(&settings,utc(21,59,59)));
 assert(mesh_sound_quiet_at(&settings,utc(22,0,0))&&mesh_sound_quiet_at(&settings,utc(23,59,59)));
 assert(mesh_sound_quiet_at(&settings,utc(0,0,0))&&mesh_sound_quiet_at(&settings,utc(6,59,59)));
 assert(!mesh_sound_quiet_at(&settings,utc(7,0,0)));
 assert(mesh_sound_quiet_at(&settings,0));assert(mesh_sound_quiet_at(&settings,1609459200)); // Unset clock stays quiet when enabled.
 assert(!mesh_sound_allowed(&settings,false,utc(22,0,0))&&!mesh_sound_allowed(&settings,true,utc(22,0,0)));
 settings.quiet_start_hour=9;settings.quiet_end_hour=17;
 assert(!mesh_sound_quiet_at(&settings,utc(8,59,59))&&mesh_sound_quiet_at(&settings,utc(9,0,0)));
 assert(mesh_sound_quiet_at(&settings,utc(16,59,59))&&!mesh_sound_quiet_at(&settings,utc(17,0,0)));
 settings.quiet_end_hour=9;assert(mesh_sound_quiet_at(&settings,utc(0,0,0))&&mesh_sound_quiet_at(&settings,utc(23,59,59)));
 settings.quiet_enabled=0;assert(!mesh_sound_quiet_at(&settings,utc(9,0,0))&&!mesh_sound_quiet_at(&settings,0));
 // Existing timezone controls apply without a separate sound timezone.
 settings.quiet_enabled=1;settings.quiet_start_hour=22;settings.quiet_end_hour=7;
 setenv("TZ","MST7MDT,M3.2.0,M11.1.0",1);tzset();
 assert(mesh_sound_quiet_at(&settings,utc(12,59,59))&&!mesh_sound_quiet_at(&settings,utc(13,0,0)));
 assert(!mesh_sound_quiet_at(&settings,utc(3,59,59))&&mesh_sound_quiet_at(&settings,utc(4,0,0)));
 // Storage failures are surfaced and an unsuccessful save leaves durable data alone.
 uint8_t before[7];memcpy(before,stored,7);
 open_error=9;unsigned before_gets=get_calls;assert(mesh_sound_load(&settings)==9&&get_calls==before_gets);open_error=0;
 get_error=10;unsigned before_closes=closes;assert(mesh_sound_load(&settings)==10&&closes==before_closes+1);get_error=0;
 set_error=11;unsigned before_commits=commits;assert(mesh_sound_save(&settings)==11&&commits==before_commits&&!memcmp(before,stored,7));set_error=0;
 commit_error=12;assert(mesh_sound_save(&settings)==12&&!memcmp(before,stored,7));
 puts("PASS: silent defaults, saved sound switches/volume/hours, corrupt-state rejection, save failures, quiet boundaries/overnight/local timezone and unknown clock");
}
'''
with tempfile.TemporaryDirectory(prefix="meshmatsu-sound-settings-") as folder:
    folder = Path(folder)
    (folder / "test.c").write_text(harness)
    subprocess.run(["clang", "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-fsanitize=address,undefined",
                    "-I", str(root / "tests/stubs"), "-I", str(root / "main"),
                    str(root / "main/sound_settings.c"), str(folder / "test.c"),
                    "-o", str(folder / "test")], check=True)
    subprocess.run([str(folder / "test")], check=True)
