static void neighbor_age(unsigned seconds, char *out, size_t size) {
  if (seconds < 60)
    snprintf(out, size, "%us", seconds);
  else if (seconds < 3600)
    snprintf(out, size, "%um", seconds / 60);
  else if (seconds < 86400)
    snprintf(out, size, "%uh %um", seconds / 3600, seconds / 60 % 60);
  else
    snprintf(out, size, "%ud %uh", seconds / 86400, seconds / 3600 % 24);
}
static const char *neighbor_config_status(mesh_neighbor_config_t c) {
  if (c.state == MESH_NBC_READING)
    return "Reading local radio settings...";
  if (c.state == MESH_NBC_CHECKING)
    return "Checking settings have not changed before saving...";
  if (c.state == MESH_NBC_SAVING || c.state == MESH_NBC_VERIFYING)
    return "Save sent once. Waiting for radio restart/readback...";
  if (c.state == MESH_NBC_SAVED)
    return "Neighbor settings verified on your local radio.";
  if (c.state == MESH_NBC_CHANGED)
    return "Radio settings changed. Reload before editing again.";
  if (c.state == MESH_NBC_TIMEOUT)
    return "No verified response. Reload to check actual settings.";
  if (c.state == MESH_NBC_INTERRUPTED)
    return "Radio connection changed. Reload to check settings.";
  if (c.state == MESH_NBC_FAILED)
    return "Not confirmed. Reload to check actual settings.";
  return NULL;
}
static void neighbor_interval(uint32_t interval, char *out, size_t size) {
  if (!interval)
    snprintf(out, size, "Default (6 hours)");
  else if (!(interval % 3600))
    snprintf(out, size, "%lu hours", (unsigned long)(interval / 3600));
  else
    snprintf(out, size, "%lu seconds", (unsigned long)interval);
}
static void draw_neighbor_info(void) {
  char name[40], line[144];
  peer_name(neighbor_target, name, sizeof(name));
  snprintf(line, sizeof(line), "%.39s / !%08lx%s", name,
           (unsigned long)neighbor_target,
           neighbor_target == mesh_radio_local_node() ? " / YOUR RADIO" : "");
  draw_rich_text(line, 20, 73, 13, logical_h_res - 40, 20, 1, UI_GREEN);
  mesh_neighbor_report_t report;
  bool have = mesh_radio_get_neighbors(neighbor_target, &report);
  if (have) {
    int64_t elapsed = esp_timer_get_time() - report.received_us;
    unsigned age = elapsed > 0 ? (unsigned)(elapsed / 1000000) : 0;
    char interval[32], age_text[32];
    neighbor_interval(report.interval, interval, sizeof(interval));
    uint32_t clock = mesh_radio_time();
    bool radio_age =
        report.timestamp >= 1700000000u && clock >= report.timestamp;
    if (radio_age)
      age = clock - report.timestamp;
    neighbor_age(age, age_text, sizeof(age_text));
    snprintf(line, sizeof(line), "%s %s ago | %u reported neighbors | %s%s",
             radio_age ? "Radio receipt:" : "App receipt:", age_text,
             (unsigned)report.count,
             report.interval ? interval : "Interval not sent",
             report.via_mqtt ? " | via MQTT" : "");
    draw_rich_text(line, 20, 99, 11, logical_h_res - 40, 16, 2, UI_MUTED);
    if (neighbor_scroll >= report.count)
      neighbor_scroll = 0;
    for (size_t i = neighbor_scroll;
         i < report.count && i < neighbor_scroll + 5; i++) {
      int y = 143 + (i - neighbor_scroll) * 38;
      const mesh_neighbor_t *n = &report.neighbors[i];
      char neighbor_name[40];
      peer_name(n->node, neighbor_name, sizeof(neighbor_name));
      pax_draw_rect(&fb, UI_PANEL, 20, y, logical_h_res - 40, 33);
      draw_rich_text(neighbor_name, 32, y + 2, 11, logical_h_res - 210, 15, 1,
                     UI_GREEN);
      snprintf(line, sizeof(line), "!%08lx", (unsigned long)n->node);
      pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 9, 32, y + 21, line);
      char snr[32];
      if (n->has_snr)
        snprintf(snr, sizeof(snr), "SNR %.1f dB", (double)n->snr);
      else
        snprintf(snr, sizeof(snr), "SNR not sent");
      float w = pax_text_size(pax_font_sky_mono, 11, snr).x;
      pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11,
                    logical_h_res - 32 - w, y + 10, snr);
    }
    if (!report.count)
      draw_rich_text("This report contains no neighbors. It does not prove the "
                     "node has no radio coverage.",
                     20, 165, 14, logical_h_res - 40, 23, 4, UI_MUTED);
  } else
    draw_rich_text(
        "No neighbor report received for this node yet. Request one below. The "
        "node needs Neighbor Info enabled and may limit its replies.",
        20, 156, 14, logical_h_res - 40, 25, 5, UI_MUTED);
  mesh_neighbor_request_t request = mesh_radio_neighbor_request();
  const char *feedback = neighbor_feedback;
  if (request.node == neighbor_target && request.state != MESH_NB_IDLE &&
      !neighbor_feedback[0]) {
    if (request.state == MESH_NB_WAITING) {
      snprintf(line, sizeof(line),
               "Waiting for report (%us). A delivery ACK is not a report.",
               request.elapsed);
      feedback = line;
    } else if (request.state == MESH_NB_REPLY)
      feedback = "Requested report received. Links are reported snapshots, not "
                 "a live route.";
    else if (request.state == MESH_NB_TIMEOUT)
      feedback = "No report reply. Module may be disabled, unreachable or "
                 "limiting replies.";
    else if (request.state == MESH_NB_INTERRUPTED)
      feedback = "Request interrupted by radio reconnection. Retry when ready.";
    else
      feedback = "Report request failed. Check the radio/node and retry.";
  }
  draw_rich_text(feedback[0] ? feedback
                             : "Reported links may have changed. Relayed "
                               "reports still describe the reporting node.",
                 20, 339, 10, logical_h_res - 40, 14, 2, UI_MUTED);
  int width = (logical_h_res - 52) / 2;
  const char *buttons[] = {"REQUEST REPORT", "LOCAL MODULE SETTINGS"};
  for (size_t i = 0; i < 2; i++) {
    int x = 20 + i * (width + 12);
    bool selected = neighbor_button == i;
    pax_draw_rect(&fb, selected ? UI_SELECTED : UI_PANEL, x, 377, width, 34);
    pax_draw_rect(&fb, selected ? UI_GREEN : UI_BORDER, x, 377, 3, 34);
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 11, x + 12, 389,
                  buttons[i]);
  }
  pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20,
                logical_v_res - UI_HINT_OFFSET,
                "Up/Down: scroll  L/R or Tab: button  Enter: open  Esc: back");
}
static void draw_neighbor_settings(void) {
  mesh_neighbor_config_t c = mesh_radio_neighbor_config();
  char line[144], interval[32];
  neighbor_interval(neighbor_draft.interval, interval, sizeof(interval));
  if (neighbor_review) {
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 17, 20, 82,
                  "SAVE LOCAL NEIGHBOR SETTINGS?");
    snprintf(line, sizeof(line),
             "Module: %s | Interval: %s | Share over LoRa: %s",
             neighbor_draft.enabled ? "On" : "Off", interval,
             neighbor_draft.over_lora ? "On" : "Off");
    draw_rich_text(line, 20, 124, 13, logical_h_res - 40, 22, 3, WHITE);
    draw_rich_text(
        "Changes only your radio. Enabling collects reports for apps. LoRa "
        "sharing adds primary-channel broadcasts and needs a private key. "
        "Minimum interval: 4 hours. Saving may restart the radio and briefly "
        "pause messaging.",
        20, 198, 13, logical_h_res - 40, 23, 6, UI_MUTED);
    int width = (logical_h_res - 52) / 2;
    for (size_t i = 0; i < 2; i++) {
      int x = 20 + i * (width + 12);
      bool selected = neighbor_confirm == (i != 0);
      pax_draw_rect(&fb, selected ? UI_GREEN : UI_PANEL, x, 359, width, 36);
      pax_draw_text(&fb, selected ? BLACK : UI_GREEN, pax_font_sky_mono, 13,
                    x + 12, 371, i ? "SAVE TO RADIO" : "CANCEL");
    }
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20,
                  logical_v_res - UI_HINT_OFFSET,
                  "L/R: choose  Enter: confirm  Esc: cancel");
    return;
  }
  const char *labels[] = {"NEIGHBOR INFO MODULE", "REPORT INTERVAL",
                          "SHARE OVER LORA", "SAVE LOCAL SETTINGS",
                          "RELOAD LOCAL SETTINGS"};
  const char *values[] = {neighbor_draft.enabled ? "On" : "Off", interval,
                          neighbor_draft.over_lora ? "On" : "Off",
                          "Review changes", "Read again"};
  for (size_t i = 0; i < 5; i++) {
    int y = 76 + i * 47;
    bool selected = neighbor_config_field == i;
    pax_draw_rect(&fb, selected ? UI_SELECTED : UI_PANEL, 20, y,
                  logical_h_res - 40, 42);
    pax_draw_rect(&fb, selected ? UI_GREEN : UI_BORDER, 20, y, 3, 42);
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 32, y + 14, labels[i]);
    const char *value =
        neighbor_config_loaded ? values[i] : (i < 3 ? "Loading..." : values[i]);
    float w = pax_text_size(pax_font_sky_mono, 12, value).x;
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 12, logical_h_res - 32 - w,
                  y + 14, value);
  }
  draw_rich_text("Changes only your radio. Remote nodes need their module "
                 "enabled. Broadcast interval: at least 4 hours. Replies may "
                 "be limited.",
                 20, 321, 11, logical_h_res - 40, 18, 3, UI_MUTED);
  const char *status = neighbor_config_status(c);
  if (neighbor_config_edited && !neighbor_config_pending &&
      (c.state == MESH_NBC_READY || c.state == MESH_NBC_SAVED))
    status = NULL;
  draw_rich_text(status ? status
                 : neighbor_feedback[0]
                     ? neighbor_feedback
                     : "Current settings are kept until you review and Save.",
                 20, 383, 11, logical_h_res - 40, 16, 2,
                 c.state == MESH_NBC_FAILED || c.state == MESH_NBC_CHANGED
                     ? KEY_TRIANGLE_COLOR
                     : UI_MUTED);
  pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20,
                logical_v_res - UI_HINT_OFFSET,
                "Up/Down: select  L/R: change  Enter: action  Esc: back");
}
