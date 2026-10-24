// App-task screen drawing, extracted without changing its behavior.
static void draw_node_value(const char *label, const char *value, int x, int y, int width) {
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 8, x, y, label);
    char text[80]; snprintf(text, sizeof(text), "%s", value);
    // Bound values by the actual font width; do not split a UTF-8 codepoint.
    size_t n = strlen(text);
    while (n && pax_text_size(pax_font_sky_mono, 11, text).x > width) {
        do { n--; } while (n && ((unsigned char)text[n] & 0xc0) == 0x80);
        text[n] = '\0';
    }
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 11, x, y + 11, text);
}

static void draw_node_information(void) {
    mesh_node_t node = action_node;
    mesh_radio_get_node_by_number(action_node.number, &node);
    int mid = (int)logical_h_res / 2 + 8;
    int column_width = (int)logical_h_res / 2 - 52;
    pax_draw_rect(&fb, UI_PANEL, 20, 84, logical_h_res - 40, 188);
    pax_draw_circle(&fb, UI_SELECTED, 50, 106, 18);
    const char *short_name = node.short_name[0] ? node.short_name : "?";
    float short_width = pax_text_size(pax_font_sky_mono, 11, short_name).x;
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 11, 50 - short_width / 2, 101, short_name);
    char title[40]; snprintf(title, sizeof(title), "%s", node.name[0] ? node.name : "Unknown node");
    size_t n = strlen(title);
    while (n && pax_text_size(pax_font_sky_mono, 14, title).x > (int)logical_h_res - 276) {
        do { n--; } while (n && ((unsigned char)title[n] & 0xc0) == 0x80);
        title[n] = '\0';
    }
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 14, 80, 99, title);
    char battery[40],voltage[40],metric_age[96],power_line[192];
    mesh_metrics_battery(&node.metrics,battery,sizeof(battery));mesh_metrics_voltage(&node.metrics,voltage,sizeof(voltage));
    mesh_metrics_age(&node.metrics,time(NULL),esp_timer_get_time(),metric_age,sizeof(metric_age));
    snprintf(power_line,sizeof(power_line),"Power: %s / %s | %s",battery,voltage,metric_age);
    if(strcmp(node.name,node.advertised_name)&&node.advertised_name[0])snprintf(power_line,sizeof(power_line),"Radio name: %.39s",node.advertised_name);
    draw_rich_text(power_line,80,119,8,logical_h_res-260,10,1,UI_MUTED);
    int signal_x = (int)logical_h_res - 162;
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 8, signal_x, 89, "LAST RX / MAY BE RELAYED");
    char signal[32];
    if (node.snr_available) snprintf(signal, sizeof(signal), "SNR %.1f dB", node.snr);
    else snprintf(signal, sizeof(signal), "SNR unavailable");
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 10, signal_x, 101, signal);
    if (node.rssi_available) snprintf(signal, sizeof(signal), "RSSI %ld dBm", (long)node.rssi);
    else snprintf(signal, sizeof(signal), "RSSI unavailable");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, signal_x, 114, signal);
    char number[16]; snprintf(number, sizeof(number), "%lu", (unsigned long)node.number);
    draw_node_value("NODE NUMBER", number, 32, 134, column_width);
    draw_node_value("USER ID", node.user_id[0] ? node.user_id : "Unavailable", mid, 134, column_width);
    draw_node_value("ROLE", mesh_node_role_name(&node), 32, 166, column_width);
    char seen[32]; format_node_last_seen(node.first_seen, time(NULL), seen, sizeof(seen));
    draw_node_value("FIRST SEEN HERE", seen, mid, 166, column_width);
    format_node_last_seen(node.last_heard, time(NULL), seen, sizeof(seen));
    draw_node_value("LAST HEARD", seen, 32, 198, column_width);
    char hops[40];
    if (node.hops_away_available) snprintf(hops, sizeof(hops), "%lu%s", (unsigned long)node.hops_away, node.via_mqtt ? " / MQTT" : "");
    else snprintf(hops, sizeof(hops), "%s", node.via_mqtt ? "Unavailable / MQTT" : "Unavailable");
    draw_node_value("HOPS", hops, mid, 198, column_width);
    mesh_node_t own; double meters, bearing; char distance[100]; time_t now = time(NULL);
    if (node.number == mesh_radio_local_node()) snprintf(distance,sizeof(distance),"LOCATION: Your node");
    else if (mesh_radio_get_node_by_number(mesh_radio_local_node(),&own) && mesh_location_distance(&own,&node,&meters,&bearing)) {
        char formatted[32];mesh_units_distance(distance_units,meters,formatted,sizeof(formatted));
        snprintf(distance,sizeof(distance),"Approx. %s %s / %.0f deg true%s",formatted,mesh_location_compass(bearing),bearing,
            !own.position_time || !node.position_time || now < own.position_time || now < node.position_time ||
            now-own.position_time>3600 || now-node.position_time>3600 ? " / old or unknown fix age" : "");
    } else snprintf(distance,sizeof(distance),"Distance unavailable: own or node location missing");
    pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,32,231,distance);
    char public_key[45]; mesh_node_public_key_text(&node, public_key);
    mesh_trust_t trust=node.public_key_available?mesh_radio_contact_trust(node.number,node.public_key,NULL):MESH_TRUST_UNKNOWN;
    pax_draw_text(&fb,trust==MESH_TRUST_CHANGED?KEY_TRIANGLE_COLOR:UI_MUTED,pax_font_sky_mono,8,32,258,trust==MESH_TRUST_CHANGED?"CHANGED KEY":trust==MESH_TRUST_VERIFIED?"VERIFIED KEY":"PUBLIC KEY");
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 9, 104, 257, public_key);
}

