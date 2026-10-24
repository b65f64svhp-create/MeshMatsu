#pragma once
#include <stddef.h>
typedef enum { MESH_SETTINGS_ROOT,MESH_SETTINGS_NODE,MESH_SETTINGS_DISPLAY,MESH_SETTINGS_GROUPS } mesh_settings_group_t;
typedef enum {
    SETTING_ABOUT, SETTING_NODE_GROUP, SETTING_CHANNELS, SETTING_SHARED_CHANNEL,
    SETTING_MANUAL_CHANNEL, SETTING_DISPLAY_GROUP, SETTING_POWER,
    SETTING_IDENTITY, SETTING_ROLE, SETTING_RADIO, SETTING_POSITION,
    SETTING_TELEMETRY, SETTING_BACKUP, SETTING_BEHAVIOR,
    SETTING_SCREEN, SETTING_ALERTS, SETTING_TEXT, SETTING_TIME,
    SETTING_HISTORY, SETTING_SOUND, SETTING_UNITS, SETTING_SECURITY, SETTING_RANGE, SETTING_ARCHIVE, SETTING_NEIGHBORS, SETTING_FIRST_USE
} mesh_setting_action_t;
typedef struct {const char *name,*detail;mesh_setting_action_t action;} mesh_setting_entry_t;
size_t mesh_settings_count(mesh_settings_group_t group);
const mesh_setting_entry_t *mesh_settings_entry(mesh_settings_group_t group,size_t index);
size_t mesh_settings_step(mesh_settings_group_t group,size_t selected,int direction);
