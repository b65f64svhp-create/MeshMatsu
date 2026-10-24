static mesh_telemetry_config_t telemetry_draft;
static bool telemetry_settings_loaded, telemetry_settings_edited, telemetry_settings_pending;
static size_t telemetry_settings_field;
static char telemetry_settings_feedback[112];
static void open_telemetry_settings(void) {
    current_screen = SCREEN_TELEMETRY_SETTINGS;
    telemetry_settings_loaded = telemetry_settings_edited = telemetry_settings_pending = false;
    telemetry_settings_field = 0; telemetry_settings_feedback[0] = '\0';
    mesh_radio_request_owner(); mesh_radio_request_metadata(); mesh_radio_request_telemetry_config();
}
static void telemetry_settings_step(bool forward) {
    if (!telemetry_settings_loaded || telemetry_settings_pending) return;
    if (!telemetry_settings_field) {
        if (!telemetry_draft.enable_supported) {
            snprintf(telemetry_settings_feedback, sizeof(telemetry_settings_feedback), "This radio firmware supports interval changes only."); return;
        }
        telemetry_draft.enabled = !telemetry_draft.enabled;
    } else if (telemetry_settings_field == 1) {
        const uint32_t options[] = {0, 1800, 3600, 10800, 21600, 43200, 86400};
        const size_t count = sizeof(options) / sizeof(options[0]);
        size_t index = 0;
        for (; index < count; index++) if (telemetry_draft.interval_secs == options[index]) break;
        telemetry_draft.interval_secs = index == count ? 0 : options[(index + (forward ? 1 : count - 1)) % count];
    } else return;
    telemetry_settings_edited = true;
    snprintf(telemetry_settings_feedback, sizeof(telemetry_settings_feedback), "Unsaved changes. Select Save to apply to your radio.");
}
static void save_telemetry_settings(void) {
    if (!telemetry_settings_loaded || telemetry_settings_pending) return;
    if (mesh_radio_set_telemetry_config(&telemetry_draft)) {
        telemetry_settings_pending = true;
        snprintf(telemetry_settings_feedback, sizeof(telemetry_settings_feedback), "Saving; waiting for verified radio readback...");
    } else {
        mesh_telemetry_config_t current;
        if (mesh_radio_get_telemetry_config(&current)) telemetry_draft.revision = current.revision;
        mesh_radio_request_owner(); mesh_radio_request_telemetry_config();
        snprintf(telemetry_settings_feedback, sizeof(telemetry_settings_feedback), "Not saved. Wait for the radio, then retry Save.");
    }
}
static bool telemetry_settings_navigation(int key) {
    if (current_screen != SCREEN_TELEMETRY_SETTINGS) return false;
    if (key == BSP_INPUT_NAVIGATION_KEY_UP) { if (telemetry_settings_field) telemetry_settings_field--; }
    else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) { if (telemetry_settings_field < 2) telemetry_settings_field++; }
    else if (key == BSP_INPUT_NAVIGATION_KEY_LEFT || key == BSP_INPUT_NAVIGATION_KEY_RIGHT) telemetry_settings_step(key == BSP_INPUT_NAVIGATION_KEY_RIGHT);
    else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) {
        if (telemetry_settings_field == 2) save_telemetry_settings();
        else telemetry_settings_step(true);
    } else return false;
    return true;
}
static bool telemetry_settings_key(char key) {
    if (current_screen != SCREEN_TELEMETRY_SETTINGS) return false;
    if (key == '\t') telemetry_settings_field = (telemetry_settings_field + 1) % 3;
    else if (key == '\r' || key == '\n') telemetry_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
    else if (key == ' ') telemetry_settings_step(true);
    return true;
}
static void draw_telemetry_settings(void) {
    mesh_telemetry_save_t save = mesh_radio_telemetry_save();
    if (telemetry_settings_pending && save.state != MESH_TELEMETRY_CONFIG_SAVING) {
        telemetry_settings_pending = false;
        const char *text = save.state == MESH_TELEMETRY_CONFIG_SAVED ? "Telemetry settings verified on your radio." :
            save.state == MESH_TELEMETRY_CONFIG_TIMEOUT ? "No verified reply. Reopen to check the radio's settings." :
            save.state == MESH_TELEMETRY_CONFIG_INTERRUPTED ? "Radio changed; save verification interrupted." :
            "Radio did not confirm the change. Reopen to check settings.";
        snprintf(telemetry_settings_feedback, sizeof(telemetry_settings_feedback), "%s", text);
        if (save.state == MESH_TELEMETRY_CONFIG_SAVED) telemetry_settings_edited = false;
    }
    mesh_telemetry_config_t current;
    if (mesh_radio_get_telemetry_config(&current)) {
        if (!telemetry_settings_loaded || (!telemetry_settings_edited && !telemetry_settings_pending)) telemetry_draft = current;
        telemetry_settings_loaded = true;
        if (!telemetry_settings_pending) telemetry_draft.enable_supported = current.enable_supported;
    } else if (!telemetry_settings_pending) mesh_radio_request_telemetry_config();
    const char *names[] = {"Broadcast device metrics", "Broadcast interval", "SAVE SETTINGS"};
    char interval[32];
    if (!telemetry_draft.interval_secs) snprintf(interval, sizeof(interval), "Radio default");
    else if (!(telemetry_draft.interval_secs % 3600)) snprintf(interval, sizeof(interval), "%lu hours", (unsigned long)(telemetry_draft.interval_secs / 3600));
    else if (!(telemetry_draft.interval_secs % 60)) snprintf(interval, sizeof(interval), "%lu min", (unsigned long)(telemetry_draft.interval_secs / 60));
    else snprintf(interval, sizeof(interval), "%lu sec", (unsigned long)telemetry_draft.interval_secs);
    const char *values[] = {!telemetry_settings_loaded ? "Loading..." : !telemetry_draft.enable_supported ? "Firmware controlled" : telemetry_draft.enabled ? "On" : "Off",
        telemetry_settings_loaded ? interval : "Loading...", telemetry_settings_pending ? "Verifying..." : "Apply to local radio"};
    for (size_t i = 0; i < 3; i++) {
        int y = 90 + i * 60;
        pax_draw_rect(&fb, telemetry_settings_field == i ? UI_SELECTED : UI_PANEL, 20, y, logical_h_res - 40, 50);
        pax_draw_rect(&fb, telemetry_settings_field == i ? UI_GREEN : UI_BORDER, 20, y, 3, 50);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 13, 32, y + 18, names[i]);
        float width = pax_text_size(pax_font_sky_mono, 13, values[i]).x;
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 13, logical_h_res - 32 - width, y + 18, values[i]);
    }
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, 286, "Shares the radio's battery, voltage, airtime and channel use.");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, 309, "The radio may lengthen intervals to reduce mesh traffic.");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, 332, "Local readings and Request Telemetry remain separate.");
    draw_rich_text(telemetry_settings_feedback[0] ? telemetry_settings_feedback : "Only your local radio is changed after explicit Save.", 20, 385, 10, logical_h_res - 40, 15, 2, UI_MUTED);
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET,
        "Up/Down: select  L/R: change  Enter: action  Esc: back");
}
