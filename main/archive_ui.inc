static size_t archive_field, archive_limit_draft, archive_selection,
    archive_detail_scroll;
static bool archive_enabled_draft, archive_review, archive_confirm,
    archive_clear_review;
static bool archive_all, archive_detail, archive_query_dirty;
static uint32_t archive_peer;
static uint8_t archive_channel;
static char archive_query[MESH_MESSAGE_SEARCH_QUERY_MAX + 1],
    archive_feedback[144];
static int64_t archive_search_at;
static void open_archive_settings(void) {
  current_screen = SCREEN_ARCHIVE_SETTINGS;
  mesh_archive_status_t s = mesh_archive_status();
  archive_enabled_draft = s.enabled;
  archive_limit_draft = s.limit;
  archive_field = 0;
  archive_review = archive_clear_review = archive_confirm = false;
  archive_feedback[0] = 0;
}
static void open_archive_browser(bool all, uint32_t peer, uint8_t channel) {
  current_screen = SCREEN_ARCHIVE_BROWSER;
  archive_all = all;
  archive_peer = peer;
  archive_channel = channel;
  archive_detail = false;
  archive_selection = 0;
  archive_query[0] = 0;
  archive_feedback[0] = 0;
  archive_query_dirty = true;
  archive_search_at = 0;
}
static void archive_submit(void) {
  if (mesh_archive_request(0, archive_query, archive_all, archive_peer,
                           archive_channel)) {
    archive_query_dirty = false;
    archive_selection = 0;
    archive_detail = false;
    archive_feedback[0] = 0;
  }
}
static bool archive_ui_poll(int64_t now) {
  bool changed = mesh_archive_poll(now);
  if (current_screen == SCREEN_ARCHIVE_BROWSER && archive_query_dirty &&
      now >= archive_search_at && !mesh_archive_status().busy) {
    archive_submit();
    changed = true;
  }
  return changed;
}
static void archive_save(void) {
  bool ok = mesh_archive_configure(archive_enabled_draft, archive_limit_draft);
  snprintf(archive_feedback, sizeof(archive_feedback), "%s",
           ok ? "Preferences saved. Applying archive limit in the background."
              : "Not saved. Wait for the archive task, then retry; check "
                "internal storage.");
  archive_review = false;
}
static bool archive_navigation(int key) {
  if (current_screen != SCREEN_ARCHIVE_SETTINGS &&
      current_screen != SCREEN_ARCHIVE_BROWSER)
    return false;
  bool enter = key == BSP_INPUT_NAVIGATION_KEY_RETURN,
       right = key == BSP_INPUT_NAVIGATION_KEY_RIGHT,
       left = key == BSP_INPUT_NAVIGATION_KEY_LEFT;
  if (key == BSP_INPUT_NAVIGATION_KEY_F1 ||
      key == BSP_INPUT_NAVIGATION_KEY_F2 ||
      key == BSP_INPUT_NAVIGATION_KEY_F3 ||
      key == BSP_INPUT_NAVIGATION_KEY_F4 ||
      key == BSP_INPUT_NAVIGATION_KEY_F5 || key == BSP_INPUT_NAVIGATION_KEY_F6)
    return false;
  if (key == BSP_INPUT_NAVIGATION_KEY_ESC) {
    if (archive_review) {
      archive_review = false;
      return true;
    }
    if (current_screen == SCREEN_ARCHIVE_BROWSER && archive_detail) {
      archive_detail = false;
      return true;
    }
    go_back();
    return true;
  }
  if (current_screen == SCREEN_ARCHIVE_SETTINGS) {
    if (archive_review) {
      if (left || right)
        archive_confirm = right;
      else if (enter) {
        if (archive_confirm) {
          if (archive_clear_review) {
            bool ok = mesh_archive_clear();
            snprintf(archive_feedback, sizeof(archive_feedback), "%s",
                     ok ? "Clearing SD archive and pending older messages..."
                        : "Archive busy. Wait and retry Clear.");
            archive_review = false;
          } else
            archive_save();
        } else
          archive_review = false;
      }
      return true;
    }
    if (key == BSP_INPUT_NAVIGATION_KEY_UP) {
      if (archive_field)
        archive_field--;
    } else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) {
      if (archive_field < 4)
        archive_field++;
    } else if ((left || right || enter) && archive_field < 2) {
      if (!archive_field)
        archive_enabled_draft = !archive_enabled_draft;
      else {
        const size_t limits[] = {2000, 5000, 10000};
        for (size_t i = 0; i < 3; i++)
          if (limits[i] == archive_limit_draft) {
            archive_limit_draft = limits[(i + (left ? 2 : 1)) % 3];
            break;
          }
      }
      snprintf(archive_feedback, sizeof(archive_feedback), "Unsaved settings.");
    } else if (enter || right) {
      if (archive_field == 2) {
        mesh_archive_status_t s = mesh_archive_status();
        if ((archive_enabled_draft && !s.enabled) ||
            archive_limit_draft < s.limit) {
          archive_review = true;
          archive_clear_review = false;
          archive_confirm = false;
        } else
          archive_save();
      } else if (archive_field == 3)
        open_archive_browser(true, message_peer, message_channel);
      else if (archive_field == 4) {
        archive_review = archive_clear_review = true;
        archive_confirm = false;
      }
    }
    return true;
  }
  const mesh_archive_page_t *p = mesh_archive_page();
  if (archive_detail) {
    if (key == BSP_INPUT_NAVIGATION_KEY_UP && archive_detail_scroll)
      archive_detail_scroll--;
    else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN &&
             archive_detail_scroll < 233)
      archive_detail_scroll++;
    return true;
  }
  if (key == BSP_INPUT_NAVIGATION_KEY_UP) {
    if (archive_selection)
      archive_selection--;
  } else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) {
    if (p->ready && archive_selection + 1 < p->count)
      archive_selection++;
  } else if (enter) {
    if (archive_query_dirty || !p->ready)
      archive_submit();
    else if (p->ready && archive_selection < p->count) {
      archive_detail = true;
      archive_detail_scroll = 0;
    }
  } else if ((left || right) && p->ready && !archive_query_dirty) {
    if ((left && p->page) ||
        (right && (p->page + 1) * MESH_ARCHIVE_PAGE < p->matches)) {
      if (mesh_archive_request(p->page + (right ? 1 : -1), archive_query,
                               archive_all, archive_peer, archive_channel))
        archive_selection = 0;
    }
  }
  return true;
}
static bool archive_key(char key) {
  if (current_screen != SCREEN_ARCHIVE_SETTINGS &&
      current_screen != SCREEN_ARCHIVE_BROWSER)
    return false;
  if (key == '\r' || key == '\n')
    return archive_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
  if (current_screen == SCREEN_ARCHIVE_SETTINGS) {
    if (key == '\t')
      archive_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);
    return true;
  }
  if (archive_detail)
    return true;
  if (key == '\t')
    archive_all = !archive_all;
  else if (key == '\b' || key == 0x7f)
    mesh_compose_backspace(archive_query);
  else if ((unsigned char)key >= 32) {
    size_t n = strlen(archive_query);
    if (n >= MESH_MESSAGE_SEARCH_QUERY_MAX)
      return true;
    archive_query[n] = key;
    archive_query[n + 1] = 0;
  } else
    return true;
  archive_query_dirty = true;
  archive_search_at = esp_timer_get_time() + 400000;
  archive_selection = 0;
  return true;
}
