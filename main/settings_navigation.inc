// Settings editors share the app task; menu routing is in settings_menu_ui.inc.
static bool settings_navigation(int key) {
    if(current_screen==SCREEN_RADIO_ACTIVITY&&telemetry_view_navigation(key))return true;
    if (behavior_navigation(key) || units_navigation(key) || config_backup_navigation(key) || key_backup_navigation(key) || sound_navigation(key) || telemetry_settings_navigation(key)) return true;
    if (position_navigation(key)) return true;
    if (history_settings_navigation(key)) return true;
    if (additional_settings_navigation(key)) return true;
    if (extra_settings_navigation(key)) return true;
    if (settings_menu_navigation(key)) return true;
    if (current_screen == SCREEN_POWER_SETTINGS) {
        if (key == BSP_INPUT_NAVIGATION_KEY_UP && power_field) power_field--;
        else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN && power_field < 4) power_field++;
        else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) save_power_settings();
        else if ((key == BSP_INPUT_NAVIGATION_KEY_LEFT || key == BSP_INPUT_NAVIGATION_KEY_RIGHT) && power_field < 4) {
            if (power_field == 0) power_draft.decorative_lights = !power_draft.decorative_lights;
            else if (power_field == 3) power_draft.reduced_background = !power_draft.reduced_background;
            else {
                uint16_t *value = power_field == 1 ? &power_draft.keyboard_timeout_seconds : &power_draft.dim_timeout_seconds;
                const uint16_t options[] = {0, 15, 30, 45, 60};
                for (size_t i = 0; i < 5; i++) if (*value == options[i]) {
                    *value = options[(i + (key == BSP_INPUT_NAVIGATION_KEY_RIGHT ? 1 : 4)) % 5]; break;
                }
            }
            snprintf(power_feedback, sizeof(power_feedback), "Unsaved changes. Enter: save.");
        } else return false;
        return true;
    }
    if (current_screen == SCREEN_DISPLAY_ALERTS) {
        if (key == BSP_INPUT_NAVIGATION_KEY_UP && display_field) display_field--;
        else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN && display_field < 6) display_field++;
        else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) save_display_settings();
        else if ((key == BSP_INPUT_NAVIGATION_KEY_LEFT || key == BSP_INPUT_NAVIGATION_KEY_RIGHT) && display_field < 6) {
            step_display_setting(key == BSP_INPUT_NAVIGATION_KEY_RIGHT);
        } else return false;
        return true;
    }
    if (current_screen == SCREEN_MANUAL_CHANNEL) {
        if (key == BSP_INPUT_NAVIGATION_KEY_ESC || key == BSP_INPUT_NAVIGATION_KEY_F1 ||
            key == BSP_INPUT_NAVIGATION_KEY_F2 || key == BSP_INPUT_NAVIGATION_KEY_F3 || key == BSP_INPUT_NAVIGATION_KEY_F4) {
            memset(manual_key, 0, sizeof(manual_key));
            manual_key_visible = false;
            manual_phase = 0;
            return false;
        }
        if (key == BSP_INPUT_NAVIGATION_KEY_F6) { manual_key_visible = false; return false; }
        if (manual_phase) return key == BSP_INPUT_NAVIGATION_KEY_RETURN;
        if (key == BSP_INPUT_NAVIGATION_KEY_UP && manual_field) { manual_field--; manual_key_visible = false; }
        else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN && manual_field < 3) { manual_field++; manual_key_visible = false; }
        else if ((key == BSP_INPUT_NAVIGATION_KEY_LEFT || key == BSP_INPUT_NAVIGATION_KEY_RIGHT) && manual_field == 0) {
            if (key == BSP_INPUT_NAVIGATION_KEY_LEFT && manual_slot > 1) manual_slot--;
            if (key == BSP_INPUT_NAVIGATION_KEY_RIGHT && manual_slot < 7) manual_slot++;
            manual_feedback[0] = '\0';
        } else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) apply_manual_key('\r');
        else if (key == BSP_INPUT_NAVIGATION_KEY_BACKSPACE) apply_manual_key('\b');
        else return false;
        return true;
    }
    if (current_screen == SCREEN_RADIO_SETTINGS) {
        if (key == BSP_INPUT_NAVIGATION_KEY_UP && radio_field) radio_field--;
        else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN && radio_field < 3) radio_field++;
        else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) {
            if (radio_field == 3) save_radio_settings();
            else radio_field++;
        } else if ((key == BSP_INPUT_NAVIGATION_KEY_LEFT || key == BSP_INPUT_NAVIGATION_KEY_RIGHT) && radio_values_loaded && !radio_save_pending && radio_field < 3) {
            bool forward = key == BSP_INPUT_NAVIGATION_KEY_RIGHT;
            if (radio_field == 0) radio_draft.region = step_radio_option(region_options, sizeof(region_options)/sizeof(region_options[0]), radio_draft.region, forward);
            else if (radio_field == 1) {
                radio_draft.modem_preset = step_radio_option(preset_options, sizeof(preset_options)/sizeof(preset_options[0]), radio_draft.modem_preset, forward);
                radio_draft.use_preset = true;
            } else if (radio_field == 2) radio_draft.hop_limit = (radio_draft.hop_limit + (forward ? 1 : 7)) % 8;
            radio_edits_started = true;
            radio_feedback[0] = '\0';
        } else return false;
        return true;
    }
    if (current_screen == SCREEN_CHANNELS) {
        if (key == BSP_INPUT_NAVIGATION_KEY_UP && channel_selection) channel_selection--;
        else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN && channel_selection + 1 < MESH_CHANNEL_LIMIT) channel_selection++;
        else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN || key == BSP_INPUT_NAVIGATION_KEY_RIGHT) open_channel_editor();
        else return false;
        return true;
    }
    if (current_screen == SCREEN_CHANNEL_EDIT) {
        if (key == BSP_INPUT_NAVIGATION_KEY_ESC || key == BSP_INPUT_NAVIGATION_KEY_F1) return false;
        if (channel_save_pending) return key == BSP_INPUT_NAVIGATION_KEY_RETURN;
        else if (key == BSP_INPUT_NAVIGATION_KEY_UP && channel_field) channel_field--;
        else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN && channel_field < 4) channel_field++;
        else if (key == BSP_INPUT_NAVIGATION_KEY_LEFT || key == BSP_INPUT_NAVIGATION_KEY_RIGHT) {
            if (channel_field == 1) channel_uplink = !channel_uplink;
            if (channel_field == 2) channel_downlink = !channel_downlink;
            if (channel_field == 3) channel_precision = mesh_location_precision_step(channel_precision, key == BSP_INPUT_NAVIGATION_KEY_RIGHT, !editing_channel.has_key || editing_channel.default_key);
        } else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) { if(channel_field==4)open_channel_tools();else save_channel(); }
        else if (key == BSP_INPUT_NAVIGATION_KEY_BACKSPACE && channel_field == 0) {
            size_t n = strlen(channel_name);
            if (n) channel_name[n - 1] = '\0';
            channel_name_edited = true;
        } else return false;
        return true;
    }
    if (current_screen == SCREEN_CHANNEL_IMPORT) {
        if (key == BSP_INPUT_NAVIGATION_KEY_UP && !import_pending && import_slot > 1) import_slot--;
        else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN && !import_pending && import_slot < 7) import_slot++;
        else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN && !import_pending) {
            mesh_channel_t channel;
            if (!mesh_radio_get_channel(import_slot, &channel)) {
                mesh_radio_request_channel(import_slot);
                snprintf(import_feedback, sizeof(import_feedback), "Reading slot; press Enter again when loaded.");
            } else if (channel.role) {
                snprintf(import_feedback, sizeof(import_feedback), "Slot occupied. Choose an unused secondary slot.");
            } else if (!mesh_radio_admin_ready()) {
                mesh_radio_request_owner();
                snprintf(import_feedback, sizeof(import_feedback), "Reading radio permissions; retry shortly.");
            } else if (mesh_radio_add_shared_channel(import_slot)) {
                import_pending = true;
                import_revision = channel.revision;
                import_started = esp_timer_get_time();
                snprintf(import_feedback, sizeof(import_feedback), "Adding channel; waiting for radio readback...");
            } else snprintf(import_feedback, sizeof(import_feedback), "No valid shared channel staged for import.");
        } else return false;
        return true;
    }
    return false;
}

