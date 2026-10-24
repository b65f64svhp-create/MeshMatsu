#include "sound_player.h"
#include <math.h>
#include <string.h>
#include "bsp/audio.h"
#include "driver/i2s_common.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

enum { SAMPLE_RATE = 44100, CHUNK_FRAMES = 256, SOUND_QUEUE_LENGTH = 6 };
static const char *TAG = "sound";
typedef struct {
    mesh_sound_event_t event;
    uint8_t volume;
    bool preview;
    uint32_t generation;
    int64_t queued_us;
} sound_request_t;
typedef struct { uint16_t frequency, duration_ms; } sound_note_t;

static portMUX_TYPE sound_lock = portMUX_INITIALIZER_UNLOCKED;
static mesh_sound_settings_t current_settings;
static uint32_t sound_generation;
static bool audio_ready;
static QueueHandle_t sound_queue;
static TaskHandle_t sound_task;
static i2s_chan_handle_t audio_channel;
static uint32_t dma_bytes;

static bool sound_snapshot(mesh_sound_settings_t *settings, uint32_t *generation) {
    portENTER_CRITICAL(&sound_lock);
    if (settings) *settings = current_settings;
    if (generation) *generation = sound_generation;
    bool ready = audio_ready;
    portEXIT_CRITICAL(&sound_lock);
    return ready;
}

bool mesh_sound_available(void) { return sound_snapshot(NULL, NULL); }

void mesh_sound_apply(const mesh_sound_settings_t *settings) {
    if (!mesh_sound_valid(settings)) return;
    portENTER_CRITICAL(&sound_lock);
    current_settings = *settings;
    sound_generation++;
    portEXIT_CRITICAL(&sound_lock);
}

void mesh_sound_stop(void) {
    portENTER_CRITICAL(&sound_lock);
    sound_generation++;
    portEXIT_CRITICAL(&sound_lock);
    if (sound_queue) xQueueReset(sound_queue);
}

static bool request_allowed(const sound_request_t *request) {
    mesh_sound_settings_t settings;
    uint32_t generation;
    if (!sound_snapshot(&settings, &generation) || generation != request->generation) return false;
    if (request->preview) return request->volume > 0;
    return mesh_sound_allowed(&settings, request->event >= MESH_SOUND_GAME_RELAY, time(NULL));
}

static bool enqueue_sound(mesh_sound_event_t event, bool preview, uint8_t volume) {
    if ((unsigned)event > MESH_SOUND_GAME_DELIVERED || volume > 100 || !sound_queue) return false;
    mesh_sound_settings_t settings;
    sound_request_t request = {.event = event, .preview = preview, .queued_us = esp_timer_get_time()};
    if (!sound_snapshot(&settings, &request.generation)) return false;
    request.volume = preview ? volume : settings.volume;
    if (!request_allowed(&request)) return false;
    return xQueueSend(sound_queue, &request, 0) == pdTRUE;
}

bool mesh_sound_play(mesh_sound_event_t event) { return enqueue_sound(event, false, 0); }
bool mesh_sound_test(uint8_t volume) { return enqueue_sound(MESH_SOUND_DIRECT, true, volume); }

static bool write_pcm(const int16_t *samples, size_t size) {
    size_t written = 0;
    return i2s_channel_write(audio_channel, samples, size, &written, 60) == ESP_OK && written == size;
}

// Start/finish with silent DMA contents so a stopped channel cannot replay an
// earlier note when it is enabled again. Only this worker touches the channel.
static bool clear_dma(void) {
    const int16_t silence[CHUNK_FRAMES * 2] = {0};
    uint32_t remaining = dma_bytes;
    while (remaining) {
        size_t bytes = remaining < sizeof(silence) ? remaining : sizeof(silence);
        size_t loaded = 0;
        if (i2s_channel_preload_data(audio_channel, silence, bytes, &loaded) != ESP_OK || loaded != bytes) return false;
        remaining -= bytes;
    }
    return true;
}

static bool render_note(const sound_note_t *note, const sound_request_t *request) {
    int16_t pcm[CHUNK_FRAMES * 2];
    unsigned total = (unsigned)note->duration_ms * SAMPLE_RATE / 1000;
    unsigned ramp = SAMPLE_RATE / 200; // Five-millisecond attack and release.
    float phase = 0, step = 6.28318530718f * note->frequency / SAMPLE_RATE;
    float amplitude = 8192.0f * request->volume / 100.0f;
    for (unsigned offset = 0; offset < total;) {
        if (!request_allowed(request)) return true; // Cancellation, not an I/O failure.
        unsigned frames = total - offset;
        if (frames > CHUNK_FRAMES) frames = CHUNK_FRAMES;
        for (unsigned i = 0; i < frames; i++) {
            unsigned position = offset + i;
            unsigned envelope = position < ramp ? position : ramp;
            if (total - position < envelope) envelope = total - position;
            int16_t value = note->frequency ? (int16_t)(sinf(phase) * amplitude * envelope / ramp) : 0;
            pcm[i * 2] = pcm[i * 2 + 1] = value;
            phase += step;
            if (phase >= 6.28318530718f) phase -= 6.28318530718f;
        }
        if (!write_pcm(pcm, frames * 2 * sizeof(pcm[0]))) return false;
        offset += frames;
    }
    return true;
}

