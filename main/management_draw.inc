static void management_buttons(bool confirm) {
  for (unsigned i = 0; i < 2; i++) {
    int w = ((int)logical_h_res - 52) / 2, x = 20 + i * (w + 12);
    bool selected = confirm == (i != 0);

    pax_draw_rect(&fb, selected ? UI_GREEN : UI_PANEL, x, 350, w, 36);
    pax_draw_text(&fb, selected ? BLACK : UI_GREEN, pax_font_sky_mono, 13,
                  x + 12, 361, i ? "CONFIRM" : "CANCEL");
  }
}
static void management_fingerprint(const uint8_t *key, char out[32]) {
  char raw[17];
  if (!mesh_keys_fingerprint(key, raw)) {
    snprintf(out, 32, "Unavailable");
    return;
  }
  snprintf(out, 32, "%.4s %.4s %.4s %.4s", raw, raw + 4, raw + 8, raw + 12);
}
static void draw_security_centre(void) {
  char heading[100], fp[32];
  snprintf(heading, sizeof(heading), "Contact !%08lx%s",
           (unsigned long)sc_target,
           sc_target == mesh_radio_local_node() ? " / YOUR RADIO" : "");

  pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 12, 20, 68, heading);

  if (sc_mode == SC_MENU) {
    const char *labels[] = {"SHARE CONTACT QR", "COMPARE / VERIFY PUBLIC KEY",
                            "IMPORT CONTACT LINK",
                            "AUTHORIZED ADMIN KEYS - LOCAL RADIO"};

    for (unsigned i = 0; i < 4; i++) {
      int y = 100 + i * 55;
      pax_draw_rect(&fb, i == sc_selection ? UI_SELECTED : UI_PANEL, 20, y,
                    logical_h_res - 40, 45);
      pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 13, 34, y + 15,
                    labels[i]);
    }
    if (sc_have_contact)
      draw_rich_text(trust_label(mesh_radio_contact_trust(
                         sc_target, sc_contact.key, NULL)),
                     20, 334, 12, logical_h_res - 40, 22, 2, UI_MUTED);

  } else if (sc_mode == SC_QR) {
    if (sc_qr_valid) {
      int size = qrcodegen_getSize(sc_qr), scale = 280 / (size + 8),
          side = (size + 8) * scale, x = 20, y = 94;

      pax_draw_rect(&fb, WHITE, x, y, side, side);
      for (int row = 0; row < size; row++)
        for (int col = 0; col < size; col++)
          if (qrcodegen_getModule(sc_qr, col, row))
            pax_draw_rect(&fb, BLACK, x + (col + 4) * scale,
                          y + (row + 4) * scale, scale, scale);

      if (logical_h_res >= 700) {
        draw_rich_text("Scan in Meshtastic to add this contact. Includes the "
                       "advertised name, node ID and public key.",
                       320, 100, 14, logical_h_res - 340, 24, 5, UI_GREEN);

        draw_rich_text("Compare the key separately before marking it verified. "
                       "Sharing does not grant administrator access.",
                       320, 236, 12, logical_h_res - 340, 22, 4, UI_MUTED);

        draw_rich_text(sc_link, 320, 332, 8, logical_h_res - 340, 13, 5, WHITE);
      } else
        draw_rich_text("Public contact only. Compare keys before trusting.", 20,
                       379, 11, logical_h_res - 40, 17, 2, UI_MUTED);

    } else
      draw_rich_text("Contact QR unavailable. Refresh Node Info and reopen.",
                     20, 110, 16, logical_h_res - 40, 26, 4,
                     KEY_TRIANGLE_COLOR);

  } else if (sc_mode == SC_COMPARE) {
    uint8_t previous[32] = {0};
    mesh_trust_t trust =
        mesh_radio_contact_trust(sc_target, sc_contact.key, previous);

    draw_rich_text(trust_label(trust), 20, 100, 16, logical_h_res - 40, 26, 2,
                   trust == MESH_TRUST_CHANGED ? KEY_TRIANGLE_COLOR : UI_GREEN);

    management_fingerprint(sc_contact.key, fp);
    snprintf(heading, sizeof(heading), "Current fingerprint: %s", fp);
    draw_rich_text(heading, 20, 172, 14, logical_h_res - 40, 24, 2, WHITE);

    if (trust != MESH_TRUST_UNKNOWN) {
      management_fingerprint(previous, fp);
      snprintf(heading, sizeof(heading), "Saved fingerprint:   %s", fp);
      draw_rich_text(heading, 20, 214, 14, logical_h_res - 40, 24, 2, UI_MUTED);
    }
    mesh_node_t n = {.public_key_available = true};
    memcpy(n.public_key, sc_contact.key, 32);
    char key[45];
    mesh_node_public_key_text(&n, key);

    draw_rich_text(key, 20, 268, 11, logical_h_res - 40, 20, 2, WHITE);

    draw_rich_text("Compare with the owner's device or another trusted source. "
                   "Enter reviews manual verification; a matching saved key "
                   "alone does not prove who owns it.",
                   20, 324, 12, logical_h_res - 40, 21, 3, UI_MUTED);

  } else if (sc_mode == SC_LINK || sc_mode == SC_ADMIN_KEY) {
    draw_rich_text(sc_mode == SC_LINK
                       ? "Enter a Meshtastic contact link. Enter opens a "
                         "review before adding it to your local radio."
                       : "Enter the administrator's PUBLIC key. This "
                         "authorizes that identity to administer your local "
                         "radio. Never enter a private key.",
                   20, 104, 14, logical_h_res - 40, 25, 4, UI_GREEN);

    const char *text = sc_mode == SC_LINK ? sc_link : sc_key_text;

    pax_draw_rect(&fb, UI_SELECTED, 20, 226, logical_h_res - 40, 130);
    draw_rich_text(text[0] ? text
                   : sc_mode == SC_LINK
                       ? "https://meshtastic.org/v/#..."
                       : "32-byte Base64 or hexadecimal public key",
                   32, 240, 12, logical_h_res - 64, 21, 5, WHITE);

  } else if (sc_mode == SC_ADMINS) {
    mesh_security_t state = mesh_radio_security();
    draw_rich_text(manage_status_text(state.state), 20, 100, 12,
                   logical_h_res - 40, 22, 2, UI_MUTED);

    for (unsigned i = 0; i < 4; i++) {
      int y = 162 + i * 45;
      char text[100];
      if (i < 3 && i < state.admin_count) {
        management_fingerprint(state.admins[i], fp);
        snprintf(text, sizeof(text), "Admin %u: %s", i + 1, fp);
      } else
        snprintf(text, sizeof(text),
                 i == 3 ? "REFRESH FROM LOCAL RADIO" : "Admin %u: empty",
                 i + 1);

      pax_draw_rect(&fb, sc_slot == i ? UI_SELECTED : UI_PANEL, 20, y,
                    logical_h_res - 40, 38);
      pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 12, 34, y + 12, text);
    }
    draw_rich_text("Up to three admin keys. Enter: add/replace. D: review "
                   "removal. Managed mode and serial access stay unchanged.",
                   20, 354, 11, logical_h_res - 40, 20, 3, UI_MUTED);

  } else if (sc_mode == SC_REVIEW) {
    const char *warning =
        sc_action == SC_VERIFY
            ? "Have you compared this public key with its owner through a "
              "trusted source? Confirm stores it as manually verified on this "
              "Tanmatsu."
        : sc_action == SC_IMPORT
            ? "Add this public contact to your local radio? Existing names/key "
              "can be replaced. Imported verification claims are ignored; "
              "compare the key yourself. The radio may favorite imported "
              "contacts."
        : sc_action == SC_ADMIN_SET
            ? "Grant this public key administrator access to YOUR LOCAL RADIO? "
              "Its owner can remotely read and change your radio settings. "
              "Other security fields are preserved."
            : "Remove this administrator key from YOUR LOCAL RADIO? That "
              "identity will lose remote administrator access. Local serial "
              "access is preserved.";

    draw_rich_text(warning, 20, 104, 14, logical_h_res - 40, 25, 6, WHITE);

    if (sc_action == SC_IMPORT || sc_action == SC_VERIFY) {
      management_fingerprint(sc_contact.key, fp);
      snprintf(heading, sizeof(heading), "!%08lx  %.39s",
               (unsigned long)sc_contact.number, sc_contact.name);
      draw_rich_text(heading, 20, 266, 13, logical_h_res - 40, 22, 2, UI_GREEN);
    } else if (sc_action == SC_ADMIN_SET)
      management_fingerprint(sc_admin_key, fp);

    else {
      mesh_security_t state = mesh_radio_security();
      management_fingerprint(state.admins[sc_slot], fp);
    }
    draw_rich_text(fp, 20, 312, 13, logical_h_res - 40, 22, 1, UI_GREEN);
    management_buttons(sc_confirm);

  } else if (sc_mode == SC_WAIT) {
    mesh_security_t state = mesh_radio_security();
    draw_rich_text(
        manage_status_text(state.state), 20, 112, 17, logical_h_res - 40, 28, 5,
        state.state == MESH_MANAGE_SAVED ? UI_GREEN : KEY_TRIANGLE_COLOR);

    if (state.error) {
      mesh_message_t failure = {.failure = MESH_FAILURE_RADIO,
                                .error_reason = state.error};
      draw_rich_text(mesh_radio_delivery_text(&failure), 20, 290, 13,
                     logical_h_res - 40, 22, 4, KEY_TRIANGLE_COLOR);
    }
  }
  if (!mesh_radio_trust_storage_ok() && sc_mode != SC_QR)
    draw_rich_text("Contact trust storage unavailable; manual verification and "
                   "remote management are disabled.",
                   20, 375, 10, logical_h_res - 40, 17, 2, KEY_TRIANGLE_COLOR);

  if (sc_feedback[0])
    draw_rich_text(sc_feedback, 20, 391, 10, logical_h_res - 40, 16, 2,
                   KEY_TRIANGLE_COLOR);

  pax_draw_text(
      &fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET,
      sc_mode == SC_REVIEW ? "Left/Right: choose  Enter: confirm  Esc: cancel"
                           : "Up/Down: choose  Enter: open/review  Esc: back");
}
static void draw_remote_manager(void) {
  remote_refresh_ui();
  mesh_remote_t state = mesh_radio_remote();
  char title[120];
  snprintf(title, sizeof(title), "REMOTE !%08lx  %.39s  %.32s",
           (unsigned long)rm_target, state.node == rm_target ? state.name : "",
           state.node == rm_target ? state.firmware : "");

  draw_rich_text(title, 20, 68, 12, logical_h_res - 40, 18, 1, UI_GREEN);

  if (rm_review) {
    const char *warning =
        rm_action == 0   ? "Change this REMOTE radio's advertised names? Other "
                           "User fields are preserved."
        : rm_action == 1 ? "Change this REMOTE radio's hop limit? This affects "
                           "how far its outgoing packets can travel."
        : rm_action == 2
            ? "Change this REMOTE radio's role? Forwarding, power use and "
              "reachability may change. A reboot may interrupt verification."
            : "Reboot this REMOTE radio in five seconds? It will temporarily "
              "stop communicating. Acknowledgement does not prove restart "
              "completed.";

    draw_rich_text(warning, 20, 104, 15, logical_h_res - 40, 27, 4, WHITE);

    if (rm_action == 0)
      snprintf(title, sizeof(title), "New names: %.39s / %.4s", rm_name,
               rm_short);

    else if (rm_action == 1)
      snprintf(title, sizeof(title), "New hop limit: %lu",
               (unsigned long)rm_hops);

    else if (rm_action == 2)
      snprintf(title, sizeof(title), "New role: %s", mesh_role_name(rm_role));
    else
      snprintf(title, sizeof(title), "Target: !%08lx",
               (unsigned long)rm_target);

    draw_rich_text(title, 20, 230, 14, logical_h_res - 40, 24, 2, UI_GREEN);

    if (rm_action == 2)
      draw_rich_text(mesh_role_description(rm_role), 20, 288, 12,
                     logical_h_res - 40, 20, 3, UI_MUTED);

    management_buttons(rm_confirm);

  } else {
    draw_rich_text(state.node == rm_target
                       ? manage_status_text(state.state)
                       : "No remote snapshot. Select Refresh.",
                   20, 95, 12, logical_h_res - 40, 19, 2, UI_MUTED);

    const char *labels[] = {"REFRESH REMOTE SETTINGS",
                            "LONG NAME",
                            "SHORT NAME",
                            "HOP LIMIT",
                            "REVIEW NAME CHANGE",
                            "DEVICE ROLE",
                            "REVIEW HOP CHANGE",
                            "REVIEW ROLE CHANGE",
                            "REVIEW REBOOT"};

    size_t first = rm_field > 5 ? rm_field - 5 : 0;

    for (size_t i = first; i < 9 && i < first + 6; i++) {
      int y = 142 + (i - first) * 34;
      char text[128];

      if (i == 1 || i == 2)
        snprintf(text, sizeof(text), "%s: %s", labels[i],
                 rm_loaded ? (i == 1 ? rm_name : rm_short) : "(refresh first)");

      else if (i == 3)
        snprintf(text, sizeof(text), "%s: %lu", labels[i],
                 (unsigned long)rm_hops);

      else if (i == 5)
        snprintf(text, sizeof(text), "%s: %s", labels[i],
                 mesh_role_name(rm_role));
      else
        snprintf(text, sizeof(text), "%s", labels[i]);

      pax_draw_rect(&fb, rm_field == i ? UI_SELECTED : UI_PANEL, 20, y,
                    logical_h_res - 40, 29);
      draw_rich_text(text, 32, y + 7, 12, logical_h_res - 64, 18, 1, UI_GREEN);
    }
    if (state.error && state.node == rm_target) {
      mesh_message_t failure = {.failure = MESH_FAILURE_RADIO,
                                .error_reason = state.error};
      draw_rich_text(mesh_radio_delivery_text(&failure), 20, 352, 11,
                     logical_h_res - 40, 18, 2, KEY_TRIANGLE_COLOR);
    } else
      draw_rich_text(rm_field == 5
                         ? mesh_role_description(rm_role)
                         : "Your public key must be authorized on the target. "
                           "Writes support firmware 2.7.x; each change is sent "
                           "once and checked by readback.",
                     20, 352, 11, logical_h_res - 40, 18, 2, UI_MUTED);
  }
  if (rm_feedback[0])
    draw_rich_text(rm_feedback, 20, 393, 10, logical_h_res - 40, 16, 2,
                   KEY_TRIANGLE_COLOR);

  pax_draw_text(
      &fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET,
      rm_review ? "Left/Right: choose  Enter: confirm  Esc: cancel"
                : "Up/Down: field  Type: names  L/R: value  Enter: review");
}
