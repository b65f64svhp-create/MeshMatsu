static bool manage_busy_ui(mesh_manage_state_t s) {
  return s == MESH_MANAGE_CHECKING || s == MESH_MANAGE_READING ||
         s == MESH_MANAGE_SAVING || s == MESH_MANAGE_VERIFYING;
}
// UI drafts never alias the radio snapshots or their session keys.
enum {
  SC_MENU,
  SC_QR,
  SC_COMPARE,
  SC_LINK,
  SC_ADMINS,
  SC_ADMIN_KEY,
  SC_REVIEW,
  SC_WAIT
};

enum { SC_VERIFY, SC_IMPORT, SC_ADMIN_SET, SC_ADMIN_REMOVE };

static unsigned sc_mode, sc_action, sc_selection, sc_slot;

static bool management_from_node;
static bool sc_confirm, sc_have_contact, sc_replace;

static uint32_t sc_target, sc_revision;

static mesh_contact_t sc_contact;

static uint8_t sc_admin_key[32];

static char sc_link[MESH_CONTACT_URL_MAX], sc_key_text[65], sc_feedback[128];

static uint8_t sc_qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)],
    sc_qr_temp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];

static bool sc_qr_valid;

static unsigned rm_field, rm_action;

static bool rm_confirm, rm_review, rm_replace, rm_loaded;

static mesh_remote_t rm_baseline;

static char rm_name[40], rm_short[5], rm_feedback[128];

static uint32_t rm_hops, rm_role, rm_target;

