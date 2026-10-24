"""Reproduce ESP-Hosted owning SDMMC before the production map mount runs."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'main/map_tiles.c').read_text()
mount=s[s.index('#ifdef ESP_PLATFORM\nstatic bool map_storage_error'):s.index('static void render(')]
harness=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ESP_PLATFORM 1
#define ESP_OK 0
#define ESP_ERR_NO_MEM 1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_TIMEOUT 3
#define ESP_ERR_NOT_FOUND 4
#define BSP_INPUT_ACTION_TYPE_SD_CARD 1
#define BSP_SDCARD_WIDTH 4
#define BSP_SDCARD_CLK 43
#define BSP_SDCARD_CMD 44
#define BSP_SDCARD_D0 39
#define BSP_SDCARD_D1 40
#define BSP_SDCARD_D2 41
#define BSP_SDCARD_D3 42
#define BSP_SDCARD_FLAGS 0
#define SDMMC_HOST_SLOT_0 0
#define SDMMC_FREQ_DEFAULT 20000
#define MALLOC_CAP_DMA 1
#define MALLOC_CAP_INTERNAL 2
#define GPIO_MODE_OUTPUT_OD 1
#define GPIO_MODE_INPUT 2
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define CONFIG_ESP_HOSTED_SDIO_SLOT 1
#define SD_IO_RW_DIRECT 52
#define SCF_CMD_AC 1
#define SCF_RSP_R5 2
#define ESP_LOGW(...) ((void)0)
#define pdMS_TO_TICKS(n) (n)
typedef int esp_err_t;
typedef void *sd_pwr_ctrl_handle_t;
typedef int sdmmc_card_t;
typedef struct {int ldo_chan_id;} sd_pwr_ctrl_ldo_config_t;
typedef struct {uint64_t pin_bit_mask;int mode,pull_up_en,pull_down_en;} gpio_config_t;
typedef struct {int opcode,arg,flags,timeout_ms;} sdmmc_command_t;
static unsigned host_inits,slot_deinits,transactions,mounts,power_creations,power_changes,archive_opens;
static bool controller_owned=CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE,inserted=true,file_present=true;
static int detection_error,mount_error,transaction_error;
static int full_host_init(void){host_inits++;if(controller_owned)return ESP_ERR_INVALID_STATE;controller_owned=true;return ESP_OK;}
static int remove_slot(int slot){assert(slot==0);slot_deinits++;return 0;}
static int sdmmc_host_deinit_slot(int slot){return remove_slot(slot);}
static int sdmmc_host_do_transaction(int slot,sdmmc_command_t *cmd){assert(slot==1&&cmd->opcode==52&&cmd->timeout_ms==1000);transactions++;return transaction_error;}
typedef struct {int slot,max_freq_khz;void *pwr_ctrl_handle,*dma_aligned_buffer;int (*init)(void);int (*deinit_p)(int);} sdmmc_host_t;
typedef struct {int width,clk,cmd,d0,d1,d2,d3,flags;} sdmmc_slot_config_t;
typedef struct {bool format_if_mount_failed;int max_files;} esp_vfs_fat_mount_config_t;
#define SDMMC_HOST_DEFAULT() (sdmmc_host_t){.init=full_host_init,.deinit_p=remove_slot}
#define SDMMC_SLOT_CONFIG_DEFAULT() (sdmmc_slot_config_t){0}
#include "map_sd_host.h"
#include "map_archive.h"
static mesh_map_archive_t archive;
static sdmmc_card_t *sd_card;
static sd_pwr_ctrl_handle_t sd_power;
static void *sd_dma_buffer;
static bool sd_mounted;
static char load_error[80];
static const char *esp_err_to_name(int e){return e==ESP_ERR_TIMEOUT?"ESP_ERR_TIMEOUT":"ESP_ERROR";}
static int bsp_input_read_action(int action,bool *out){*out=inserted;return detection_error;}
static int sd_pwr_ctrl_new_on_chip_ldo(const sd_pwr_ctrl_ldo_config_t *cfg,void **handle){assert(cfg->ldo_chan_id==4);*handle=(void*)1;power_creations++;return 0;}
static void *heap_caps_aligned_alloc(size_t align,size_t bytes,int caps){static char memory[2048];assert(align==64&&bytes==2048);return memory;}
static int gpio_config(const gpio_config_t *cfg){assert(cfg->pin_bit_mask==((1ULL<<39)|(1ULL<<40)|(1ULL<<41)|(1ULL<<42)|(1ULL<<43)|(1ULL<<44)));return 0;}
static void gpio_set_level(int pin,int value){assert(pin>=39&&pin<=44&&value==0);}
static int sd_pwr_ctrl_set_io_voltage(void *handle,int voltage){assert(voltage==0||voltage==3300);power_changes++;return 0;}
static void vTaskDelay(int ms){assert(ms==150);}
static int esp_vfs_fat_sdmmc_mount(const char *path,const sdmmc_host_t *host,const sdmmc_slot_config_t *slot,const esp_vfs_fat_mount_config_t *config,sdmmc_card_t **card){
 assert(!strcmp(path,"/sdcard")&&host->slot==0&&host->pwr_ctrl_handle&&host->dma_aligned_buffer);
 assert(!config->format_if_mount_failed&&slot->clk==43&&slot->cmd==44&&slot->d0==39&&slot->d3==42&&slot->width==4);
 mounts++;int error=host->init();if(error)return error;
 if(mount_error){host->deinit_p(0);return mount_error;}*card=(void*)1;return 0;
}
bool mesh_map_archive_open(mesh_map_archive_t *out,const char *path){assert(!strcmp(path,"/sdcard/maps/meshmatsu.mmap"));archive_opens++;if(file_present)out->file=(FILE*)1;return file_present;}
'''+mount+r'''
int main(void){
 // Old SDMMC_HOST_DEFAULT initialization fails after ESP-Hosted claimed it.
 if(CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE){assert(full_host_init()==ESP_ERR_INVALID_STATE);host_inits=0;}
 assert(open_archive()&&sd_mounted&&archive.file&&mounts==1&&power_changes==2);
 assert(host_inits==(CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE?0:1));
 assert(open_archive()&&mounts==1&&archive_opens==1);
 archive.file=NULL;file_present=false;assert(!open_archive()&&strstr(load_error,"Map file unavailable"));
 file_present=true;assert(open_archive()&&mounts==1);
 // A failed mount cleans only slot 0, including when the radio doesn't answer SDIO.
 archive.file=NULL;sd_mounted=false;mount_error=ESP_ERR_TIMEOUT;transaction_error=ESP_ERR_TIMEOUT;
 if(!CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE)controller_owned=false;
 assert(!open_archive()&&strstr(load_error,"SD mount failed: ESP_ERR_TIMEOUT")&&strstr(load_error,"R: retry"));
 assert(slot_deinits==1&&transactions==(CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE?1:0));
 mount_error=0;if(!CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE)controller_owned=false;
 assert(open_archive());
 archive.file=NULL;sd_mounted=false;inserted=false;unsigned before=mounts;
 assert(!open_archive()&&strstr(load_error,"SD card not detected")&&mounts==before);
 inserted=true;detection_error=1;assert(!open_archive()&&strstr(load_error,"SD detection failed")&&mounts==before);
 assert(power_creations==1);
 puts("PASS: production SD mount reuses shared host, independent-host fallback, slot-only cleanup, radio timeout, card/file/mount errors, retry and no formatting");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'driver').mkdir()
 for name in ['driver/sdmmc_host.h','sdmmc_cmd.h','sd_protocol_defs.h']:(p/name).write_text('#pragma once\n')
 (p/'test.c').write_text(harness)
 for shared in [0,1]:
  subprocess.run(['clang','-std=c11','-fsanitize=address,undefined',f'-DCONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE={shared}','-I',str(p),'-I',str(root/'main'),str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
