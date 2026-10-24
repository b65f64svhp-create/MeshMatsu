#include "settings_menu.h"
#define COUNT(a) (sizeof(a)/sizeof(*(a)))
// Labels, descriptions, navigation bounds and destination IDs share these tables.
static const mesh_setting_entry_t root_entries[]={
    {"ABOUT & DIAGNOSTICS","App version, radio firmware and connection",SETTING_ABOUT},
    {"NODE & RADIO","Node identity, device role and radio settings",SETTING_NODE_GROUP},
    {"CHANNELS","Channel names and MQTT permissions",SETTING_CHANNELS},
    {"ADD SHARED CHANNEL","Import a staged channel name and key",SETTING_SHARED_CHANNEL},
    {"ADD CHANNEL MANUALLY","Enter an existing channel name and key",SETTING_MANUAL_CHANNEL},
    {"DISPLAY & ALERTS","Screen, lights, channel alerts, text, time and history",SETTING_DISPLAY_GROUP},
    {"BATTERY SAVER","Lights, dimming and background activity",SETTING_POWER},
    {"FIRST-TIME SETUP","Radio firmware, region and radio names",SETTING_FIRST_USE},
};
static const mesh_setting_entry_t node_entries[]={
    {"NODE IDENTITY","Radio long name and short name",SETTING_IDENTITY},
    {"DEVICE ROLE","Forwarding role and explanation",SETTING_ROLE},
    {"RADIO","Region, modem preset and hop limit",SETTING_RADIO},
    {"POSITION & LOCATION","Own position, sharing interval and fixed coordinates",SETTING_POSITION},
    {"TELEMETRY BROADCASTS","Radio device-metrics sharing and broadcast interval",SETTING_TELEMETRY},
    {"BACKUP & RESTORE","Encrypted configuration, preferences, nicknames and keys",SETTING_BACKUP},
    {"RANGE TESTING","Timed direct tests, signal readings, sequence gaps and CSV export",SETTING_RANGE},
    {"SECURITY & CONTACTS","Public contact sharing, verified keys and local administrators",SETTING_SECURITY},
    {"RADIO BEHAVIOR","Node-name announcements and packet forwarding",SETTING_BEHAVIOR},
    {"NEIGHBOR INFO","Reported direct neighbors and local module settings",SETTING_NEIGHBORS},
};
static const mesh_setting_entry_t display_entries[]={
    {"SCREEN & LIGHTS","Screen and keyboard brightness, timer and unread light",SETTING_SCREEN},
    {"CHANNEL ALERTS","Choose channels for the blue light and message sounds",SETTING_ALERTS},
    {"MESSAGE TEXT SIZE","Small, medium or large chat text",SETTING_TEXT},
    {"TIME","Timezone and 12/24-hour timestamps",SETTING_TIME},
    {"MESSAGE HISTORY","Recent message count and capacity",SETTING_HISTORY},
    {"SD MESSAGE ARCHIVE","Older messages, search, storage limit and clear",SETTING_ARCHIVE},
    {"SOUNDS & QUIET HOURS","Message/game sounds, volume and quiet hours",SETTING_SOUND},
    {"UNITS","Metric/imperial distances and Celsius/Fahrenheit temperatures",SETTING_UNITS},
};
static const struct {const mesh_setting_entry_t *entries;size_t count;} menus[MESH_SETTINGS_GROUPS]={
    {root_entries,COUNT(root_entries)}, {node_entries,COUNT(node_entries)},
    {display_entries,COUNT(display_entries)}
};
size_t mesh_settings_count(mesh_settings_group_t group){
    return (unsigned)group<MESH_SETTINGS_GROUPS?menus[group].count:0;
}
const mesh_setting_entry_t *mesh_settings_entry(mesh_settings_group_t group,size_t index){
    return index<mesh_settings_count(group)?&menus[group].entries[index]:NULL;
}
size_t mesh_settings_step(mesh_settings_group_t group,size_t selected,int direction){
    size_t count=mesh_settings_count(group);
    if(!count)return 0;
    if(selected>=count)selected=count-1;
    if(direction>0&&selected+1<count)selected++;
    else if(direction<0&&selected)selected--;
    return selected;
}
