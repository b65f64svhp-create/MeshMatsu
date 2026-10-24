#include "first_use_store.h"
// Guided first use: no firmware flashing, no writes until a reviewed Save.
enum {
  FIRST_WELCOME,
  FIRST_CONNECT,
  FIRST_REGION,
  FIRST_NAMES,
  FIRST_COMPLETE
};
static unsigned first_step, first_field, first_pending;
static bool first_auto, first_read_attempted, first_review, first_confirm,
    first_completed;
static uint32_t first_region, first_node;
static mesh_setup_radio_t first_baseline;
static char first_name[40], first_short[5], first_feedback[160];
static int64_t first_probe_at;
static void open_first_use(bool automatic) {
  current_screen = SCREEN_FIRST_USE;
  first_step = FIRST_WELCOME;
  first_field = first_pending = 0;
  first_auto = automatic;
  first_read_attempted = first_review = first_confirm = false;
  first_probe_at = 0;
  first_node = 0;
  first_feedback[0] = 0;
}
static void first_leave(void) {
  first_review = false;
  if (first_auto)
    go_home();
  else
    go_back();
}
static void first_load_draft(mesh_setup_radio_t radio) {
  first_baseline = radio;
  first_node = radio.node;
  first_region = radio.region;
  snprintf(first_name, sizeof(first_name), "%s", radio.name);
  snprintf(first_short, sizeof(first_short), "%s", radio.short_name);
}
static void first_connect(void) {
  first_step = FIRST_CONNECT;
  first_field = 0;
  first_probe_at = 0;
  first_read_attempted = false;
  first_review = false;
  first_feedback[0] = 0;
}
static bool first_finish_check(void) {
  if (mesh_radio_setup_confirm(first_region, first_name, first_short,
                               first_baseline.revision)) {
    first_pending = 3;
    first_feedback[0] = 0;
    return true;
  }
  snprintf(first_feedback, sizeof(first_feedback),
           "Unable to verify. Reload current radio values and review again.");
  return false;
}
static bool first_region_available(uint32_t region) {
  for (size_t i = 0; i < sizeof(region_options) / sizeof(region_options[0]);
       i++)
    if (region_options[i].value == region)
      return true;
  return false;
}
static void first_next(void) {
  if (first_step == FIRST_REGION) {
    if (!first_region || !first_region_available(first_region)) {
      snprintf(first_feedback, sizeof(first_feedback),
               "Choose your region with Left/Right before continuing.");
      return;
    }
    if (first_region == first_baseline.region) {
      first_step = FIRST_NAMES;
      first_field = 0;
      first_feedback[0] = 0;
      return;
    }
  } else if (first_step == FIRST_NAMES) {
    if (!first_name[0] || !first_short[0]) {
      snprintf(first_feedback, sizeof(first_feedback),
               "Enter both a long name and a short name.");
      return;
    }
    if (!strcmp(first_name, first_baseline.name) &&
        !strcmp(first_short, first_baseline.short_name)) {
      first_finish_check();
      return;
    }
  }
  first_review = true;
  first_confirm = false;
}
static void first_save_review(void) {
  bool sent =
      first_step == FIRST_REGION
          ? mesh_radio_setup_region(first_region, first_baseline.revision)
          : mesh_radio_setup_names(first_name, first_short,
                                   first_baseline.revision);
  first_review = false;
  if (sent) {
    first_pending = first_step == FIRST_REGION ? 1 : 2;
    first_feedback[0] = 0;
  } else
    snprintf(
        first_feedback, sizeof(first_feedback),
        "Not saved. Values may be stale or radio busy. Reload and review.");
}
static bool first_use_poll(void) {
  if (current_screen != SCREEN_FIRST_USE)
    return false;
  mesh_setup_radio_t radio = mesh_radio_setup();
  mesh_radio_status_t link = mesh_radio_status();
  if (first_pending) {
    if (mesh_radio_setup_busy())
      return false;
    unsigned pending = first_pending;
    first_pending = 0;
    if (radio.state != MESH_SETUP_SAVED || !radio.loaded ||
        radio.node != first_node) {
      snprintf(first_feedback, sizeof(first_feedback),
               "Save/verification not confirmed. Reload to check actual radio "
               "values.");
    } else if (pending == 1) {
      first_baseline = radio;
      first_step = FIRST_NAMES;
      first_field = 0;
      first_feedback[0] = 0;
    } else if (pending == 2) {
      first_baseline = radio;
      first_finish_check();
    } else {
      first_baseline = radio;
      first_step = FIRST_COMPLETE;
      first_field = 0;
      if (mesh_first_use_complete() == ESP_OK) {
        first_completed = true;
        first_feedback[0] = 0;
      } else {
        first_completed = false;
        snprintf(
            first_feedback, sizeof(first_feedback),
            "Radio verified. Setup reminder could not be saved. Enter: retry.");
      }
    }
    return true;
  }
  if (first_step == FIRST_CONNECT) {
    if (first_read_attempted && radio.state == MESH_SETUP_READY &&
        radio.loaded && radio.node == mesh_radio_local_node()) {
      first_load_draft(radio);
      first_step = FIRST_REGION;
      first_field = 0;
      first_feedback[0] = 0;
      return true;
    }
    int64_t now = esp_timer_get_time();
    if (now >= first_probe_at) {
      first_probe_at = now + 5000000;
      if (link.synchronized && !link.reconnecting && mesh_radio_local_node()) {
        mesh_radio_request_metadata();
        mesh_radio_request_owner();
        if (!first_read_attempted && mesh_radio_setup_supported() &&
            mesh_radio_setup_read())
          first_read_attempted = true;
      }
      return true;
    }
  } else if ((first_step == FIRST_REGION || first_step == FIRST_NAMES) &&
             (!link.synchronized || link.reconnecting || !radio.loaded ||
              radio.node != first_node ||
              first_node != mesh_radio_local_node() ||
              !mesh_radio_setup_supported())) {
    first_connect();
    snprintf(first_feedback, sizeof(first_feedback),
             "Connection changed. Read and review this radio before saving.");
    return true;
  }
  return false;
}
static bool first_use_navigation(int key) {
  if (current_screen != SCREEN_FIRST_USE)
    return false;
  // Standard Home, Channels, Direct, Nodes, Maps and Screen shortcuts still
  // work.
  if (key == BSP_INPUT_NAVIGATION_KEY_F1 ||
      key == BSP_INPUT_NAVIGATION_KEY_F2 ||
      key == BSP_INPUT_NAVIGATION_KEY_F3 ||
      key == BSP_INPUT_NAVIGATION_KEY_F4 ||
      key == BSP_INPUT_NAVIGATION_KEY_F5 || key == BSP_INPUT_NAVIGATION_KEY_F6)
    return false;
  if (first_pending || mesh_radio_setup_busy()) {
    if (key == BSP_INPUT_NAVIGATION_KEY_ESC) {
      first_leave();
      return true;
    }
    return true;
  }
  if (first_review) {
    if (key == BSP_INPUT_NAVIGATION_KEY_ESC)
      first_review = false;
    else if (key == BSP_INPUT_NAVIGATION_KEY_LEFT)
      first_confirm = false;
    else if (key == BSP_INPUT_NAVIGATION_KEY_RIGHT)
      first_confirm = true;
    else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) {
      if (first_confirm)
        first_save_review();
      else
        first_review = false;
    }
    return true;
  }
  if (key == BSP_INPUT_NAVIGATION_KEY_ESC) {
    if (first_step == FIRST_NAMES) {
      first_step = FIRST_REGION;
      first_field = 0;
    } else if (first_step == FIRST_REGION)
      first_connect();
    else
      first_leave();
    return true;
  }
  unsigned max = first_step == FIRST_WELCOME || first_step == FIRST_CONNECT ? 1
                 : first_step == FIRST_REGION                               ? 2
                 : first_step == FIRST_NAMES                                ? 3
                                                                            : 0;
  if (key == BSP_INPUT_NAVIGATION_KEY_UP) {
    if (first_field)
      first_field--;
  } else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) {
    if (first_field < max)
      first_field++;
  } else if (key == BSP_INPUT_NAVIGATION_KEY_LEFT ||
             key == BSP_INPUT_NAVIGATION_KEY_RIGHT) {
    if (first_step == FIRST_REGION && !first_field) {
      first_region = step_radio_option(
          region_options, sizeof(region_options) / sizeof(region_options[0]),
          first_region, key == BSP_INPUT_NAVIGATION_KEY_RIGHT);
      first_feedback[0] = 0;
    }
  } else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) {
    if (first_step == FIRST_WELCOME) {
      if (first_field)
        first_leave();
      else
        first_connect();
    } else if (first_step == FIRST_CONNECT) {
      if (first_field)
        first_leave();
      else {
        first_read_attempted = false;
        first_probe_at = 0;
        first_feedback[0] = 0;
      }
    } else if (first_step == FIRST_REGION) {
      if (first_field == 0)
        first_field = 1;
      else if (first_field == 1)
        first_next();
      else
        first_connect();
    } else if (first_step == FIRST_NAMES) {
      if (first_field < 2)
        first_field++;
      else if (first_field == 2)
        first_next();
      else
        first_connect();
    } else if (first_completed)
      go_home();
    else {
      first_step = FIRST_NAMES;
      first_finish_check();
    }
  } else if (key == BSP_INPUT_NAVIGATION_KEY_BACKSPACE &&
             first_step == FIRST_NAMES && first_field < 2) {
    mesh_compose_backspace(first_field ? first_short : first_name);
    first_feedback[0] = 0;
  }
  return true;
}
static bool first_use_key(char key) {
  if (current_screen != SCREEN_FIRST_USE)
    return false;
  if (key == '\r' || key == '\n')
    return first_use_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
  if (key == 27)
    return first_use_navigation(BSP_INPUT_NAVIGATION_KEY_ESC);
  if (key == '\t')
    return first_use_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);
  if (key == '\b' || key == 127)
    return first_use_navigation(BSP_INPUT_NAVIGATION_KEY_BACKSPACE);
  if (!first_pending && !mesh_radio_setup_busy() && !first_review &&
      first_step == FIRST_NAMES && first_field < 2 && key >= 32 && key <= 126) {
    char *value = first_field ? first_short : first_name;
    size_t cap = first_field ? sizeof(first_short) : sizeof(first_name),
           n = strlen(value);
    if (n + 1 < cap) {
      value[n] = key;
      value[n + 1] = 0;
      first_feedback[0] = 0;
    } else
      snprintf(first_feedback, sizeof(first_feedback),
               "Name limit: 39 bytes; short name: 4 bytes.");
  }
  return true;
}
