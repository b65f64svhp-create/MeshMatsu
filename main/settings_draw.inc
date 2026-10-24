// App-task screen drawing, extracted without changing its behavior.
static void draw_power_settings(void) {
    const char *labels[] = {"Decorative lights (A/B too)", "Keyboard auto-off", "Screen auto-dim", "Reduced background work"};
    char keyboard_timer[20], dim_timer[20];
    if (power_draft.keyboard_timeout_seconds >= 60) snprintf(keyboard_timer, sizeof(keyboard_timer), "%u min", power_draft.keyboard_timeout_seconds / 60);
    else if (power_draft.keyboard_timeout_seconds) snprintf(keyboard_timer, sizeof(keyboard_timer), "%u sec", power_draft.keyboard_timeout_seconds);
    else snprintf(keyboard_timer, sizeof(keyboard_timer), "Off");
    if (power_draft.dim_timeout_seconds >= 60) snprintf(dim_timer, sizeof(dim_timer), "%u min", power_draft.dim_timeout_seconds / 60);
    else if (power_draft.dim_timeout_seconds) snprintf(dim_timer, sizeof(dim_timer), "%u sec", power_draft.dim_timeout_seconds);
    else snprintf(dim_timer, sizeof(dim_timer), "Off");
    const char *values[] = {power_draft.decorative_lights ? "On" : "Off",
        keyboard_timer, dim_timer,
        power_draft.reduced_background ? "On" : "Off"};
    for (size_t i = 0; i < 4; i++) {
        int y = 86 + i * 52;
        pax_draw_rect(&fb, power_field == i ? UI_SELECTED : UI_PANEL, 20, y, logical_h_res - 40, 44);
        pax_draw_rect(&fb, power_field == i ? UI_GREEN : UI_BORDER, 20, y, 3, 44);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 32, y + 15, labels[i]);
        float width = pax_text_size(pax_font_sky_mono, 13, values[i]).x;
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 13, logical_h_res - 32 - width, y + 14, values[i]);
    }
    pax_draw_rect(&fb, power_field == 4 ? UI_GREEN : UI_SELECTED, 20, 306, logical_h_res - 40, 36);
    pax_draw_text(&fb, power_field == 4 ? BLACK : UI_GREEN, pax_font_sky_mono, 14, 32, 316, "SAVE SETTINGS");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, 358, "Any key restores dimmed screen / keyboard.");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, 376, "Blue unread light has its own settings.");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, 400,
        power_feedback[0] ? power_feedback : "Options are independent; radio stays active.");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET,
        "Up/Down: select  L/R: change  Enter: save");
}

static void draw_display_settings(void) {
    const char *labels[] = {"Screen brightness", "Keyboard brightness", "Auto screen-off", "Blue unread light", "Light when screen off", "Light triggered by"};
    char values[6][24];
    snprintf(values[0], sizeof(values[0]), "%u%%", display_draft.screen_brightness);
    snprintf(values[1], sizeof(values[1]), "%u%%", display_draft.keyboard_brightness);
    if (display_draft.timeout_seconds >= 60) snprintf(values[2], sizeof(values[2]), "%u min", display_draft.timeout_seconds / 60);
    else if (display_draft.timeout_seconds) snprintf(values[2], sizeof(values[2]), "%u sec", display_draft.timeout_seconds);
    else snprintf(values[2], sizeof(values[2]), "Off");
    snprintf(values[3], sizeof(values[3]), "%s", display_draft.unread_light ? "On" : "Off");
    snprintf(values[4], sizeof(values[4]), "%u%%", display_draft.night_brightness);
    snprintf(values[5], sizeof(values[5]), "%s", display_draft.alert_sources == MESH_ALERT_CHANNEL ? "Channels" :
             display_draft.alert_sources == MESH_ALERT_DIRECT ? "Direct" : "Both");
    for (size_t i = 0; i < 6; i++) {
        int y = 86 + (int)i * 42;
        pax_draw_rect(&fb, display_field == i ? UI_SELECTED : UI_PANEL, 20, y, logical_h_res - 40, 36);
        pax_draw_rect(&fb, display_field == i ? UI_GREEN : UI_BORDER, 20, y, 3, 36);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 32, y + 11, labels[i]);
        float width = pax_text_size(pax_font_sky_mono, 13, values[i]).x;
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 13, logical_h_res - 32 - width, y + 10, values[i]);
    }
    pax_draw_rect(&fb, display_field == 6 ? UI_GREEN : UI_SELECTED, 20, 346, logical_h_res - 40, 36);
    pax_draw_text(&fb, display_field == 6 ? BLACK : UI_GREEN, pax_font_sky_mono, 14, 32, 356, "SAVE SETTINGS");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, 394,
                  display_feedback[0] ? display_feedback : "Radio keeps running with the screen off.");
    if (display_draft.timeout_seconds) {
        pax_draw_text(&fb, KEY_TRIANGLE_COLOR, pax_font_sky_mono, 10, 20, 406,
            "Auto-off: press diamond/F6 to wake screen.");
    }
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET,
                  "Up/Down: select  L/R: preview  Enter: save");
}

