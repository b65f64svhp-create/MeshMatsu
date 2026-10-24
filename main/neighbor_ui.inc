static uint32_t neighbor_target;
static size_t neighbor_scroll, neighbor_button, neighbor_config_field;
static bool neighbor_from_node, neighbor_config_loaded, neighbor_config_edited,
    neighbor_review, neighbor_confirm, neighbor_config_pending;
static mesh_neighbor_config_t neighbor_draft;
static char neighbor_feedback[144];
static void open_neighbor_info(uint32_t target) {
  neighbor_from_node = current_screen == SCREEN_NODES;
  neighbor_target = target ? target : mesh_radio_local_node();
  neighbor_scroll = neighbor_button = 0;
  neighbor_feedback[0] = 0;
  current_screen = SCREEN_NEIGHBOR_INFO;
}
static void open_neighbor_settings(void) {
  current_screen = SCREEN_NEIGHBOR_SETTINGS;
  neighbor_config_field = 0;
  neighbor_config_loaded = neighbor_config_edited = neighbor_review =
      neighbor_confirm = false;
  neighbor_config_pending = mesh_radio_neighbor_config_busy();
  neighbor_feedback[0] = 0;
  mesh_radio_request_metadata();
  mesh_radio_request_owner();
  if (!mesh_radio_neighbor_config_read())
    snprintf(neighbor_feedback, sizeof(neighbor_feedback),
             "Wait for the radio, then select Reload Local Settings.");
}
static void neighbor_request_report(void) {
  mesh_neighbor_start_t result = mesh_radio_request_neighbors(neighbor_target);
  char text[144];
  if (result == MESH_NB_START_COOLDOWN)
    snprintf(text, sizeof(text),
             "Wait %u seconds before another report request.",
             mesh_radio_neighbor_cooldown());
  else
    snprintf(text, sizeof(text), "%s",
             result == MESH_NB_START_OK
                 ? "Report requested. Waiting for the node; delivery alone is "
                   "not a report."
             : result == MESH_NB_START_BUSY
                 ? "A neighbor report request is already waiting."
             : result == MESH_NB_START_OFFLINE
                 ? "Radio is reconnecting. Retry after synchronization."
             : result == MESH_NB_START_INVALID
                 ? "Choose a known, unignored node."
                 : "Request was not queued. Check the radio and retry.");
  if (result == MESH_NB_START_OK)
    neighbor_feedback[0] = 0;
  else
    snprintf(neighbor_feedback, sizeof(neighbor_feedback), "%s", text);
}
static void neighbor_config_step(int direction) {
  if (!neighbor_config_loaded || mesh_radio_neighbor_config_busy())
    return;
  if (neighbor_config_field == 0) {
    neighbor_draft.enabled = !neighbor_draft.enabled;
    if (!neighbor_draft.enabled)
      neighbor_draft.over_lora = false;
  } else if (neighbor_config_field == 1) {
    const uint32_t values[] = {0, 14400, 21600, 43200, 86400, 172800};
    size_t i = 0;
    for (; i < 6; i++)
      if (neighbor_draft.interval == values[i])
        break;
    neighbor_draft.interval =
        i == 6 ? 0 : values[(i + (direction > 0 ? 1 : 5)) % 6];
  } else if (neighbor_config_field == 2) {
    if (!neighbor_draft.over_lora &&
        (!neighbor_draft.enabled || !mesh_radio_neighbor_lora_allowed())) {
      snprintf(neighbor_feedback, sizeof(neighbor_feedback),
               "Enable the module first. Radio sharing also requires a primary "
               "channel with a private key.");
      return;
    }
    neighbor_draft.over_lora = !neighbor_draft.over_lora;
  } else
    return;
  neighbor_config_edited = true;
  snprintf(neighbor_feedback, sizeof(neighbor_feedback),
           "Unsaved changes. Save changes only this Tanmatsu's radio.");
}
static bool neighbor_ui_poll(void) {
  if (current_screen != SCREEN_NEIGHBOR_SETTINGS)
    return false;
  mesh_neighbor_config_t c = mesh_radio_neighbor_config();
  if (neighbor_config_pending && !mesh_radio_neighbor_config_busy()) {
    neighbor_config_pending = false;
    if (c.state == MESH_NBC_SAVED)
      neighbor_config_edited = false;
  }
  if (c.loaded && (c.state == MESH_NBC_READY || c.state == MESH_NBC_SAVED) &&
      (!neighbor_config_loaded || !neighbor_config_edited)) {
    bool changed =
        !neighbor_config_loaded || c.revision != neighbor_draft.revision;
    neighbor_draft = c;
    neighbor_config_loaded = true;
    return changed;
  }
  return false;
}
static bool neighbor_navigation(int key) {
  if (current_screen != SCREEN_NEIGHBOR_INFO &&
      current_screen != SCREEN_NEIGHBOR_SETTINGS)
    return false;
  if (key == BSP_INPUT_NAVIGATION_KEY_F1 ||
      key == BSP_INPUT_NAVIGATION_KEY_F2 ||
      key == BSP_INPUT_NAVIGATION_KEY_F3 ||
      key == BSP_INPUT_NAVIGATION_KEY_F4 ||
      key == BSP_INPUT_NAVIGATION_KEY_F5 || key == BSP_INPUT_NAVIGATION_KEY_F6)
    return false;
  bool enter = key == BSP_INPUT_NAVIGATION_KEY_RETURN,
       left = key == BSP_INPUT_NAVIGATION_KEY_LEFT,
       right = key == BSP_INPUT_NAVIGATION_KEY_RIGHT;
  if (key == BSP_INPUT_NAVIGATION_KEY_ESC) {
    if (neighbor_review) {
      neighbor_review = false;
      return true;
    }
    go_back();
    if (current_screen == SCREEN_NODES && neighbor_from_node)
      node_modal = NODE_ACTIONS;
    return true;
  }
  if (current_screen == SCREEN_NEIGHBOR_INFO) {
    mesh_neighbor_report_t report;
    bool have = mesh_radio_get_neighbors(neighbor_target, &report);
    if (key == BSP_INPUT_NAVIGATION_KEY_UP) {
      if (neighbor_scroll)
        neighbor_scroll--;
    } else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) {
      if (have && neighbor_scroll + 5 < report.count)
        neighbor_scroll++;
    } else if (left || right)
      neighbor_button = right ? 1 : 0;
    else if (enter) {
      if (neighbor_button)
        open_neighbor_settings();
      else
        neighbor_request_report();
    }
    return true;
  }
  if (neighbor_review) {
    if (left || right)
      neighbor_confirm = right;
    else if (enter) {
      if (neighbor_confirm) {
        bool ok = mesh_radio_neighbor_config_save(&neighbor_draft);
        neighbor_config_pending = ok;
        snprintf(neighbor_feedback, sizeof(neighbor_feedback), "%s",
                 ok ? "Checking current settings before one save, then "
                      "verifying radio readback..."
                    : "Not saved. Reload settings, wait for radio "
                      "synchronization and retry.");
      }
      neighbor_review = false;
    }
    return true;
  }
  if (key == BSP_INPUT_NAVIGATION_KEY_UP) {
    if (neighbor_config_field)
      neighbor_config_field--;
  } else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) {
    if (neighbor_config_field < 4)
      neighbor_config_field++;
  } else if ((left || right || enter) && neighbor_config_field < 3)
    neighbor_config_step(left ? -1 : 1);
  else if ((enter || right) && neighbor_config_field == 3) {
    if (neighbor_config_loaded && !mesh_radio_neighbor_config_busy()) {
      neighbor_review = true;
      neighbor_confirm = false;
    }
  } else if ((enter || right) && neighbor_config_field == 4) {
    if (mesh_radio_neighbor_config_read()) {
      neighbor_config_loaded = neighbor_config_edited = false;
      neighbor_feedback[0] = 0;
      mesh_radio_request_metadata();
      mesh_radio_request_owner();
    } else
      snprintf(neighbor_feedback, sizeof(neighbor_feedback),
               "Radio busy. Wait for synchronization, then retry Reload.");
  }
  return true;
}
static bool neighbor_key(char key) {
  if (current_screen != SCREEN_NEIGHBOR_INFO &&
      current_screen != SCREEN_NEIGHBOR_SETTINGS)
    return false;
  if (key == '\r' || key == '\n')
    return neighbor_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
  if (key == '\t') {
    if (current_screen == SCREEN_NEIGHBOR_INFO)
      neighbor_button = 1 - neighbor_button;
    else if (!neighbor_review)
      neighbor_config_field = (neighbor_config_field + 1) % 5;
  }
  return true;
}
