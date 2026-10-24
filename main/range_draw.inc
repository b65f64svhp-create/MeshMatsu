static void draw_range_test(void) {
  mesh_range_status_t s = mesh_radio_range_status();
  bool send = rt_review == 1 ? rt_mode == MESH_RANGE_SEND
              : s.origin     ? s.mode == MESH_RANGE_SEND
                             : rt_mode == MESH_RANGE_SEND;
  char text[192];
  const char *state = rt_review == 1                      ? "REVIEW NEW TEST"
                      : s.state == MESH_RANGE_ACTIVE      ? "RUNNING"
                      : s.state == MESH_RANGE_FINISHED    ? "TIMER FINISHED"
                      : s.state == MESH_RANGE_INTERRUPTED ? "RADIO CHANGED"
                                                          : "STOPPED";
  snprintf(text, sizeof(text), "%s | %s | %lu:%02lu elapsed",
           send ? "SENDER" : "RECEIVER", state, (unsigned long)(s.elapsed / 60),
           (unsigned long)(s.elapsed % 60));
  pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 20, 68, text);
  if (rt_review) {
    const char *warning =
        rt_review == 2
            ? "Save the retained range records as a plaintext CSV on the SD "
              "card? It includes node IDs and any available reported "
              "locations, precision and fix times. Existing exports are kept."
        : rt_mode == MESH_RANGE_SEND
            ? "Start timed one-hop broadcasts with no acknowledgements? Range "
              "traffic uses airtime. Sending continues with the screen off and "
              "stops at the timer, Stop, or app Exit. Starting clears previous "
              "session results."
            : "Start a new timed receiving session? Starting clears previous "
              "results. A selected node stays fixed; otherwise the first valid "
              "range packet locks the peer. Counts continue with the screen "
              "off.";
    draw_rich_text(warning, 20, 112, 15, logical_h_res - 40, 26, 7, WHITE);
    if (rt_review == 1) {
      snprintf(text, sizeof(text), "Channel %u | %lu min%s", rt_channel,
               (unsigned long)(rt_duration / 60),
               rt_mode == MESH_RANGE_SEND ? " | direct broadcast only" : "");
      draw_rich_text(text, 20, 302, 13, logical_h_res - 40, 22, 2, UI_GREEN);
    }
    management_buttons(rt_confirm);
  } else {
    const char *labels[3] = {send ? "QUEUED TO RADIO" : "UNIQUE RECEIVED",
                             send ? "SKIPPED" : "ESTIMATED GAPS",
                             send ? "UART FAILURES" : "DUPLICATES"};
    uint32_t counts[3] = {send ? s.queued : s.counter.received,
                          send ? s.skipped : s.counter.gaps,
                          send ? s.write_failures : s.counter.duplicates};
    int w = ((int)logical_h_res - 56) / 3;
    for (unsigned i = 0; i < 3; i++) {
      int x = 20 + i * (w + 8);
      pax_draw_rect(&fb, UI_PANEL, x, 94, w, 53);
      pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 9, x + 10, 101,
                    labels[i]);
      snprintf(text, sizeof(text), "%lu", (unsigned long)counts[i]);
      pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 20, x + 10, 119, text);
    }
    char snr[32], rssi[32];
    if (s.has_last && s.last.has_snr)
      snprintf(snr, sizeof(snr), "SNR %.1f dB", s.last.snr);
    else
      snprintf(snr, sizeof(snr), "SNR unavailable");
    if (s.has_last && s.last.has_rssi)
      snprintf(rssi, sizeof(rssi), "RSSI %ld dBm", (long)s.last.rssi);
    else
      snprintf(rssi, sizeof(rssi), "RSSI unavailable");
    snprintf(text, sizeof(text), "%s | %s", snr, rssi);
    draw_rich_text(text, 20, 158, 12, logical_h_res - 40, 20, 1, UI_GREEN);
    if (s.mode == MESH_RANGE_RECEIVE && s.peer)
      snprintf(text, sizeof(text),
               "Peer !%08lx | segment %lu | seq %lu | late %lu",
               (unsigned long)s.peer, (unsigned long)s.counter.segment,
               (unsigned long)s.last.sequence, (unsigned long)s.counter.late);
    else
      snprintf(
          text, sizeof(text), "%s",
          send ? "Queued is not proof of over-air transmission or reception."
          : rt_peer ? "Waiting for the selected node's direct range packets."
                    : "First direct range packet locks the receiving peer.");
    draw_rich_text(text, 20, 184, 11, logical_h_res - 40, 18, 2, UI_MUTED);
    const char *names[] = {"MODE",           "CHANNEL",
                           "SEND INTERVAL",  "AUTO STOP",
                           "START NEW TEST", "STOP TEST",
                           "EXPORT CSV",     "REFRESH BUILT-IN SENDER CHECK"};
    unsigned first = rt_field > 4 ? rt_field - 4 : 0;
    for (unsigned i = first; i < 8 && i < first + 5; i++) {
      int y = 226 + (i - first) * 28;
      mesh_channel_t c = {0};
      mesh_radio_get_channel(rt_channel, &c);
      if (i == 0)
        snprintf(text, sizeof(text), "%s: %s", names[i],
                 rt_mode == MESH_RANGE_SEND ? "Send broadcasts"
                                            : "Receive / record");
      else if (i == 1)
        snprintf(text, sizeof(text), "%s: %u / %s", names[i], rt_channel,
                 c.name[0] ? c.name : "preset name");
      else if (i == 2)
        snprintf(text, sizeof(text), "%s: %lu sec", names[i],
                 (unsigned long)rt_interval);
      else if (i == 3)
        snprintf(text, sizeof(text), "%s: %lu min", names[i],
                 (unsigned long)(rt_duration / 60));
      else
        snprintf(text, sizeof(text), "%s", names[i]);
      pax_draw_rect(&fb, rt_field == i ? UI_SELECTED : UI_PANEL, 20, y,
                    logical_h_res - 40, 24);
      draw_rich_text(text, 32, y + 5, 11, logical_h_res - 64, 17, 1, UI_GREEN);
    }
    mesh_range_native_t native = mesh_radio_range_native();
    mesh_range_export_status_t export = mesh_range_export_status();
    const char *feedback =
        rt_feedback[0]      ? rt_feedback
        : export.message[0] ? export.message
        : rt_mode == MESH_RANGE_SEND
            ? native.pending ? "Checking the built-in sender..."
              : !native.loaded
                  ? "Refresh the built-in sender check before starting Send."
              : native.enabled && native.sender
                  ? "Built-in radio sender is active. Disable it before "
                    "starting a second sender."
                  : "Built-in sender is off. Recent channel use >=25% skips "
                    "sends; unknown use remains radio-controlled."
            : "Gaps are sequence estimates between observed packets, not proof "
              "of loss. First packet sets the baseline.";
    draw_rich_text(feedback, 20, 373, 10, logical_h_res - 40, 16, 2, UI_MUTED);
  }
  if (rt_review && rt_feedback[0])
    draw_rich_text(rt_feedback, 20, 393, 10, logical_h_res - 40, 16, 1,
                   KEY_TRIANGLE_COLOR);
  pax_draw_text(
      &fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET,
      rt_review ? "Left/Right: choose  Enter: confirm  Esc: cancel"
                : "Arrows: select/change  Enter: action  E: export  S: stop");
}