#include "telemetry_draw.inc"

static void draw_node_actions(void) {
    if(node_modal==NODE_TELEMETRY){draw_telemetry_page(action_node.number,false);return;}
    if (node_modal == NODE_CONTROL_CONFIRM) {
        char name[40]; peer_name(action_node.number, name, sizeof(name));
        draw_rich_text(name, 20, 90, 16, logical_h_res - 40, 24, 2, UI_GREEN);
        const char *explanation = node_control_kind == MESH_NODE_REMOVE ?
            "Remove this node from the radio database? Saved chats and drafts stay. The node can reappear when heard again." :
            node_control_kind == MESH_NODE_IGNORE ?
            "Ignore this node on the radio? Its packets will be ignored and its cached location, telemetry and public key cleared. Find it under Nodes > Ignored to undo." :
            "Stop ignoring this node? New packets can be received again. Its location and public key must be learned again.";
        draw_rich_text(explanation, 20, 158, 14, logical_h_res - 40, 24, 7, WHITE);
        for (unsigned i = 0; i < 2; i++) {
            int width = ((int)logical_h_res - 52)/2, x = 20+i*(width+12);
            bool selected = node_control_confirm == (i != 0);
            pax_draw_rect(&fb, selected ? UI_GREEN : UI_PANEL, x, 350, width, 36);
            pax_draw_text(&fb, selected ? BLACK : UI_GREEN, pax_font_sky_mono, 13, x+12, 361, i ? "CONFIRM" : "CANCEL");
        }
        if (node_action_feedback[0]) draw_rich_text(node_action_feedback, 20, 398, 10, logical_h_res-40, 18, 1, KEY_TRIANGLE_COLOR);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res-UI_HINT_OFFSET, "Left/Right: choose  Enter: confirm  Esc: Actions");
        return;
    }
    if(node_modal==NODE_NICKNAME){
        draw_rich_text("Local nickname for this node. Type to replace; Backspace clears the selected text. Leave empty to use its radio name. Saved only in MeshMatsu.",20,100,14,logical_h_res-40,26,4,UI_MUTED);
        pax_draw_rect(&fb,UI_SELECTED,20,230,logical_h_res-40,48);
        draw_rich_text(nickname_draft[0]?nickname_draft:"(use radio name)",32,244,16,logical_h_res-64,24,1,UI_GREEN);
        draw_rich_text(node_action_feedback,20,310,12,logical_h_res-40,22,3,KEY_TRIANGLE_COLOR);
        pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,11,20,logical_v_res-UI_HINT_OFFSET,"Enter: save  Esc: cancel  39 UTF-8 bytes maximum");return;
    }
    if (node_modal == NODE_ACTIONS) draw_node_information();
    else {
    pax_draw_rect(&fb, UI_PANEL, 20, 84, logical_h_res - 40, 44);
    char name[40], id[16];
    peer_name(action_node.number, name, sizeof(name));
    char title[32]; snprintf(title, sizeof(title), "%.22s", name);
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 14, 32, 98, title);
    snprintf(id, sizeof(id), "!%08lx", (unsigned long)action_node.number);
    float id_width = pax_text_size(pax_font_sky_mono, 10, id).x;
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, logical_h_res - 32 - id_width, 101, id);
    }
    mesh_trace_result_t trace;
    bool have_result = mesh_radio_get_traceroute(action_node.number, &trace);
    if (node_modal == NODE_ACTIONS) {
        mesh_node_t live_node = {0};
        bool known = mesh_radio_get_node_by_number(action_node.number, &live_node) && live_node.favorite_known;
        bool favorite = known && live_node.is_favorite;
        mesh_favorite_status_t saving = mesh_radio_favorite_status();
        bool pending = saving.state == MESH_FAVORITE_SAVING && saving.node == action_node.number;
        const char *names[] = {"DIRECT MESSAGE", "START TRACEROUTE", "VIEW TRACEROUTE RESULTS", pending ? "SAVING FAVORITE..." :
            !known ? "FAVORITE UNAVAILABLE" : favorite ? "REMOVE FAVORITE (RADIO)" : "ADD FAVORITE (RADIO)", "VIEW TELEMETRY", mesh_radio_node_muted(action_node.number) ? "UNMUTE NOTIFICATIONS" : "MUTE NOTIFICATIONS",
            live_node.is_ignored ? "UNIGNORE NODE (RADIO)" : "IGNORE NODE (RADIO)", "REMOVE NODE FROM RADIO", "REQUEST NODE INFO / NAME", "SET LOCAL NICKNAME", "SECURITY / SHARE CONTACT", "REMOTE MANAGEMENT", "RANGE TEST THIS NODE", "VIEW NEIGHBORS"};
        size_t first = node_action_selection > 3 ? node_action_selection - 3 : 0;
        for (size_t i = first; i < 14 && i < first + 4; i++) {
            int y = 280 + (i-first) * 27;
            pax_draw_rect(&fb, node_action_selection == i ? UI_SELECTED : UI_PANEL, 20, y, logical_h_res - 40, 21);
            pax_draw_rect(&fb, node_action_selection == i ? UI_GREEN : UI_BORDER, 20, y, 3, 21);
            pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 11, 34, y + 5, names[i]);
        }
        char range[32]; snprintf(range, sizeof(range), "%u / 14 actions", (unsigned)(node_action_selection+1));
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 9, logical_h_res-120, 389, range);
        const char *feedback = node_action_feedback;
        mesh_node_control_status_t control = mesh_radio_node_control_status();
        if (!feedback[0] && control.node == action_node.number && control.state != MESH_FAVORITE_IDLE) {
            feedback = control.state == MESH_FAVORITE_SAVING ? "Checking the radio database..." :
                control.state == MESH_FAVORITE_SAVED ? (control.kind == MESH_NODE_REMOVE ? "Node removed. Saved chats kept." : control.kind == MESH_NODE_IGNORE ? "Node ignored. Use Ignored filter to undo." : "Node unignored.") :
                control.state == MESH_FAVORITE_TIMEOUT ? "Change not confirmed: timed out. Check radio." :
                control.state == MESH_FAVORITE_INTERRUPTED ? "Change not confirmed: radio reconnected." : "Radio did not confirm the change. Check and retry.";
        }
        if (!feedback[0] && saving.node == action_node.number) {
            feedback = saving.state == MESH_FAVORITE_SAVING ? "Saving; checking the radio's node database..." :
                saving.state == MESH_FAVORITE_SAVED ? (saving.desired ? "Favorite saved on radio." : "Favorite removed from radio.") :
                saving.state == MESH_FAVORITE_FAILED ? "Radio did not confirm the change. Retry." :
                saving.state == MESH_FAVORITE_TIMEOUT ? "Save not confirmed: request timed out." :
                saving.state == MESH_FAVORITE_INTERRUPTED ? "Save not confirmed: radio reconnected." : "";
        }
        if (feedback[0]) pax_draw_text(&fb, KEY_TRIANGLE_COLOR, pax_font_sky_mono, 10, 20, 401, feedback);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET,
            map_action_origin ? "Up/Down: choose  Enter: open  Esc: Map" : "Up/Down: choose  Enter: open  Esc: Nodes");
        return;
    }
    int tab_width = ((int)logical_h_res - 48) / 2;
    for (unsigned i = 0; i < 2; i++) {
        bool selected = trace_return_selected == (i != 0);
        int x = 20 + i * (tab_width + 8);
        pax_draw_rect(&fb, selected ? UI_SELECTED : UI_PANEL, x, 140, tab_width, 28);
        pax_draw_text(&fb, selected ? UI_GREEN : UI_MUTED, pax_font_sky_mono, 12, x + 10, 148, i ? "RETURN PATH" : "OUTBOUND PATH");
    }
    if (!have_result || trace.state != MESH_TRACE_COMPLETE) {
        const char *status = !have_result ? "No traceroute for this node yet." :
            trace.state == MESH_TRACE_WAITING ? "Waiting for traceroute reply..." :
            trace.state == MESH_TRACE_TIMEOUT ? "No reply within 120 seconds." :
            trace.state == MESH_TRACE_INTERRUPTED ? "Interrupted: radio reconnected." : trace_error_text(trace.error_reason);
        pax_draw_text(&fb, have_result && trace.state == MESH_TRACE_WAITING ? UI_GREEN : KEY_TRIANGLE_COLOR,
            pax_font_sky_mono, 13, 20, 194, status);
        if (have_result && trace.state == MESH_TRACE_WAITING) {
            char elapsed[48]; snprintf(elapsed, sizeof(elapsed), "%u / 120 seconds", mesh_radio_traceroute_elapsed(action_node.number));
            pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, 226, elapsed);
            pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, 258, "You can leave this page while it runs.");
        } else {
            pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, 232, "Esc: Actions, then Start traceroute.");
            if (have_result && trace.state == MESH_TRACE_ERROR) {
                char error[40]; snprintf(error, sizeof(error), "Routing error code: %lu", (unsigned long)trace.error_reason);
                pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, 262, error);
            }
        }
    } else {
        uint8_t count = trace_return_selected ? trace.return_count : trace.route_count;
        uint8_t snr_count = trace_return_selected ? trace.return_snr_count : trace.snr_count;
        const uint32_t *route = trace_return_selected ? trace.return_route : trace.route;
        const int32_t *snr = trace_return_selected ? trace.return_snr : trace.snr;
        if (trace_return_selected && !count && !snr_count) {
            pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 13, 20, 194, "Return path was not reported.");
        } else {
            char summary[56]; snprintf(summary, sizeof(summary), "Complete: %u intermediate nodes reported", count);
            pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 11, 20, 180, summary);
            size_t total = count + 2;
            size_t visible = ((int)logical_v_res - UI_HINT_OFFSET - 214) / 32;
            if (!visible) visible = 1;
            if (trace_scroll + visible > total) trace_scroll = total > visible ? total - visible : 0;
            for (size_t row = trace_scroll; row < total && row < trace_scroll + visible; row++) {
                uint32_t number = row == 0 ? (trace_return_selected ? trace.node : trace.origin) :
                    row == total - 1 ? (trace_return_selected ? trace.origin : trace.node) : route[row - 1];
                char hop_name[40], label[48], address[16];
                if (!number || number == MESH_BROADCAST_NODE) snprintf(hop_name, sizeof(hop_name), "Unknown hop");
                else if (number == trace.origin) snprintf(hop_name, sizeof(hop_name), "You");
                else peer_name(number, hop_name, sizeof(hop_name));
                snprintf(label, sizeof(label), "%u. %.21s", (unsigned)(row + 1), hop_name);
                snprintf(address, sizeof(address), "!%08lx", (unsigned long)number);
                int y = 204 + (row - trace_scroll) * 32;
                pax_draw_rect(&fb, UI_PANEL, 20, y, logical_h_res - 40, 30);
                pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 12, 32, y + 2, label);
                pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 9, 32, y + 19, address);
                if (row) {
                    char signal[40] = "SNR --";
                    if (snr_count == count + 1 && snr[row - 1] != -128) snprintf(signal, sizeof(signal), "SNR %.2fdB", snr[row - 1] / 4.0);
                    float width = pax_text_size(pax_font_sky_mono, 10, signal).x;
                    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, logical_h_res - 32 - width, y + 10, signal);
                }
            }
        }
        char seen[24], received[64];
        format_node_last_seen(trace.completed_at, time(NULL), seen, sizeof(seen));
        snprintf(received, sizeof(received), "Result received: %s", seen);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, 402, received);
    }
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET,
        "Up/Down: scroll  L/R: direction  Esc: Actions");
}