static void draw_history_settings(void) {
    char value[64];
    snprintf(value, sizeof(value), "%u / %u recent messages stored", (unsigned)mesh_radio_message_count(), (unsigned)mesh_radio_history_limit());
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 14, 20, 88, value);
    pax_draw_rect(&fb, history_field == 0 ? UI_SELECTED : UI_PANEL, 20, 130, logical_h_res - 40, 64);
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 32, 142, "RECENT MESSAGE LIMIT");
    snprintf(value, sizeof(value), "%u messages", (unsigned)history_limit_draft);
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 18, 32, 165, value);
    draw_rich_text("Shared across channels and direct chats. Oldest read messages go first; pending sends are protected. SD Message Archive can keep older messages separately. Reducing this limit moves messages out of recent history.", 20, 220, 13, logical_h_res - 40, 22, 5, UI_MUTED);
    pax_draw_rect(&fb, history_field ? UI_GREEN : UI_SELECTED, 20, 344, logical_h_res - 40, 36);
    pax_draw_text(&fb, history_field ? BLACK : UI_GREEN, pax_font_sky_mono, 13, 32, 355, history_limit_confirm ? "CONFIRM SMALLER LIMIT" : "SAVE HISTORY LIMIT");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, 400, history_feedback);
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET, "Up/Down: field  L/R: limit  Enter: save  Esc: back");
}

#include "node_browser_draw.inc"