static const char *manage_status_text(mesh_manage_state_t state) {
  switch (state) {
  case MESH_MANAGE_IDLE:
    return "Select Refresh to read current values.";

  case MESH_MANAGE_READING:
    return "Reading settings; waiting for the radio...";

  case MESH_MANAGE_CHECKING:
    return "Checking reviewed values before applying the change...";
  case MESH_MANAGE_READY:
    return "Current values loaded. Changes require review.";

  case MESH_MANAGE_SAVING:
    return "Sent once; waiting to verify the result...";

  case MESH_MANAGE_VERIFYING:
    return "Checking a fresh response from the target radio...";

  case MESH_MANAGE_SAVED:
    return "Change confirmed by fresh radio readback. Refresh before another "
           "change.";

  case MESH_MANAGE_REBOOT_ACK:
    return "Radio acknowledged reboot. Restart itself is not verified.";

  case MESH_MANAGE_TIMEOUT:
    return "Timed out; result unconfirmed. Refresh before retrying.";

  case MESH_MANAGE_INTERRUPTED:
    return "Interrupted: connection, radio identity or public key changed.";

  default:
    return "Radio did not confirm this request. Refresh and check its current "
           "values.";
  }
}
static const char *trust_label(mesh_trust_t trust) {
  return trust == MESH_TRUST_CHANGED ? "KEY CHANGED - COMPARE BEFORE TRUSTING"
         : trust == MESH_TRUST_VERIFIED ? "KEY MANUALLY VERIFIED"
         : trust == MESH_TRUST_SEEN     ? "KEY SEEN - NOT MANUALLY VERIFIED"
                                        : "NO SAVED KEY BASELINE";
}
static void open_security_centre(uint32_t target) {
  management_from_node = current_screen == SCREEN_NODES;
  current_screen = SCREEN_SECURITY;
  sc_target = target;
  sc_mode = SC_MENU;
  sc_selection = 0;
  sc_feedback[0] = 0;
  sc_have_contact = mesh_radio_contact(target, &sc_contact);
  sc_link[0] = 0;
  sc_qr_valid = false;
}
static void open_remote_manager(uint32_t target) {
  management_from_node = current_screen == SCREEN_NODES;
  current_screen = SCREEN_REMOTE;
  rm_target = target;
  rm_field = 0;
  rm_loaded = rm_review = rm_confirm = false;
  rm_feedback[0] = 0;

  mesh_remote_t state = mesh_radio_remote();
  if (state.node == target && manage_busy_ui(state.state))
    return;

  if (!mesh_radio_remote_read(target))
    snprintf(rm_feedback, sizeof(rm_feedback),
             "Open Security / Contact and verify this node's key first. The "
             "remote radio must authorize your public key.");
}
static bool management_shortcut(int key) {
  return key == BSP_INPUT_NAVIGATION_KEY_F1 ||
         key == BSP_INPUT_NAVIGATION_KEY_F2 ||
         key == BSP_INPUT_NAVIGATION_KEY_F3 ||
         key == BSP_INPUT_NAVIGATION_KEY_F4 ||
         key == BSP_INPUT_NAVIGATION_KEY_F6;
}
static void sc_review(unsigned action) {
  sc_action = action;
  sc_mode = SC_REVIEW;
  sc_confirm = false;
  sc_feedback[0] = 0;
}
static void sc_execute(void) {
  bool ok = false;

  if (sc_action == SC_VERIFY) {
    ok = mesh_radio_verify_contact(sc_target, sc_contact.key);
    if (ok) {
      sc_mode = SC_COMPARE;
      snprintf(sc_feedback, sizeof(sc_feedback),
               "Verified key saved on this Tanmatsu.");
      return;
    }
  } else if (sc_action == SC_IMPORT)
    ok = mesh_radio_import_contact(&sc_contact);

  else
    ok = mesh_radio_security_set_admin(
        sc_slot, sc_admin_key, sc_action == SC_ADMIN_REMOVE, sc_revision);

  if (ok) {
    sc_mode = SC_WAIT;
    sc_feedback[0] = 0;
  } else
    snprintf(sc_feedback, sizeof(sc_feedback),
             "Not applied. Values may be stale, storage unavailable or another "
             "change pending. Refresh and review again.");
}
static bool security_navigation(int key) {
  if (current_screen != SCREEN_SECURITY)
    return false;
  if (management_shortcut(key))
    return false;

  bool enter = key == BSP_INPUT_NAVIGATION_KEY_RETURN,
       right = key == BSP_INPUT_NAVIGATION_KEY_RIGHT,
       left = key == BSP_INPUT_NAVIGATION_KEY_LEFT;

  if (key == BSP_INPUT_NAVIGATION_KEY_ESC) {
    if (sc_mode == SC_MENU) {
      go_back();
      if (management_from_node && current_screen == SCREEN_NODES)
        node_modal = NODE_ACTIONS;
      return true;
    }
    sc_mode = SC_MENU;
    sc_feedback[0] = 0;
    return true;
  }
  if (sc_mode == SC_REVIEW) {
    if (left || right)
      sc_confirm = right;
    else if (enter) {
      if (sc_confirm)
        sc_execute();
      else
        sc_mode = SC_MENU;
    }
    return true;
  }
  if (sc_mode == SC_MENU) {
    if (key == BSP_INPUT_NAVIGATION_KEY_UP && sc_selection)
      sc_selection--;
    else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN && sc_selection < 3)
      sc_selection++;

    else if (enter || right) {
      sc_feedback[0] = 0;

      if (sc_selection < 2) {
        sc_have_contact = mesh_radio_contact(sc_target, &sc_contact);
        if (!sc_have_contact) {
          snprintf(sc_feedback, sizeof(sc_feedback),
                   "Public identity unavailable. Request Node Info and retry.");
          return true;
        }
        if (!sc_selection) {
          sc_qr_valid =
              mesh_radio_contact_url(&sc_contact, sc_link, sizeof(sc_link)) &&
              qrcodegen_encodeText(sc_link, sc_qr_temp, sc_qr,
                                   qrcodegen_Ecc_MEDIUM, 1, 10,
                                   qrcodegen_Mask_AUTO, true);
          sc_mode = SC_QR;
        } else
          sc_mode = SC_COMPARE;
      } else if (sc_selection == 2) {
        sc_link[0] = 0;
        sc_mode = SC_LINK;
      } else {
        sc_mode = SC_ADMINS;
        sc_slot = 0;
        if (!mesh_radio_security_read())
          snprintf(sc_feedback, sizeof(sc_feedback),
                   "Could not start a fresh local Security read. Wait for "
                   "synchronization or another operation to finish.");
      }
    }
  } else if (sc_mode == SC_COMPARE && enter) {
    sc_review(SC_VERIFY);
  } else if (sc_mode == SC_LINK && enter) {
    if (mesh_radio_parse_contact_url(sc_link, &sc_contact) &&
        sc_contact.number != mesh_radio_local_node())
      sc_review(SC_IMPORT);
    else
      snprintf(sc_feedback, sizeof(sc_feedback),
               "Invalid public contact link. Use https://meshtastic.org/v/#... "
               "(not a channel link).");
  } else if (sc_mode == SC_ADMINS) {
    mesh_security_t state = mesh_radio_security();

    if (key == BSP_INPUT_NAVIGATION_KEY_UP && sc_slot)
      sc_slot--;
    else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN && sc_slot < 3)
      sc_slot++;

    else if (enter || right) {
      if (sc_slot == 3) {
        sc_feedback[0] = 0;
        if (!mesh_radio_security_read())
          snprintf(
              sc_feedback, sizeof(sc_feedback),
              "Refresh unavailable while another radio action is pending.");
      } else if (state.state == MESH_MANAGE_READY &&
                 sc_slot <= state.admin_count) {
        sc_revision = state.revision;
        sc_key_text[0] = 0;
        sc_replace = true;
        sc_mode = SC_ADMIN_KEY;
      } else
        snprintf(sc_feedback, sizeof(sc_feedback),
                 "Refresh first. Fill admin slots in order.");
    }
  } else if (sc_mode == SC_ADMIN_KEY && enter) {
    if (mesh_contact_key_parse(sc_key_text, sc_admin_key))
      sc_review(SC_ADMIN_SET);
    else
      snprintf(
          sc_feedback, sizeof(sc_feedback),
          "Enter one 32-byte public key: Base64 or 64 hexadecimal characters.");
  } else if (sc_mode == SC_WAIT && enter) {
    if (!manage_busy_ui(mesh_radio_security().state))
      sc_mode = SC_MENU;
  }
  return true;
}
static void remote_refresh_ui(void) {
  mesh_remote_t state = mesh_radio_remote();
  if (state.node != rm_target)
    return;

  if (state.state == MESH_MANAGE_READY &&
      (!rm_loaded || state.revision != rm_baseline.revision)) {
    rm_baseline = state;
    snprintf(rm_name, sizeof(rm_name), "%s", state.name);
    snprintf(rm_short, sizeof(rm_short), "%s", state.short_name);
    rm_role = state.role;
    rm_hops = state.hops;
    rm_loaded = true;
    rm_replace = true;
  }
}
static bool remote_navigation(int key) {
  if (current_screen != SCREEN_REMOTE)
    return false;
  if (management_shortcut(key))
    return false;

  remote_refresh_ui();
  bool enter = key == BSP_INPUT_NAVIGATION_KEY_RETURN,
       right = key == BSP_INPUT_NAVIGATION_KEY_RIGHT,
       left = key == BSP_INPUT_NAVIGATION_KEY_LEFT;

  if (key == BSP_INPUT_NAVIGATION_KEY_ESC) {
    if (!rm_review) {
      go_back();
      if (management_from_node && current_screen == SCREEN_NODES)
        node_modal = NODE_ACTIONS;
      return true;
    }
    rm_review = false;
    return true;
  }
  if (rm_review) {
    if (left || right)
      rm_confirm = right;
    else if (enter) {
      if (!rm_confirm)
        rm_review = false;
      else {
        mesh_remote_change_t kind = rm_action == 0   ? MESH_REMOTE_OWNER
                                    : rm_action == 1 ? MESH_REMOTE_HOPS
                                    : rm_action == 2 ? MESH_REMOTE_ROLE
                                                     : MESH_REMOTE_REBOOT;

        if (mesh_radio_remote_change(kind, rm_name, rm_short,
                                     kind == MESH_REMOTE_HOPS ? rm_hops
                                                              : rm_role,
                                     rm_baseline.revision)) {
          rm_review = false;
          rm_loaded = false;
          rm_feedback[0] = 0;
        } else
          snprintf(rm_feedback, sizeof(rm_feedback),
                   "Not sent. Refresh; check verified key, firmware 2.7.x and "
                   "role/rebroadcast compatibility.");
      }
    }
    return true;
  }
  if (key == BSP_INPUT_NAVIGATION_KEY_UP) {
    if (rm_field)
      rm_field--;
    rm_replace = true;
  } else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) {
    if (rm_field < 8)
      rm_field++;
    rm_replace = true;
  } else if ((left || right) && rm_loaded && rm_field == 3) {
    if (right && rm_hops < 7)
      rm_hops++;
    if (left && rm_hops)
      rm_hops--;
  } else if ((left || right) && rm_loaded && rm_field == 5) {
    size_t i = 0;
    for (size_t n = 0; n < mesh_role_count(); n++)
      if (mesh_role_at(n) == rm_role)
        i = n;

    rm_role = mesh_role_at((i + mesh_role_count() + (right ? 1 : -1)) %
                           mesh_role_count());
  } else if (enter ||
             (right && (rm_field == 0 || rm_field == 4 || rm_field == 6 ||
                        rm_field == 7 || rm_field == 8))) {
    if (rm_field == 0) {
      rm_loaded = false;
      rm_feedback[0] = 0;
      if (!mesh_radio_remote_read(rm_target))
        snprintf(rm_feedback, sizeof(rm_feedback),
                 "Could not refresh. Verify the current key, radio connection "
                 "and remote admin authorization.");
    } else if (rm_field == 1 || rm_field == 2 || rm_field == 3 ||
               rm_field == 5) {
      if (rm_field < 8)
        rm_field++;
      rm_replace = true;
    } else if (rm_loaded) {
      rm_action = rm_field == 4 ? 0 : rm_field == 6 ? 1 : rm_field == 7 ? 2 : 3;
      rm_review = true;
      rm_confirm = false;
      rm_feedback[0] = 0;
    }
  }
  return true;
}
static bool management_navigation(int key) {
  return security_navigation(key) || remote_navigation(key);
}
static bool management_key(char key) {
  if (current_screen != SCREEN_SECURITY && current_screen != SCREEN_REMOTE)
    return false;

  if (key == '\r' || key == '\n')
    return management_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);

  if (key == '\t')
    return management_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);

  if (current_screen == SCREEN_SECURITY) {
    if (sc_mode == SC_ADMINS && (key == 'd' || key == 'D')) {
      mesh_security_t s = mesh_radio_security();
      if (s.state == MESH_MANAGE_READY && sc_slot < s.admin_count) {
        sc_revision = s.revision;
        sc_review(SC_ADMIN_REMOVE);
      }
      return true;
    }
    char *text = sc_mode == SC_LINK        ? sc_link
                 : sc_mode == SC_ADMIN_KEY ? sc_key_text
                                           : NULL;
    size_t cap = sc_mode == SC_LINK ? sizeof(sc_link) : sizeof(sc_key_text);

    if (text) {
      size_t n = strlen(text);
      if (key == '\b' || key == 127) {
        if (sc_mode == SC_ADMIN_KEY && sc_replace) {
          text[0] = 0;
          sc_replace = false;
        } else if (n)
          text[n - 1] = 0;
      } else if (key >= 33 && key <= 126 && n + 1 < cap) {
        if (sc_mode == SC_ADMIN_KEY && sc_replace) {
          text[0] = 0;
          n = 0;
          sc_replace = false;
        }
        text[n] = key;
        text[n + 1] = 0;
      }
      sc_feedback[0] = 0;
    }
  } else if (!rm_review && rm_loaded && (rm_field == 1 || rm_field == 2)) {
    char *text = rm_field == 1 ? rm_name : rm_short;
    size_t cap = rm_field == 1 ? sizeof(rm_name) : sizeof(rm_short),
           n = strlen(text);

    if (key == '\b' || key == 127) {
      if (rm_replace) {
        text[0] = 0;
        rm_replace = false;
      } else
        mesh_compose_backspace(text);
    } else if ((unsigned char)key >= 32 && key != 127) {
      if (rm_replace) {
        text[0] = 0;
        n = 0;
        rm_replace = false;
      }
      if (n + 1 < cap) {
        text[n] = key;
        text[n + 1] = 0;
      }
    }
  }
  return true;
}
