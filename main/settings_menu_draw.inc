// Render the same descriptors used for navigation.
static void draw_settings_menu(void) {
    mesh_settings_group_t group=settings_menu_group();
    size_t count=mesh_settings_count(group),selected=*settings_menu_cursor(group);
    size_t first=selected>6?selected-6:0;
    for (size_t i = first; i < count && i < first+7; i++) {
        const mesh_setting_entry_t *entry=mesh_settings_entry(group,i);
        int y = 86 + (i-first) * 44;
        pax_draw_rect(&fb, selected == i ? UI_SELECTED : UI_PANEL, 20, y, logical_h_res - 40, 38);
        pax_draw_rect(&fb, selected == i ? UI_GREEN : UI_BORDER, 20, y, 3, 38);
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 13, 34, y + 6, entry->name);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 9, 34, y + 23, entry->detail);
    }
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 20, logical_v_res - UI_HINT_OFFSET,
        "Up/Down: select  Right/Enter: open  Esc: back");
}

