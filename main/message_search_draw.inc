static void draw_message_search(void) {
    char label[96];
    if (search_all) snprintf(label, sizeof(label), "ALL SAVED MESSAGES / %u matches", (unsigned)search_count);
    else if (search_peer) { char name[40]; peer_name(search_peer, name, sizeof(name));snprintf(label,sizeof(label),"THIS CHAT: %.30s / %u matches",name,(unsigned)search_count); }
    else snprintf(label, sizeof(label), "CHANNEL SLOT %u / %u matches", search_channel, (unsigned)search_count);
    draw_rich_text(label, 20, 64, 12, logical_h_res - 40, 16, 1, UI_MUTED);
    pax_draw_rect(&fb, UI_SELECTED, 20, 86, logical_h_res - 40, 42);
    pax_draw_rect(&fb, UI_GREEN, 20, 86, 3, 42);
    draw_rich_text(search_query[0] ? search_query : "Type message text, sender name or node ID...", 32, 98, 13, logical_h_res - 64, 18, 1, search_query[0] ? WHITE : UI_MUTED);
    if (!search_count) pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 13, 32, 164, "No matching retained messages.");
    size_t first = search_selection / 4 * 4;
    for (size_t i = first; i < search_count && i < first + 4; i++) {
        int y = 143 + (i - first) * 64;
        bool selected = i == search_selection;
        const mesh_message_t *message = &search_results[i];
        pax_draw_rect(&fb, selected ? UI_SELECTED : UI_PANEL, 20, y, logical_h_res - 40, 58);
        pax_draw_rect(&fb, selected ? UI_GREEN : UI_BORDER, 20, y, 3, 58);
        draw_message_header(message, y + 5, i + 1);
        draw_rich_text(message->text, 32, y + 27, 12, logical_h_res - 64, 18, 1, WHITE);
    }
    draw_rich_text(search_feedback[0] ? search_feedback : "Results are a snapshot. Tab switches scope and refreshes.", 20, 403, 10, logical_h_res - 40, 14, 1, UI_MUTED);
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET,
        "Type: search  Up/Down: select  Enter: view  Tab: scope  Esc: back");
}