static void draw_channel_alerts(void) {
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,64,"Blue light and message sounds; unread counts are kept.");
    for(uint8_t i=0;i<8;i++) {
        mesh_channel_t channel;bool loaded=mesh_radio_get_channel(i,&channel);
        char label[64];snprintf(label,sizeof(label),"Slot %u: %s",i,loaded&&channel.role?(channel.name[0]?channel.name:"Primary"):"Disabled / unavailable");
        int y=84+i*32;
        pax_draw_rect(&fb,channel_alert_field==i?UI_SELECTED:UI_PANEL,20,y,logical_h_res-40,28);
        if(channel_alert_field==i)pax_draw_rect(&fb,UI_GREEN,20,y,3,28);
        pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,12,32,y+8,label);
        const char *value=mesh_channel_alert_enabled(channel_alerts_draft,i)?"On":"Off";
        float width=pax_text_size(pax_font_sky_mono,12,value).x;
        pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,12,logical_h_res-32-width,y+8,value);
    }
    pax_draw_rect(&fb,channel_alert_field==8?UI_GREEN:UI_SELECTED,20,354,logical_h_res-40,36);
    pax_draw_text(&fb,channel_alert_field==8?BLACK:UI_GREEN,pax_font_sky_mono,13,32,365,"SAVE CHANNEL ALERTS");
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,401,channel_alert_feedback[0]?channel_alert_feedback:"Also follows the master light/source settings.");
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,logical_v_res-UI_HINT_OFFSET,"Up/Down: slot  L/R: toggle  Enter: save  Esc: back");
}
static void draw_time_settings(void) {
    char offset[24];int minutes=time_draft.offset_minutes;
    snprintf(offset,sizeof(offset),"UTC%c%02d:%02d",minutes<0?'-':'+',abs(minutes)/60,abs(minutes)%60);
    const char *labels[]={"Timezone","Clock format","Fixed UTC offset"};
    const char *values[]={mesh_time_zone_name(time_draft.zone),time_draft.use_24h?"24-hour":"12-hour",offset};
    for(unsigned i=0;i<3;i++) {
        int y=86+i*66;
        pax_draw_rect(&fb,time_field==i?UI_SELECTED:UI_PANEL,20,y,logical_h_res-40,56);
        if(time_field==i)pax_draw_rect(&fb,UI_GREEN,20,y,3,56);
        pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,32,y+8,labels[i]);
        pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,14,32,y+28,values[i]);
    }
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,9,20,282,"Offset is used only with Fixed UTC offset.");
    pax_draw_rect(&fb,time_field==3?UI_GREEN:UI_SELECTED,20,306,logical_h_res-40,36);
    pax_draw_text(&fb,time_field==3?BLACK:UI_GREEN,pax_font_sky_mono,13,32,317,"SAVE TIME SETTINGS");
    char stamp[32];mesh_time_stamp(&time_draft,(uint32_t)time(NULL),stamp,sizeof(stamp));
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,9,20,357,"FORMAT PREVIEW (current timezone)");
    pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,16,20,372,stamp);
    if(time_feedback[0])pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,401,time_feedback);
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,logical_v_res-UI_HINT_OFFSET,"Up/Down: field  L/R: change  Enter: save  Esc: back");
}
static void draw_role_settings(void) {
    mesh_device_config_t current;
    bool known=mesh_radio_get_device(&current);
    mesh_role_save_t operation=mesh_radio_role_save();
    if(known && (!role_loaded || !role_edited)) {role_baseline=current;role_draft=current.role;role_loaded=true;}
    // A confirmed save supplies the baseline for another edit on this page.
    if(known && operation.state==MESH_ROLE_SAVED && current.role==operation.desired)role_baseline=current;
    if(!known && operation.state!=MESH_ROLE_SAVING)mesh_radio_request_device();
    char value[64];snprintf(value,sizeof(value),"Current: %s",known?mesh_role_name(current.role):"Loading from radio...");
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,12,20,82,value);
    pax_draw_rect(&fb,role_field==0?UI_SELECTED:UI_PANEL,20,112,logical_h_res-40,52);
    if(role_field==0)pax_draw_rect(&fb,UI_GREEN,20,112,3,52);
    pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,18,32,130,role_loaded?mesh_role_name(role_draft):"Please wait...");
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,184,"FORWARDING BEHAVIOR");
    draw_rich_text(role_loaded?mesh_role_description(role_draft):"Reading the current device configuration.",20,207,13,logical_h_res-40,22,4,WHITE);
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,9,20,290,"Forwarding also follows rebroadcast settings.");
    pax_draw_text(&fb,KEY_TRIANGLE_COLOR,pax_font_sky_mono,10,20,306,"Saving may restart radio and apply role defaults.");
    pax_draw_rect(&fb,role_field==1?UI_GREEN:UI_SELECTED,20,336,logical_h_res-40,36);
    pax_draw_text(&fb,role_field==1?BLACK:UI_GREEN,pax_font_sky_mono,13,32,347,"SAVE ROLE TO RADIO");
    const char *feedback=role_feedback;
    if(!feedback[0])feedback=operation.state==MESH_ROLE_SAVING?"Saving; waiting for radio restart / readback...":
        operation.state==MESH_ROLE_SAVED?"Role confirmed by radio.":operation.state==MESH_ROLE_FAILED?"Change not confirmed. Reopen to check radio role.":
        operation.state==MESH_ROLE_TIMEOUT?"No confirmation within 60s. Check Diagnostics.":"Choose a role, then select Save and Enter.";
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,401,feedback);
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,20,logical_v_res-UI_HINT_OFFSET,"Up/Down: field  L/R: role  Enter: save  Esc: back");
}

