#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sound_settings.h"

typedef enum {
    MESH_SOUND_CHANNEL,
    MESH_SOUND_DIRECT,
    MESH_SOUND_GAME_RELAY,
    MESH_SOUND_GAME_DROP,
    MESH_SOUND_GAME_DELIVERED,
} mesh_sound_event_t;

// Call after bsp_device_initialize: the BSP owns the codec and I2S channel.
esp_err_t mesh_sound_init(const mesh_sound_settings_t *settings);
// These calls never wait for playback. Full queues drop new sounds.
void mesh_sound_apply(const mesh_sound_settings_t *settings);
bool mesh_sound_play(mesh_sound_event_t event);
// Explicit preview bypasses category switches and quiet hours; 0 remains silent.
bool mesh_sound_test(uint8_t volume);
bool mesh_sound_available(void);
// Cancels queued/current sounds; the worker switches the amplifier off.
void mesh_sound_stop(void);
