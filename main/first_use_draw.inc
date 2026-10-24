static void first_text(const char *text, int y, int lines, pax_col_t color) {
  draw_rich_text(text, 24, y, 12, logical_h_res - 48, 19, lines, color);
}
static void first_row(unsigned field, int y, const char *label,
                      const char *value) {
  bool selected = first_field == field;
  pax_draw_rect(&fb, selected ? UI_SELECTED : UI_PANEL, 20, y,
                logical_h_res - 40, value ? 56 : 38);
  pax_draw_rect(&fb, selected ? UI_GREEN : UI_BORDER, 20, y, 3,
                value ? 56 : 38);
  draw_rich_text(label, 32, y + 7, value ? 10 : 13, logical_h_res - 64, 16, 1,
                 UI_MUTED);
  if (value)
    draw_rich_text(value, 32, y + 28, 12, logical_h_res - 64, 18, 1, UI_GREEN);
}
static void draw_first_use(void) {
  char line[160], region[40];
  mesh_setup_radio_t radio = mesh_radio_setup();
  mesh_radio_metadata_t metadata = mesh_radio_metadata();
  radio_option_name(region_options,
                    sizeof(region_options) / sizeof(region_options[0]),
                    first_region, region, sizeof(region));
  if (!first_region)
    snprintf(region, sizeof(region), "Not set - choose with Left/Right");
  if (first_review) {
    first_text(first_step == FIRST_REGION ? "REVIEW REGION CHANGE"
                                          : "REVIEW RADIO NAMES",
               85, 1, UI_GREEN);
    if (first_step == FIRST_REGION) {
      snprintf(line, sizeof(line), "Region: %s", region);
      first_text(line, 120, 2, WHITE);
    } else {
      snprintf(line, sizeof(line), "Long name: %s", first_name);
      first_text(line, 120, 2, WHITE);
      snprintf(line, sizeof(line), "Short name: %s", first_short);
      first_text(line, 168, 1, WHITE);
    }
    if (first_step == FIRST_REGION) {
      first_text(
          !first_baseline.region
              ? "First region setup enables transmission and initializes radio "
                "keys. The radio applies its regional defaults."
              : "The radio applies regional defaults, including transmit "
                "power.",
          165, 3, KEY_TRIANGLE_COLOR);
      first_text("Saving may restart the radio and briefly pause messaging. "
                 "MeshMatsu waits for a fresh readback.",
                 240, 3, UI_MUTED);
    } else
      first_text(
          "Save changes to this radio? It may restart and briefly pause "
          "messaging. MeshMatsu will read back the result before continuing.",
          218, 4, UI_MUTED);
    pax_draw_rect(&fb, UI_PANEL, 20, 319, logical_h_res - 40, 44);
    first_text(first_confirm ? "Cancel       [ SAVE TO RADIO ]"
                             : "[ CANCEL ]       Save to radio",
               332, 1, UI_GREEN);
    first_text("Left/Right: choose   Enter: confirm   Esc: cancel", 395, 2,
               UI_MUTED);
    return;
  }
  const char *steps[] = {"WELCOME / RADIO FIRMWARE", "CONNECT TO YOUR RADIO",
                         "1 OF 2 / RADIO REGION", "2 OF 2 / RADIO NAMES",
                         "SETUP COMPLETE"};
  first_text(steps[first_step], 80, 1, UI_GREEN);
  if (first_step == FIRST_WELCOME) {
    first_text(
        "Your Tanmatsu ESP32-C6 radio needs the Tanmatsu Meshtastic build. If "
        "it is not installed, flash that radio separately from a computer.",
        113, 4, WHITE);
    first_text("Tested: Tanmatsu Meshtastic 2.7.18. An already working radio "
               "does not need reflashing. MeshMatsu does not flash firmware.",
               196, 4, UI_MUTED);
    first_text("Firmware source: Nicolai-Electronics / tanmatsu-meshtastic-v2 "
               "(GitHub)",
               279, 2, UI_MUTED);
    first_row(0, 332, "BEGIN SETUP", NULL);
    first_row(1, 378, "SET UP LATER", NULL);
  } else if (first_step == FIRST_CONNECT) {
    mesh_radio_status_t link = mesh_radio_status();
    snprintf(
        line, sizeof(line), "Radio: %s",
        !link.synchronized || link.reconnecting ? "waiting for full connection"
        : mesh_radio_setup_supported()          ? "connected / tested version"
                                       : "connected / check firmware version");
    first_text(line, 116, 2, WHITE);
    snprintf(line, sizeof(line), "Reported firmware: %.63s",
             metadata.loaded && metadata.firmware_version[0]
                 ? metadata.firmware_version
                 : "waiting for radio metadata");
    first_text(line, 163, 2, UI_MUTED);
    if (first_read_attempted && mesh_radio_setup_busy())
      first_text("Reading your current region and radio names...", 211, 2,
                 UI_GREEN);
    else if (first_read_attempted && radio.state != MESH_SETUP_READY)
      first_text("No complete settings reply. Check the connection, then "
                 "Retry. No setup values were written.",
                 211, 3, KEY_TRIANGLE_COLOR);
    else
      first_text(
          "Setup supports the tested Tanmatsu 2.7.18 build. After a confirmed "
          "connection, your current settings will appear here.",
          211, 3, UI_MUTED);
    first_row(0, 303, "RETRY CONNECTION / READ SETTINGS", NULL);
    first_row(1, 349, "SET UP LATER", NULL);
  } else if (first_step == FIRST_REGION) {
    first_text(
        "Choose the radio region for where you will use it. Your existing "
        "region is preselected; setup does not guess your location.",
        111, 3, UI_MUTED);
    first_row(0, 183, "REGION / LEFT-RIGHT TO CHOOSE", region);
    first_row(1, 253,
              first_region == first_baseline.region ? "KEEP REGION & CONTINUE"
                                                    : "SAVE REGION & CONTINUE",
              NULL);
    first_row(2, 301, "RELOAD CURRENT RADIO VALUES", NULL);
    first_text("Modem preset, hop limit and channels stay as configured.", 353,
               2, UI_MUTED);
  } else if (first_step == FIRST_NAMES) {
    first_text("These names identify your radio on the mesh. Edit the current "
               "text, or keep both names and finish.",
               111, 3, UI_MUTED);
    first_row(0, 174, "LONG NAME / UP TO 39 BYTES",
              first_name[0] ? first_name : "(enter a name)");
    first_row(1, 237, "SHORT NAME / UP TO 4 BYTES",
              first_short[0] ? first_short : "(enter a short name)");
    first_row(2, 306,
              !strcmp(first_name, first_baseline.name) &&
                      !strcmp(first_short, first_baseline.short_name)
                  ? "KEEP NAMES & FINISH"
                  : "SAVE NAMES & FINISH",
              NULL);
    first_row(3, 352, "RELOAD CURRENT RADIO VALUES", NULL);
  } else {
    first_text("Your region and radio names were confirmed by fresh replies "
               "from your radio.",
               116, 3, WHITE);
    snprintf(line, sizeof(line), "Region: %s", region);
    first_text(line, 184, 2, UI_GREEN);
    snprintf(line, sizeof(line), "%s / %s", first_name, first_short);
    first_text(line, 229, 2, UI_GREEN);
    first_text("Run setup again from Settings > First-Time Setup. Channels and "
               "other settings remain available in Settings.",
               277, 3, UI_MUTED);
    first_row(0, 349, first_completed ? "OPEN HOME" : "RETRY FINISHING SETUP",
              NULL);
  }
  const char *feedback = first_feedback;
  if (first_pending)
    feedback = radio.state == MESH_SETUP_CHECKING
                   ? "Checking current radio values before continuing..."
                   : "Waiting for radio restart / verified readback...";
  if (feedback[0])
    first_text(feedback, first_step == FIRST_WELCOME ? 420 : 397, 2,
               KEY_TRIANGLE_COLOR);
  else if (first_step != FIRST_WELCOME)
    draw_rich_text("Up/Down: select | Enter: open | Esc: back", 24, 410, 10,
                   logical_h_res - 48, 16, 1, UI_MUTED);
}