static bool play_request(const sound_request_t *request) {
    static const sound_note_t channel[] = {{784, 90}};
    static const sound_note_t direct[] = {{659, 80}, {0, 35}, {988, 110}};
    static const sound_note_t relay[] = {{1047, 55}, {1319, 75}};
    static const sound_note_t drop[] = {{392, 90}, {262, 120}};
    static const sound_note_t delivered[] = {{784, 75}, {988, 75}, {1319, 140}};
    static const sound_note_t *const sounds[] = {channel, direct, relay, drop, delivered};
    static const uint8_t counts[] = {1, 3, 2, 2, 3};
    if (!clear_dma() || i2s_channel_enable(audio_channel) != ESP_OK) return false;
    // Never force the amplifier: the coprocessor retains headphone detection.
    bool success = bsp_audio_set_amplifier(true) == ESP_OK;
    for (unsigned i = 0; success && i < counts[request->event] && request_allowed(request); i++)
        success = render_note(&sounds[request->event][i], request);
    if (success && request_allowed(request)) {
        const int16_t silence[CHUNK_FRAMES * 2] = {0};
        // Flush more than the complete DMA ring before stopping the clock.
        for (unsigned flushed = 0; success && flushed < dma_bytes + sizeof(silence); flushed += sizeof(silence))
            success = write_pcm(silence, sizeof(silence));
    }
    esp_err_t amplifier_result = bsp_audio_set_amplifier(false);
    esp_err_t channel_result = i2s_channel_disable(audio_channel);
    return success && amplifier_result == ESP_OK && channel_result == ESP_OK;
}

static void sound_worker(void *unused) {
    (void)unused;
    sound_request_t request;
    for (;;) {
        if (xQueueReceive(sound_queue, &request, portMAX_DELAY) != pdTRUE) continue;
        int64_t age = esp_timer_get_time() - request.queued_us;
        if (age < 0 || age > 3000000 || !request_allowed(&request)) continue;
        if (!play_request(&request)) {
            // A codec/I2S fault disables sounds without disturbing the app.
            bsp_audio_set_amplifier(false);
            portENTER_CRITICAL(&sound_lock);
            audio_ready = false;
            portEXIT_CRITICAL(&sound_lock);
            ESP_LOGW(TAG, "Audio unavailable; messaging continues");
        }
    }
}

esp_err_t mesh_sound_init(const mesh_sound_settings_t *settings) {
    if (!mesh_sound_valid(settings)) return ESP_ERR_INVALID_ARG;
    mesh_sound_apply(settings);
    if (sound_task) return mesh_sound_available() ? ESP_OK : ESP_ERR_INVALID_STATE;
    esp_err_t result = bsp_audio_get_i2s_handle(&audio_channel);
    if (result != ESP_OK) return result;
    if (!audio_channel) return ESP_ERR_INVALID_STATE;
    i2s_chan_info_t info;
    result = i2s_channel_get_info(audio_channel, &info);
    if (result != ESP_OK) return result;
    if (info.total_dma_buf_size == 0 || info.total_dma_buf_size > 65536) return ESP_ERR_INVALID_STATE;
    dma_bytes = info.total_dma_buf_size;
    result = bsp_audio_set_amplifier(false);
    if (result != ESP_OK) return result;
    if (info.is_enabled) {
        result = i2s_channel_disable(audio_channel);
        if (result != ESP_OK) return result;
    }
    // Use the BSP's safe codec range; the user slider scales sample amplitude.
    result = bsp_audio_set_volume(100);
    if (result != ESP_OK) return result;
    sound_queue = xQueueCreate(SOUND_QUEUE_LENGTH, sizeof(sound_request_t));
    if (!sound_queue) return ESP_ERR_NO_MEM;
    portENTER_CRITICAL(&sound_lock);
    audio_ready = true;
    portEXIT_CRITICAL(&sound_lock);
    if (xTaskCreate(sound_worker, "mesh_sound", 6144, NULL, 2, &sound_task) != pdPASS) {
        portENTER_CRITICAL(&sound_lock);
        audio_ready = false;
        portEXIT_CRITICAL(&sound_lock);
        vQueueDelete(sound_queue);
        sound_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
