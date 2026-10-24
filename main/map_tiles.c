#include "map_tiles.h"
#include "map_archive.h"
#include "map_cache.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "sd_pwr_ctrl.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "driver/gpio.h"
#include "bsp/input.h"
#include "esp_log.h"
#include "map_sd_host.h"
#include "miniz.h"
#define MESH_MAP_WORKER_STACK (24 * 1024)
_Static_assert(MESH_MAP_WORKER_STACK >= sizeof(tinfl_decompressor) + 8192,
               "Map worker must fit ROM miniz workspace plus SD/filesystem call stack");
// Tanmatsu SD card pins, from badge-bsp targets/tanmatsu/tanmatsu_hardware.h.
#define BSP_SDCARD_WIDTH 4
#define BSP_SDCARD_CLK 43
#define BSP_SDCARD_CMD 44
#define BSP_SDCARD_D0 39
#define BSP_SDCARD_D1 40
#define BSP_SDCARD_D2 41
#define BSP_SDCARD_D3 42
#define BSP_SDCARD_FLAGS 0
#endif
#define MAX_WIDTH 800
#define MAX_HEIGHT 400
static mesh_map_archive_t archive;
static uint16_t *front, *back, *tile;
static uint8_t *compressed;
static uint16_t *cached_pixels;
static mesh_map_cache_t cache;
static uint32_t retry_generation, rendered_generation;
typedef struct { mesh_map_view_t view; int width, height; uint32_t generation; } map_request_t;
static map_request_t shown, requested;
static bool ready, initialized, request_valid, frame_unpainted;
static char result_status[80] = "Loading offline maps...";
static char load_error[80];
#ifdef ESP_PLATFORM
static QueueHandle_t requests;
static SemaphoreHandle_t lock;
static sdmmc_card_t *sd_card;
static sd_pwr_ctrl_handle_t sd_power;
static void *sd_dma_buffer;
static bool sd_mounted;
#define LOCK() xSemaphoreTake(lock, portMAX_DELAY)
#define UNLOCK() xSemaphoreGive(lock)
static void *map_alloc(size_t size) { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
#else
#define LOCK() ((void)0)
#define UNLOCK() ((void)0)
static void *map_alloc(size_t size) { return malloc(size); }
#endif
static bool same_request(const map_request_t *a, const map_request_t *b) {
    return a->generation == b->generation && a->width == b->width && a->height == b->height && a->view.zoom == b->view.zoom &&
        a->view.x == b->view.x && a->view.y == b->view.y;
}
#ifdef ESP_PLATFORM
static bool map_storage_error(const char *step, esp_err_t error) {
    snprintf(load_error, sizeof(load_error), "%s: %s / R: retry", step, esp_err_to_name(error));
    ESP_LOGW("map_tiles", "%s", load_error);
    return false;
}
#endif
#ifdef ESP_PLATFORM
#include <stdatomic.h>
static atomic_flag sd_mount_lock=ATOMIC_FLAG_INIT;
static bool mount_sd_unlocked(void) {
    if (!sd_mounted) {
        // SD power sequence follows badge-bsp's Tanmatsu storage driver (MIT).
        // The app uses UART for the C6; only the SD-card slot uses SDMMC.
        bool inserted = false;
        esp_err_t error = bsp_input_read_action(BSP_INPUT_ACTION_TYPE_SD_CARD, &inserted);
        if (error != ESP_OK) return map_storage_error("SD detection failed", error);
        if (!inserted) { snprintf(load_error, sizeof(load_error), "SD card not detected / R: retry"); return false; }
        if (!sd_power) {
            sd_pwr_ctrl_ldo_config_t power_config = {.ldo_chan_id = 4};
            error = sd_pwr_ctrl_new_on_chip_ldo(&power_config, &sd_power);
            if (error != ESP_OK) return map_storage_error("SD power failed", error);
        }
        if (!sd_dma_buffer) sd_dma_buffer = heap_caps_aligned_alloc(64, 2048, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        if (!sd_dma_buffer) return map_storage_error("SD buffer unavailable", ESP_ERR_NO_MEM);
        gpio_config_t pins = {.pin_bit_mask = (1ULL<<39)|(1ULL<<40)|(1ULL<<41)|(1ULL<<42)|(1ULL<<43)|(1ULL<<44),
            .mode = GPIO_MODE_OUTPUT_OD, .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE};
        error = gpio_config(&pins);
        if (error != ESP_OK) return map_storage_error("SD pins failed", error);
        for (int pin = 39; pin <= 44; pin++) gpio_set_level(pin, 0);
        error = sd_pwr_ctrl_set_io_voltage(sd_power, 0);
        if (error != ESP_OK) return map_storage_error("SD power reset failed", error);
        vTaskDelay(pdMS_TO_TICKS(150));
        pins.mode = GPIO_MODE_INPUT;
        error = gpio_config(&pins);
        if (error == ESP_OK) error = sd_pwr_ctrl_set_io_voltage(sd_power, 3300);
        if (error != ESP_OK) return map_storage_error("SD power on failed", error);
        vTaskDelay(pdMS_TO_TICKS(150));
        sdmmc_host_t host = SDMMC_HOST_DEFAULT(); host.slot = SDMMC_HOST_SLOT_0;
        host.max_freq_khz = SDMMC_FREQ_DEFAULT;
        host.pwr_ctrl_handle = sd_power; host.dma_aligned_buffer = sd_dma_buffer;
        mesh_map_configure_sd_host(&host);
        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = BSP_SDCARD_WIDTH; slot.clk = BSP_SDCARD_CLK; slot.cmd = BSP_SDCARD_CMD;
        slot.d0 = BSP_SDCARD_D0; slot.d1 = BSP_SDCARD_D1; slot.d2 = BSP_SDCARD_D2; slot.d3 = BSP_SDCARD_D3;
        slot.flags = BSP_SDCARD_FLAGS;
        const esp_vfs_fat_mount_config_t config = {.format_if_mount_failed = false, .max_files = 5};
        error = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &config, &sd_card);
        if (error != ESP_OK) return map_storage_error("SD mount failed", error);
        sd_mounted = true;
    }
    return true;
}
bool mesh_map_storage_mount(void) {
    while(atomic_flag_test_and_set_explicit(&sd_mount_lock,memory_order_acquire))vTaskDelay(1);
    bool ok=mount_sd_unlocked();
    atomic_flag_clear_explicit(&sd_mount_lock,memory_order_release);
    return ok;
}
#endif
static bool open_archive(void) {
    if (archive.file) return true;
    load_error[0] = '\0';
#ifdef ESP_PLATFORM
    if(!mesh_map_storage_mount())return false;
    if (mesh_map_archive_open(&archive, "/sdcard/maps/meshmatsu.mmap")) return true;
    snprintf(load_error, sizeof(load_error), "Map file unavailable / R: retry");
    return false;
#else
    const char *path = getenv("MESHMATSU_MAP");
    bool opened = path && mesh_map_archive_open(&archive, path);
    if (!opened) snprintf(load_error, sizeof(load_error), "Map file unavailable / R: retry");
    return opened;
#endif
}
static void render(map_request_t job) {
    if (job.generation != rendered_generation) {
        mesh_map_archive_close(&archive);
        mesh_map_cache_init(&cache, cached_pixels, MESH_MAP_CACHE_TILES);
        rendered_generation = job.generation;
    }
    bool available = open_archive();
    int missing = 0, total = 0;
    for (int y = 0; y < job.height; y++) for (int x = 0; x < job.width; x++)
        back[y * job.width + x] = ((x / 32 + y / 32) & 1) ? 0x18E4 : 0x2125;
    double world = 256.0 * (1u << job.view.zoom);
    int left = (int)floor(job.view.x * world - job.width / 2.0);
    int top = (int)floor(job.view.y * world - job.height / 2.0);
    int first_x = (int)floor(left / 256.0), first_y = (int)floor(top / 256.0);
    int side = 1 << job.view.zoom;
    mesh_map_cache_begin_view(&cache);
    // Protect overlapping tiles before loading newly exposed edges.
    for (int ty = first_y; ty * 256 < top + job.height; ty++)
        for (int tx = first_x; tx * 256 < left + job.width; tx++)
            if (ty >= 0 && ty < side) mesh_map_cache_pin(&cache, job.view.zoom,
                (unsigned)((tx % side + side) % side), (unsigned)ty);
    for (int ty = first_y; ty * 256 < top + job.height; ty++) {
        for (int tx = first_x; tx * 256 < left + job.width; tx++) {
            total++;
            const uint16_t *pixels = available && ty >= 0 && ty < side ?
                mesh_map_cache_tile(&cache, &archive, job.view.zoom,
                    (unsigned)((tx % side + side) % side), (unsigned)ty, compressed, tile) : NULL;
            if (!pixels) { missing++; continue; }
            int dx = tx * 256 - left, dy = ty * 256 - top;
            int sx = dx < 0 ? -dx : 0, sy = dy < 0 ? -dy : 0;
            int ex = job.width - dx < 256 ? job.width - dx : 256;
            int ey = job.height - dy < 256 ? job.height - dy : 256;
            for (int y = sy; y < ey; y++) memcpy(back + (dy+y)*job.width + dx+sx,
                pixels + y*256+sx, (size_t)(ex-sx)*2);
#ifdef ESP_PLATFORM
            vTaskDelay(1); // Keep radio/input polling ahead of bulk map work.
#endif
        }
    }
    LOCK();
    uint16_t *swap = front; front = back; back = swap;
    shown = job; ready = true; frame_unpainted = true;
    snprintf(result_status, sizeof(result_status), "%s", !available ? load_error :
        missing == total ? "No offline detail here at this zoom" : missing ? "Edge of offline coverage" : "Offline / Peace Country");
    UNLOCK();
}
#ifdef ESP_PLATFORM
static void worker(void *arg) {
    (void)arg; map_request_t job;
    while (true) if (xQueueReceive(requests, &job, portMAX_DELAY) == pdTRUE) render(job);
}
#endif
static bool initialize(void) {
    if (initialized) return true;
    if (!front) front = map_alloc(MAX_WIDTH * MAX_HEIGHT * 2);
    if (!back) back = map_alloc(MAX_WIDTH * MAX_HEIGHT * 2);
    if (!tile) tile = map_alloc(MESH_MAP_TILE_BYTES);
    if (!compressed) compressed = map_alloc(MESH_MAP_COMPRESSED_MAX);
    if (!front || !back || !tile || !compressed) return false;
    if (!cached_pixels) cached_pixels = map_alloc(MESH_MAP_CACHE_TILES * MESH_MAP_TILE_BYTES);
    // Map loading remains usable if optional cache allocation fails.
    mesh_map_cache_init(&cache, cached_pixels, MESH_MAP_CACHE_TILES);
#ifdef ESP_PLATFORM
    if (!lock) lock = xSemaphoreCreateMutex();
    if (!requests) requests = xQueueCreate(1, sizeof(map_request_t));
    if (!lock || !requests) return false;
    if (xTaskCreate(worker, "map_tiles", MESH_MAP_WORKER_STACK, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS) return false;
#endif
    initialized = true; return true;
}
void mesh_map_tiles_retry(void) {
    // Only the UI task changes request_valid; storage remains worker-owned.
    request_valid = false; retry_generation++;
}
void mesh_map_tiles_request(const mesh_map_view_t *view, int width, int height) {
    if (width <= 0 || height <= 0 || width > MAX_WIDTH || height > MAX_HEIGHT || !initialize()) return;
    map_request_t job = {*view, width, height, retry_generation};
    if (request_valid && same_request(&job, &requested)) return;
    requested = job; request_valid = true;
#ifdef ESP_PLATFORM
    xQueueOverwrite(requests, &job);
#else
    render(job);
#endif
}
bool mesh_map_tiles_needs_draw(void) {
    if (!initialized) return false;
    LOCK();
    bool pending = ready && frame_unpainted && request_valid && same_request(&shown, &requested);
    UNLOCK();
    return pending;
}
bool mesh_map_tiles_draw(pax_buf_t *fb, int x, int y, const mesh_map_view_t *view,
                         int width, int height, char *status, size_t status_size) {
    if (!initialized) { snprintf(status, status_size, "Map memory unavailable"); return false; }
    map_request_t job = {*view, width, height, retry_generation};
    LOCK();
    bool current = ready && same_request(&shown, &job);
    snprintf(status, status_size, "%s", current ? result_status : "Loading offline maps...");
    if (current) {
        pax_buf_t image;
        pax_buf_init(&image, front, width, height, PAX_BUF_16_565RGB);
        pax_draw_image_op(fb, &image, x, y);
        pax_buf_destroy(&image);
        frame_unpainted = false;
    }
    UNLOCK(); return current;
}
