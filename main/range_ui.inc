static mesh_range_mode_t rt_mode;
static uint8_t rt_channel;
static uint32_t rt_peer, rt_interval = 60, rt_duration = 900;
static unsigned rt_field, rt_review;
static bool rt_confirm, rt_from_node;
static char rt_feedback[144];
static void open_range_testing(uint32_t peer) {
  rt_from_node = current_screen == SCREEN_NODES;
  current_screen = SCREEN_RANGE_TEST;
  rt_field = rt_review = 0;
  rt_confirm = false;
  rt_feedback[0] = 0;
  mesh_range_status_t status = mesh_radio_range_status();
  rt_peer = peer;
  if (status.state == MESH_RANGE_ACTIVE) {
    rt_mode = status.mode;
    rt_channel = status.channel;
    rt_interval = status.interval;
    rt_duration = status.duration;
    rt_peer = status.peer;
  } else {
    rt_mode = MESH_RANGE_RECEIVE;
    rt_interval = 60;
    rt_duration = 900;
    rt_channel = 0;
    for (uint8_t i = 0; i < 8; i++) {
      mesh_channel_t c;
      if (mesh_radio_get_channel(i, &c) && c.role == 1) {
        rt_channel = i;
        break;
      }
    }
  }
  mesh_radio_range_refresh();
}
static void rt_export_review(void) {
  rt_review = 2;
  rt_confirm = false;
  rt_feedback[0] = 0;
}
static void rt_execute(void) {
  if (rt_review == 1) {
    if (mesh_radio_range_start(rt_mode, rt_channel, rt_peer, rt_interval,
                               rt_duration)) {
      rt_review = 0;
      rt_feedback[0] = 0;
    } else
      snprintf(rt_feedback, sizeof(rt_feedback),
               "Not started. Stop any current test, check radio/channel, and "
               "refresh the built-in sender check before sending.");
  } else if (rt_review == 2) {
    mesh_range_export_start_t result = mesh_range_export_start();
    rt_review = 0;
    snprintf(rt_feedback, sizeof(rt_feedback), "%s",
             result == MESH_RANGE_EXPORT_OK
                 ? "Exporting a snapshot to SD; messaging continues."
             : result == MESH_RANGE_EXPORT_EMPTY
                 ? "No range records to export yet."
             : result == MESH_RANGE_EXPORT_BUSY
                 ? "A range export is already running."
                 : "Not exported: memory unavailable. Retry after another task "
                   "finishes.");
  }
}
static bool range_navigation(int key) {
  if (current_screen != SCREEN_RANGE_TEST)
    return false;
  if (key == BSP_INPUT_NAVIGATION_KEY_F1 ||
      key == BSP_INPUT_NAVIGATION_KEY_F2 ||
      key == BSP_INPUT_NAVIGATION_KEY_F3 ||
      key == BSP_INPUT_NAVIGATION_KEY_F4 || key == BSP_INPUT_NAVIGATION_KEY_F6)
    return false;
  bool enter = key == BSP_INPUT_NAVIGATION_KEY_RETURN,
       right = key == BSP_INPUT_NAVIGATION_KEY_RIGHT,
       left = key == BSP_INPUT_NAVIGATION_KEY_LEFT;
  if (key == BSP_INPUT_NAVIGATION_KEY_ESC) {
    if (rt_review) {
      rt_review = 0;
      return true;
    }
    go_back();
    if (rt_from_node && current_screen == SCREEN_NODES)
      node_modal = NODE_ACTIONS;
    return true;
  }
  if (rt_review) {
    if (left || right)
      rt_confirm = right;
    else if (enter) {
      if (rt_confirm)
        rt_execute();
      else
        rt_review = 0;
    }
    return true;
  }
  mesh_range_status_t status = mesh_radio_range_status();
  if (key == BSP_INPUT_NAVIGATION_KEY_UP) {
    if (rt_field)
      rt_field--;
  } else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) {
    if (rt_field < 7)
      rt_field++;
  } else if ((left || right) && rt_field < 4) {
    if (status.state == MESH_RANGE_ACTIVE) {
      snprintf(rt_feedback, sizeof(rt_feedback),
               "Stop the active test before changing its settings.");
      return true;
    }
    if (!rt_field)
      rt_mode =
          rt_mode == MESH_RANGE_SEND ? MESH_RANGE_RECEIVE : MESH_RANGE_SEND;
    else if (rt_field == 1) {
      for (unsigned i = 0; i < 8; i++) {
        rt_channel = (rt_channel + 8 + (right ? 1 : -1)) % 8;
        mesh_channel_t c;
        if (mesh_radio_get_channel(rt_channel, &c) && c.role)
          break;
      }
    } else {
      const uint32_t intervals[] = {30, 60, 120, 300},
                     durations[] = {300, 900, 1800, 3600};
      const uint32_t *values = rt_field == 2 ? intervals : durations;
      uint32_t *v = rt_field == 2 ? &rt_interval : &rt_duration;
      size_t i = 0;
      for (; i < 4; i++)
        if (values[i] == *v)
          break;
      *v = values[(i + (right ? 1 : 3)) % 4];
    }
    rt_feedback[0] = 0;
  } else if (enter || (right && rt_field >= 4)) {
    if (rt_field < 4)
      return range_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);
    if (rt_field == 4) {
      if (status.state == MESH_RANGE_ACTIVE)
        snprintf(rt_feedback, sizeof(rt_feedback),
                 "A test is active. Select Stop first.");
      else {
        rt_review = 1;
        rt_confirm = false;
        rt_feedback[0] = 0;
      }
    } else if (rt_field == 5) {
      mesh_radio_range_stop();
      snprintf(rt_feedback, sizeof(rt_feedback),
               "Test stopped. Results stay until another test starts or the "
               "app exits.");
    } else if (rt_field == 6)
      rt_export_review();
    else {
      bool ok = mesh_radio_range_refresh();
      snprintf(
          rt_feedback, sizeof(rt_feedback), "%s",
          ok ? "Checking the radio's built-in sender (no settings are changed)."
             : "Check unavailable. Wait for radio synchronization or a pending "
               "check.");
    }
  }
  return true;
}
static bool range_key(char key) {
  if (current_screen != SCREEN_RANGE_TEST)
    return false;
  if (key == '\r' || key == '\n')
    return range_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
  if (key == '\t')
    return range_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);
  if (!rt_review && (key == 'e' || key == 'E'))
    rt_export_review();
  else if (!rt_review && (key == 's' || key == 'S')) {
    mesh_radio_range_stop();
    snprintf(rt_feedback, sizeof(rt_feedback),
             "Test stopped; results retained for export.");
  }
  return true;
}
