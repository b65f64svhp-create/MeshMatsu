// Draft controls are separate from the currently applied/persisted audio policy.
static mesh_sound_settings_t sound_settings, sound_draft;
static size_t sound_field;
static char sound_feedback[96];
static void open_sound_settings(void) {
    sound_draft = sound_settings; sound_field = 0; sound_feedback[0] = '\0';
    current_screen = SCREEN_SOUND_SETTINGS;
}
static void save_sound_settings(void) {
    if (mesh_sound_save(&sound_draft) == ESP_OK) {
        sound_settings = sound_draft; mesh_sound_apply(&sound_settings);
        snprintf(sound_feedback, sizeof(sound_feedback), "Sound settings saved.");
    } else snprintf(sound_feedback, sizeof(sound_feedback), "Could not save. Current sound settings kept.");
}
static void sound_step(bool forward) {
    if (!sound_field) sound_draft.message_sounds = !sound_draft.message_sounds;
    else if (sound_field == 1) sound_draft.game_sounds = !sound_draft.game_sounds;
    else if (sound_field == 2) {
        int next = sound_draft.volume + (forward ? 5 : -5);
        sound_draft.volume = next < 0 ? 0 : next > 100 ? 100 : next;
    } else if (sound_field == 3) sound_draft.quiet_enabled = !sound_draft.quiet_enabled;
    else if (sound_field == 4) sound_draft.quiet_start_hour = (sound_draft.quiet_start_hour + (forward ? 1 : 23)) % 24;
    else if (sound_field == 5) sound_draft.quiet_end_hour = (sound_draft.quiet_end_hour + (forward ? 1 : 23)) % 24;
    else return;
    snprintf(sound_feedback, sizeof(sound_feedback), "Unsaved changes. Select Save Settings when ready.");
}
static bool sound_navigation(int key) {
    if (current_screen != SCREEN_SOUND_SETTINGS) return false;
    if (key == BSP_INPUT_NAVIGATION_KEY_UP) { if (sound_field) sound_field--; }
    else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) { if (sound_field < 7) sound_field++; }
    else if (key == BSP_INPUT_NAVIGATION_KEY_LEFT || key == BSP_INPUT_NAVIGATION_KEY_RIGHT) sound_step(key == BSP_INPUT_NAVIGATION_KEY_RIGHT);
    else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) {
        if (sound_field == 7) save_sound_settings();
        else if (sound_field == 6) snprintf(sound_feedback, sizeof(sound_feedback), "%s", !sound_draft.volume ? "Test is muted at volume 0." :
            mesh_sound_test(sound_draft.volume) ? "Test queued at draft volume; quiet hours bypassed." : "Audio unavailable or busy. Please retry.");
        else sound_step(true);
    } else return false;
    return true;
}
static bool sound_key(char key) {
    if (current_screen != SCREEN_SOUND_SETTINGS) return false;
    if (key == '\t') sound_field = (sound_field + 1) % 8;
    else if (key == '\r' || key == '\n') sound_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
    else if (key == ' ') sound_step(true);
    return true;
}
static void sound_hour_label(unsigned hour, char *out, size_t size) {
    if (time_settings.use_24h) snprintf(out, size, "%02u:00", hour);
    else snprintf(out, size, "%u%s", hour % 12 ? hour % 12 : 12, hour < 12 ? "am" : "pm");
}
static void draw_sound_settings(void) {
    const char *names[] = {"Message sounds", "Packet Runner sounds", "Volume", "Quiet hours", "Quiet starts", "Quiet ends", "TEST SOUND", "SAVE SETTINGS"};
    char start[16], end[16], volume[16];
    sound_hour_label(sound_draft.quiet_start_hour, start, sizeof(start));
    sound_hour_label(sound_draft.quiet_end_hour, end, sizeof(end));
    snprintf(volume, sizeof(volume), "%u%%", sound_draft.volume);
    const char *values[] = {sound_draft.message_sounds ? "On" : "Off", sound_draft.game_sounds ? "On" : "Off", volume,
        sound_draft.quiet_enabled ? "On" : "Off", start, end, "Use draft volume", "Apply and remember"};
    for (size_t i = 0; i < 8; i++) {
        int y = 82 + i * 38;
        pax_draw_rect(&fb, sound_field == i ? UI_SELECTED : UI_PANEL, 20, y, logical_h_res - 40, 34);
        pax_draw_rect(&fb, sound_field == i ? UI_GREEN : UI_BORDER, 20, y, 3, 34);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 32, y + 10, names[i]);
        float width = pax_text_size(pax_font_sky_mono, 12, values[i]).x;
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 12, logical_h_res - 32 - width, y + 10, values[i]);
    }
    draw_rich_text(sound_feedback[0] ? sound_feedback : "Quiet hours use local time; same start/end mutes all day.", 20, 395, 10, logical_h_res - 40, 14, 1, UI_MUTED);
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET,
        "Up/Down: select  L/R: change  Enter: action  Esc: back");
}
static void play_message_alerts(void) {
    uint16_t events = mesh_radio_take_message_alerts();
    bool direct = (events & 0x100) != 0;
    bool channel = (events & channel_alerts_mask & 0xff) != 0;
    // Burst coalescing gives a DM priority; disabled/muted events are consumed.
    if (direct) mesh_sound_play(MESH_SOUND_DIRECT);
    else if (channel) mesh_sound_play(MESH_SOUND_CHANNEL);
}
