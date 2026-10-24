"""Run the production sound worker against bounded queue/I2S hardware fakes."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
headers = {
    "nvs.h": (root / "tests/stubs/nvs.h").read_text(),
    "esp_timer.h": "#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\n",
    "esp_log.h": "#pragma once\n#define ESP_LOGW(...) ((void)0)\n",
    "esp_err.h": '''#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_NO_MEM 3
#define ESP_ERR_NVS_NOT_FOUND 0x1102
''',
    "freertos/FreeRTOS.h": '''#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
''',
    "freertos/queue.h": '''#pragma once
#include "freertos/FreeRTOS.h"
typedef struct fake_queue *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned, unsigned);
int xQueueSend(QueueHandle_t,const void *,unsigned);
int xQueueReceive(QueueHandle_t,void *,unsigned);
int xQueueReset(QueueHandle_t);
void vQueueDelete(QueueHandle_t);
''',
    "freertos/task.h": '''#pragma once
#include "freertos/FreeRTOS.h"
typedef void *TaskHandle_t;
int xTaskCreate(void (*)(void *),const char *,unsigned,void *,unsigned,TaskHandle_t *);
''',
    "driver/i2s_common.h": '''#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
typedef void *i2s_chan_handle_t;
typedef struct {bool is_enabled;uint32_t total_dma_buf_size;} i2s_chan_info_t;
esp_err_t i2s_channel_get_info(i2s_chan_handle_t,i2s_chan_info_t *);
esp_err_t i2s_channel_enable(i2s_chan_handle_t);
esp_err_t i2s_channel_disable(i2s_chan_handle_t);
esp_err_t i2s_channel_preload_data(i2s_chan_handle_t,const void *,size_t,size_t *);
esp_err_t i2s_channel_write(i2s_chan_handle_t,const void *,size_t,size_t *,uint32_t);
''',
    "bsp/audio.h": '''#pragma once
#include "driver/i2s_common.h"
esp_err_t bsp_audio_get_i2s_handle(i2s_chan_handle_t *);
esp_err_t bsp_audio_set_volume(float);
esp_err_t bsp_audio_set_amplifier(bool);
''',
}
harness = r'''
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "nvs.h"
#include "sound_player.h"
#include "driver/i2s_common.h"
#include "freertos/queue.h"
#include "freertos/task.h"
static int64_t fake_us;
static time_t wall_now=1792324800; // Noon UTC.
static time_t fake_time(time_t *out){if(out)*out=wall_now;return wall_now;}
int64_t esp_timer_get_time(void){return fake_us;}
#define time fake_time
#include "sound_player.c"
#undef time
struct fake_queue {unsigned head,count,capacity,item_size;unsigned char data[6][128];};
static jmp_buf empty_worker;
static bool no_queue, no_task, no_audio, enabled=true, amplifier;
static int fail_write, fail_amp, cancel_after_write;
static unsigned amp_on,amp_off,preloaded,writes,nonzero_samples;
static uint64_t wave_hash;
static int peak;
QueueHandle_t xQueueCreate(unsigned capacity,unsigned size){
 assert(capacity==6&&size<=128);if(no_queue)return NULL;
 QueueHandle_t queue=calloc(1,sizeof(*queue));assert(queue);queue->capacity=capacity;queue->item_size=size;return queue;
}
int xQueueSend(QueueHandle_t queue,const void *item,unsigned wait){
 assert(!wait);if(queue->count==queue->capacity)return 0;
 memcpy(queue->data[(queue->head+queue->count)%queue->capacity],item,queue->item_size);queue->count++;return pdTRUE;
}
int xQueueReceive(QueueHandle_t queue,void *item,unsigned wait){
 assert(wait==portMAX_DELAY);if(!queue->count)longjmp(empty_worker,1);
 memcpy(item,queue->data[queue->head],queue->item_size);queue->head=(queue->head+1)%queue->capacity;queue->count--;return pdTRUE;
}
int xQueueReset(QueueHandle_t queue){queue->head=queue->count=0;return pdTRUE;}
void vQueueDelete(QueueHandle_t queue){free(queue);}
int xTaskCreate(void (*worker)(void *),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *task){
 assert(worker==sound_worker&&!strcmp(name,"mesh_sound")&&stack>=4096&&!arg&&priority==2);
 if(no_task)return 0;*task=(void *)1;return pdPASS;
}
int bsp_audio_get_i2s_handle(i2s_chan_handle_t *out){*out=no_audio?NULL:(void *)1;return ESP_OK;}
int bsp_audio_set_volume(float volume){assert(volume==100);return ESP_OK;}
int bsp_audio_set_amplifier(bool on){
 if(fail_amp&&on)return 9;
 amplifier=on;if(on)amp_on++;else amp_off++;return ESP_OK;
}
int i2s_channel_get_info(i2s_chan_handle_t channel,i2s_chan_info_t *out){assert(channel==(void *)1);*out=(i2s_chan_info_t){enabled,5760};return ESP_OK;}
int i2s_channel_enable(i2s_chan_handle_t channel){assert(channel==(void *)1&&!enabled&&preloaded==5760);enabled=true;return ESP_OK;}
int i2s_channel_disable(i2s_chan_handle_t channel){assert(channel==(void *)1&&enabled);enabled=false;preloaded=0;return ESP_OK;}
int i2s_channel_preload_data(i2s_chan_handle_t channel,const void *data,size_t size,size_t *loaded){
 assert(channel==(void *)1&&!enabled&&size<=1024&&preloaded+size<=5760);
 const uint8_t *bytes=data;for(unsigned i=0;i<size;i++)assert(!bytes[i]);
 preloaded+=size;*loaded=size;return ESP_OK;
}
int i2s_channel_write(i2s_chan_handle_t channel,const void *data,size_t size,size_t *written,uint32_t timeout){
 assert(channel==(void *)1&&enabled&&amplifier&&size<=1024&&!(size%4)&&timeout<=60);writes++;
 const int16_t *samples=data;
 for(unsigned i=0;i<size/2;i+=2){
  assert(samples[i]==samples[i+1]);int magnitude=abs(samples[i]);if(magnitude>peak)peak=magnitude;
  if(samples[i])nonzero_samples++;
  wave_hash=(wave_hash*1099511628211ULL)^(uint16_t)samples[i];
 }
 fake_us+=(int64_t)(size/4)*1000000/44100;
 if(cancel_after_write&&writes==(unsigned)cancel_after_write)mesh_sound_stop();
 if(fail_write){*written=size/2;return 9;}
 *written=size;return ESP_OK;
}
int nvs_open(const char *a,int b,nvs_handle_t *c){return ESP_ERR_NVS_NOT_FOUND;}
int nvs_get_blob(nvs_handle_t a,const char *b,void *c,size_t *d){return ESP_ERR_NVS_NOT_FOUND;}
int nvs_set_blob(nvs_handle_t a,const char *b,const void *c,size_t d){return ESP_ERR_NVS_NOT_FOUND;}
int nvs_commit(nvs_handle_t a){return ESP_ERR_NVS_NOT_FOUND;}
void nvs_close(nvs_handle_t h){}
static void run_worker(void){if(!setjmp(empty_worker))sound_worker(NULL);}
static void reset_device(void){
 if(sound_queue)vQueueDelete(sound_queue);
 sound_queue=NULL;sound_task=NULL;audio_channel=NULL;audio_ready=false;sound_generation=0;
 no_queue=no_task=no_audio=false;enabled=true;amplifier=false;
 amp_on=amp_off=preloaded=writes=nonzero_samples=0;wave_hash=0;peak=0;
 fail_write=fail_amp=cancel_after_write=0;fake_us=0;wall_now=1792324800;
}
int main(void){
 setenv("TZ","UTC0",1);tzset();
 mesh_sound_settings_t settings=mesh_sound_defaults();
 assert(!mesh_sound_available()&&!mesh_sound_play(MESH_SOUND_DIRECT)&&!mesh_sound_test(35));
 assert(mesh_sound_init(NULL)==ESP_ERR_INVALID_ARG);
 no_audio=true;assert(mesh_sound_init(&settings)==ESP_ERR_INVALID_STATE&&!mesh_sound_available());reset_device();
 no_queue=true;assert(mesh_sound_init(&settings)==ESP_ERR_NO_MEM&&!mesh_sound_available()&&!amplifier&&!enabled);reset_device();
 no_task=true;assert(mesh_sound_init(&settings)==ESP_ERR_NO_MEM&&!mesh_sound_available()&&!sound_queue&&!amplifier&&!enabled);reset_device();
 assert(mesh_sound_init(&settings)==ESP_OK&&mesh_sound_available()&&!enabled&&!amplifier&&!amp_on);
 assert(!mesh_sound_play(MESH_SOUND_CHANNEL)&&!mesh_sound_play(MESH_SOUND_GAME_RELAY));
 assert(!mesh_sound_test(0)&&!mesh_sound_test(101)&&mesh_sound_test(35));
 run_worker();assert(amp_on==1&&!amplifier&&!enabled&&nonzero_samples&&peak>2000&&peak<=2868);
 // Distinct actual generated tones for each event, all on the same async worker.
 settings.message_sounds=settings.game_sounds=1;settings.volume=100;mesh_sound_apply(&settings);
 uint64_t hashes[5];
 for(unsigned event=0;event<5;event++){
  unsigned old_on=amp_on;peak=nonzero_samples=0;wave_hash=0;
  assert(mesh_sound_play((mesh_sound_event_t)event));assert(amp_on==old_on); // Enqueue does no I/O.
  run_worker();assert(amp_on==old_on+1&&!amplifier&&!enabled&&nonzero_samples&&peak<=8192);
  hashes[event]=wave_hash;for(unsigned before=0;before<event;before++)assert(hashes[event]!=hashes[before]);
 }
 assert(!mesh_sound_play((mesh_sound_event_t)-1)&&!mesh_sound_play((mesh_sound_event_t)5));
 // Bound a burst; accepting a full queue never blocks and rejects the seventh sound.
 for(unsigned i=0;i<6;i++)assert(mesh_sound_play(MESH_SOUND_CHANNEL));
 assert(sound_queue->count==6&&!mesh_sound_play(MESH_SOUND_DIRECT));
 mesh_sound_stop();assert(!sound_queue->count);unsigned old_on=amp_on;run_worker();assert(amp_on==old_on);
 // Saved switches and quiet hours are rechecked immediately before playback.
 assert(mesh_sound_play(MESH_SOUND_DIRECT));settings.message_sounds=0;mesh_sound_apply(&settings);run_worker();assert(amp_on==old_on);
 settings.message_sounds=1;settings.quiet_enabled=1;mesh_sound_apply(&settings);
 assert(mesh_sound_play(MESH_SOUND_DIRECT));wall_now=1792360800; // 22:00.
 run_worker();assert(amp_on==old_on&&!mesh_sound_play(MESH_SOUND_DIRECT)&&!mesh_sound_play(MESH_SOUND_GAME_DROP));
 assert(mesh_sound_test(35));run_worker();assert(amp_on==old_on+1); // Explicit preview during quiet hours.
 old_on=amp_on;wall_now=1792324800;assert(mesh_sound_play(MESH_SOUND_DIRECT));fake_us+=3000001;run_worker();assert(amp_on==old_on); // Stale queue entries expire.
 // An in-flight stop cuts off playback and leaves both hardware outputs idle.
 assert(mesh_sound_play(MESH_SOUND_GAME_DELIVERED));cancel_after_write=writes+1;unsigned before=writes;
 run_worker();assert(writes==before+1&&!amplifier&&!enabled&&mesh_sound_available());cancel_after_write=0;
 assert(mesh_sound_play(MESH_SOUND_DIRECT));fail_write=1;run_worker();
 assert(!mesh_sound_available()&&!amplifier&&!enabled&&!mesh_sound_play(MESH_SOUND_CHANNEL));
 // Failed amplifier activation also ends with a stopped clock and quiet amp.
 reset_device();assert(mesh_sound_init(&settings)==ESP_OK);fail_amp=1;
 assert(mesh_sound_play(MESH_SOUND_CHANNEL));run_worker();assert(!mesh_sound_available()&&!amplifier&&!enabled);
 reset_device();
 puts("PASS: silent startup, bounded nonblocking queue, distinct stereo tones and volume bounds, quiet/changed policy, preview, expiry/cancel, idle amplifier/I2S and graceful hardware failures");
}
'''
with tempfile.TemporaryDirectory(prefix="meshmatsu-sound-player-") as folder:
    folder = Path(folder)
    for relative, content in headers.items():
        path = folder / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    (folder / "test.c").write_text(harness)
    subprocess.run(["clang", "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-fsanitize=address,undefined",
                    "-I", str(folder), "-I", str(root / "tests/stubs"), "-I", str(root / "main"),
                    str(root / "main/sound_settings.c"), str(folder / "test.c"),
                    "-lm", "-o", str(folder / "test")], check=True)
    subprocess.run([str(folder / "test")], check=True)
