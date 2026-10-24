static void archive_stamp(uint32_t timestamp, char *out, size_t size) {
  char stamp[32];
  mesh_time_stamp(&time_settings, timestamp, stamp, sizeof(stamp));
  time_t value = timestamp;
  struct tm local;
  if (timestamp >= 1700000000u && localtime_r(&value, &local))
    snprintf(out, size, "%04d %s", local.tm_year + 1900, stamp);
  else
    snprintf(out, size, "%s", stamp);
}
typedef struct {
  rich_text_context_t base;
  int first;
} archive_paint_t;
static void archive_body_token(const mesh_text_token_t *token, int x, int y,
                               void *context) {
  archive_paint_t *p = context;
  int line = y / 23;
  if (line >= p->first && line < p->first + 6)
    draw_rich_token(token, x, y - p->first * 23, &p->base);
}
static void draw_archive_settings(void) {
  mesh_archive_status_t s = mesh_archive_status();
  char line[144];
  if (archive_review) {
    draw_rich_text(archive_clear_review ? "CLEAR ARCHIVE?"
                                        : "SAVE ARCHIVE SETTINGS?",
                   20, 84, 17, logical_h_res - 40, 24, 1, UI_GREEN);
    draw_rich_text(
        archive_clear_review
            ? "Permanently remove all SD archive messages and older messages "
              "waiting internally. Recent chats, drafts and unread counts stay "
              "intact. The SD card must be present."
            : "Archived messages and tapbacks are readable without a password "
              "on the SD card. Up to 1,024 older messages can wait internally "
              "when the card is unavailable. Lowering the limit removes the "
              "oldest archived entries. Recent-chat deletion does not erase "
              "the archive.",
        20, 132, 14, logical_h_res - 40, 25, 7, WHITE);
    int width = (logical_h_res - 52) / 2;
    for (unsigned i = 0; i < 2; i++) {
      bool selected = archive_confirm == (i != 0);
      int x = 20 + i * (width + 12);
      pax_draw_rect(&fb, selected ? UI_GREEN : UI_PANEL, x, 350, width, 36);
      pax_draw_text(
          &fb, selected ? BLACK : UI_GREEN, pax_font_sky_mono, 13, x + 12, 361,
          i ? (archive_clear_review ? "CLEAR ARCHIVE" : "SAVE SETTINGS")
            : "CANCEL");
    }
    draw_rich_text("L/R: choose  Enter: confirm  Esc: cancel", 20, 403, 11,
                   logical_h_res - 40, 16, 1, UI_MUTED);
    return;
  }
  const char *labels[] = {"ARCHIVE OLDER MESSAGES", "MAXIMUM ON SD",
                          "SAVE ARCHIVE SETTINGS", "BROWSE / SEARCH ARCHIVE",
                          "CLEAR ARCHIVE & PENDING"};
  for (size_t i = 0; i < 5; i++) {
    int y = 76 + i * 47;
    bool selected = archive_field == i;
    pax_draw_rect(&fb, selected ? UI_SELECTED : UI_PANEL, 20, y,
                  logical_h_res - 40, 42);
    pax_draw_rect(&fb, selected ? UI_GREEN : UI_BORDER, 20, y, 3, 42);
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 12, 32, y + 14, labels[i]);
    if (i < 2) {
      if (i)
        snprintf(line, sizeof(line), "%u messages",
                 (unsigned)archive_limit_draft);
      else
        snprintf(line, sizeof(line), "%s",
                 archive_enabled_draft ? "On" : "Off");
      float width = pax_text_size(pax_font_sky_mono, 12, line).x;
      pax_draw_text(&fb, WHITE, pax_font_sky_mono, 12,
                    logical_h_res - 32 - width, y + 14, line);
    }
  }
  snprintf(line, sizeof(line), "SD: %u / %u messages, %.2f MiB | %s",
           (unsigned)s.count, (unsigned)s.limit, (double)s.bytes / 1048576,
           s.busy        ? "working"
           : s.available ? "available"
                         : "unavailable / unchecked");
  draw_rich_text(line, 20, 321, 11, logical_h_res - 40, 16, 1, UI_MUTED);
  snprintf(line, sizeof(line),
           "Waiting internally: %u | Unsaved queue: %u | Not archived this "
           "session: %u",
           (unsigned)s.pending, (unsigned)s.volatile_count, (unsigned)s.missed);
  draw_rich_text(line, 20, 342, 11, logical_h_res - 40, 16, 2,
                 s.missed ? KEY_TRIANGLE_COLOR : UI_MUTED);
  draw_rich_text(archive_feedback[0] ? archive_feedback : s.feedback, 20, 382,
                 11, logical_h_res - 40, 16, 2,
                 s.failed ? KEY_TRIANGLE_COLOR : UI_MUTED);
  pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20,
                logical_v_res - UI_HINT_OFFSET,
                "Up/Down: choose  L/R: change  Enter: open/save  Esc: back");
}
static void draw_archive_browser(void) {
  const mesh_archive_page_t *p = mesh_archive_page();
  mesh_archive_status_t s = mesh_archive_status();
  char line[144];
  if (archive_detail && p->ready && archive_selection < p->count) {
    const mesh_archive_row_t *r = &p->rows[archive_selection];
    draw_rich_text(r->sender, 20, 77, 16, logical_h_res - 40, 22, 1, UI_GREEN);
    char stamp[48];
    archive_stamp(r->message.timestamp, stamp, sizeof(stamp));
    if (r->message.peer_node)
      snprintf(line, sizeof(line), "%s | DM !%08lx | %s",
               r->message.outgoing ? "Sent" : "Received",
               (unsigned long)r->message.peer_node, stamp);
    else
      snprintf(line, sizeof(line), "%s | Channel %u | %s",
               r->message.outgoing ? "Sent" : "Received", r->message.channel,
               stamp);
    draw_rich_text(line, 20, 108, 11, logical_h_res - 40, 18, 2, UI_MUTED);
    pax_draw_rect(&fb, UI_PANEL, 20, 153, logical_h_res - 40, 161);
    int body_lines = mesh_text_line_count(r->message.text, logical_h_res - 64,
                                          rich_text_advance(14));
    size_t last = body_lines > 6 ? (size_t)body_lines - 6 : 0;
    if (archive_detail_scroll > last)
      archive_detail_scroll = last;
    archive_paint_t paint = {{32, 166, 14, WHITE}, (int)archive_detail_scroll};
    mesh_text_layout(r->message.text, logical_h_res - 64, rich_text_advance(14),
                     23, 1024, archive_body_token, &paint);
    snprintf(line, sizeof(line), "Packet %lu / reply %lu",
             (unsigned long)r->message.packet_id,
             (unsigned long)r->message.reply_id);
    draw_rich_text(line, 20, 326, 10, logical_h_res - 40, 15, 1, UI_MUTED);
    draw_rich_text(r->message.outgoing ? mesh_radio_delivery_text(&r->message)
                                       : "Received message",
                   20, 346, 11, logical_h_res - 40, 17, 1, UI_GREEN);
    if (r->reaction_count) {
      snprintf(line, sizeof(line), "Tapbacks at archive time: %u",
               (unsigned)r->reaction_count);
      draw_rich_text(line, 20, 369, 11, logical_h_res - 40, 16, 1, UI_MUTED);
      draw_rich_text(r->reactions, 20, 390, 13, logical_h_res - 40, 20, 1,
                     WHITE);
    }
    snprintf(line, sizeof(line),
             "Up/Down: text %u-%u/%u  Esc: results  Red X: Home",
             (unsigned)archive_detail_scroll + 1,
             (unsigned)(archive_detail_scroll + 6 < (size_t)body_lines
                            ? archive_detail_scroll + 6
                            : (size_t)body_lines),
             (unsigned)body_lines);
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20,
                  logical_v_res - UI_HINT_OFFSET, line);
    return;
  }
  if (archive_all)
    snprintf(line, sizeof(line), "ALL ARCHIVED CHATS");
  else if (archive_peer)
    snprintf(line, sizeof(line), "DIRECT CHAT !%08lx",
             (unsigned long)archive_peer);
  else
    snprintf(line, sizeof(line), "CHANNEL SLOT %u", archive_channel);
  draw_rich_text(line, 20, 65, 11, logical_h_res - 40, 16, 1, UI_MUTED);
  pax_draw_rect(&fb, UI_SELECTED, 20, 87, logical_h_res - 40, 36);
  draw_rich_text(archive_query[0]
                     ? archive_query
                     : "Type text, saved sender name or node ID...",
                 32, 98, 12, logical_h_res - 64, 18, 1, WHITE);
  if (archive_query_dirty || !p->ready)
    draw_rich_text(s.busy ? "Reading archive in the background..." : s.feedback,
                   20, 164, 13, logical_h_res - 40, 22, 5, UI_MUTED);
  else {
    size_t first = archive_selection / 4 * 4;
    for (size_t i = first; i < p->count && i < first + 4; i++) {
      const mesh_archive_row_t *r = &p->rows[i];
      int y = 137 + (i - first) * 62;
      pax_draw_rect(&fb, i == archive_selection ? UI_SELECTED : UI_PANEL, 20, y,
                    logical_h_res - 40, 56);
      pax_draw_rect(&fb, i == archive_selection ? UI_GREEN : UI_BORDER, 20, y,
                    3, 56);
      char stamp[48];
      archive_stamp(r->message.timestamp, stamp, sizeof(stamp));
      snprintf(line, sizeof(line), "%.39s / %s", r->sender, stamp);
      draw_rich_text(line, 32, y + 6, 11, logical_h_res - 64, 16, 1, UI_GREEN);
      draw_rich_text(r->message.text, 32, y + 29, 12, logical_h_res - 64, 18, 1,
                     WHITE);
    }
    snprintf(line, sizeof(line), "%u matches | Batch %u / %u | %u-%u",
             (unsigned)p->matches, (unsigned)p->page + 1,
             (unsigned)(p->matches ? (p->matches + MESH_ARCHIVE_PAGE - 1) /
                                         MESH_ARCHIVE_PAGE
                                   : 1),
             (unsigned)(p->count ? p->page * MESH_ARCHIVE_PAGE + first + 1 : 0),
             (unsigned)(p->count
                            ? p->page * MESH_ARCHIVE_PAGE +
                                  (first + 4 < p->count ? first + 4 : p->count)
                            : 0));
    draw_rich_text(p->matches ? line
                              : "No archived matches. Messages appear here "
                                "after leaving recent history.",
                   20, 390, 11, logical_h_res - 40, 17, 2, UI_MUTED);
  }
  pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20,
                logical_v_res - UI_HINT_OFFSET,
                "Type: find  Up/Dn: pick  L/R: page  Enter: view  Tab: scope");
}