static void draw_text_settings(void) {
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 20, 84, "Channel and direct-message text");
    int gap = 10, width = ((int)logical_h_res - 60) / 3;
    for (unsigned i = 0; i < 3; i++) {
        int x = 20 + i * (width + gap);
        pax_draw_rect(&fb, text_size_draft == i ? UI_SELECTED : UI_PANEL, x, 120, width, 48);
        if (text_size_draft == i) pax_outline_rect(&fb, UI_GREEN, x, 120, width, 48);
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 14, x + 12, 137, mesh_message_size_name(i));
    }
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, 194, "PREVIEW");
    pax_draw_rect(&fb, UI_PANEL, 20, 216, logical_h_res - 40, 120);
    draw_rich_text("Hello from MeshMatsu! This is how your messages will look.", 32, 230,
        mesh_message_font(text_size_draft), logical_h_res - 64, mesh_message_line_height(text_size_draft), 3, WHITE);
    pax_draw_rect(&fb, UI_SELECTED, 20, 354, logical_h_res - 40, 36);
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 13, 32, 365, "ENTER: SAVE TEXT SIZE");
    if (text_size_feedback[0]) pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, 401, text_size_feedback);
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET, "L/R: choose  Enter: save  Esc: back");
}

static void draw_about(void) {
    mesh_radio_metadata_t metadata = mesh_radio_metadata();
    mesh_radio_status_t radio = mesh_radio_status();
    char app[40], hardware[80], node[24], age[32], frames[40];
    snprintf(app, sizeof(app), "%s / v0.%u.beta", MESH_APP_NAME, MESH_APP_VERSION);
    if (metadata.loaded) snprintf(hardware, sizeof(hardware), "%s (%lu)", mesh_hardware_model_name(metadata.hardware_model), (unsigned long)metadata.hardware_model);
    else snprintf(hardware, sizeof(hardware), "Unavailable");
    if (mesh_radio_local_node()) snprintf(node, sizeof(node), "!%08lx", (unsigned long)mesh_radio_local_node());
    else snprintf(node, sizeof(node), "Unavailable");
    if (radio.frames_received) snprintf(age, sizeof(age), "%u seconds ago", mesh_radio_rx_age_seconds());
    else snprintf(age, sizeof(age), "No frames received");
    snprintf(frames, sizeof(frames), "%lu frames / %lu reconnects", (unsigned long)radio.frames_received, (unsigned long)radio.reconnect_attempts);
    const char *labels[] = {"APPLICATION", "RADIO FIRMWARE", "RADIO HARDWARE MODEL", "LOCAL RADIO NODE", "CONNECTION", "LAST RADIO ACTIVITY", "LINK COUNTERS"};
    const char *values[] = {app, metadata.loaded && metadata.firmware_version[0] ? metadata.firmware_version : "Unavailable",
        hardware, node, radio.reconnecting ? "Reconnecting" : radio.synchronized ? "Online" : radio.started ? "Waiting / syncing" : "Offline", age, frames};
    for (unsigned i = 0; i < 7; i++) {
        int y = 82 + i * 36;
        pax_draw_rect(&fb, UI_PANEL, 20, y, logical_h_res - 40, 32);
        draw_node_value(labels[i], values[i], 32, y + 5, logical_h_res - 64);
    }
    const char *actions[]={"REFRESH RADIO INFO","RADIO ACTIVITY >"};
    for(unsigned i=0;i<2;i++) {
        int x=20+i*((int)logical_h_res/2);int w=(int)logical_h_res/2-40;
        pax_draw_rect(&fb,about_action_selection==i?UI_SELECTED:UI_PANEL,x,352,w,36);
        if(about_action_selection==i)pax_draw_rect(&fb,UI_GREEN,x,352,3,36);
        pax_draw_text(&fb,UI_GREEN,pax_font_sky_mono,12,x+14,364,actions[i]);
    }
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, 401, metadata.pending ? "Reading metadata from the connected radio..." :
        metadata.loaded ? "Firmware and model reported by the radio." : "No metadata reply yet. Enter: retry.");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET, "L/R: select action  Enter: open  Esc: back");
}

