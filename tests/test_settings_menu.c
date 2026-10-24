#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "settings_menu.c"
int main(void) {
    const mesh_setting_action_t expected[3][10]={
        {SETTING_ABOUT,SETTING_NODE_GROUP,SETTING_CHANNELS,SETTING_SHARED_CHANNEL,SETTING_MANUAL_CHANNEL,SETTING_DISPLAY_GROUP,SETTING_POWER,SETTING_FIRST_USE},
        {SETTING_IDENTITY,SETTING_ROLE,SETTING_RADIO,SETTING_POSITION,SETTING_TELEMETRY,SETTING_BACKUP,SETTING_RANGE,SETTING_SECURITY,SETTING_BEHAVIOR,SETTING_NEIGHBORS},
        {SETTING_SCREEN,SETTING_ALERTS,SETTING_TEXT,SETTING_TIME,SETTING_HISTORY,SETTING_ARCHIVE,SETTING_SOUND,SETTING_UNITS}
    };
    bool seen[26]={0};
    for(unsigned group=0;group<MESH_SETTINGS_GROUPS;group++) {
        size_t count=mesh_settings_count(group);assert(count==(group==MESH_SETTINGS_NODE?10:group==MESH_SETTINGS_DISPLAY?8:8));
        for(size_t i=0;i<count;i++) {
            const mesh_setting_entry_t *entry=mesh_settings_entry(group,i);
            assert(entry&&entry->name[0]&&entry->detail[0]&&entry->action==expected[group][i]);
            assert(!seen[entry->action]);seen[entry->action]=true;
            assert(mesh_settings_step(group,i,0)==i);
            assert(mesh_settings_step(group,i,INT_MIN)==(i?i-1:0));
            assert(mesh_settings_step(group,i,INT_MAX)==(i+1<count?i+1:i));
        }
        assert(!mesh_settings_entry(group,count)&&!mesh_settings_entry(group,SIZE_MAX));
        assert(mesh_settings_step(group,SIZE_MAX,0)==count-1);
    }
    assert(!mesh_settings_count(MESH_SETTINGS_GROUPS)&&!mesh_settings_count((mesh_settings_group_t)-1));
    assert(!mesh_settings_entry((mesh_settings_group_t)-1,0)&&!mesh_settings_step(MESH_SETTINGS_GROUPS,9,1));
    puts("PASS: all 26 settings keep their destinations/order, shared labels and counts, clamped navigation and invalid-group/index handling");
}
