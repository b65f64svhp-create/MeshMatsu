static void draw_node_browser(void) {
    time_t now = time(NULL);
    size_t count = filtered_node_count(now);
    size_t visible = ((int)logical_v_res - UI_HINT_OFFSET - 146) / 54;
    if (!visible) visible = 1;
    size_t first = node_selection >= visible ? node_selection - visible + 1 : 0;
    size_t end = first + visible < count ? first + visible : count;
    char label[128], sort[40], range[24];
    snprintf(label, sizeof(label), "%s  %u nodes", mesh_node_filter_name(node_browser.filter), (unsigned)count);
    if(node_browser.filter==MESH_NODES_NEARBY){char radius[32];mesh_units_distance(distance_units,mesh_node_browser_radius(&node_browser),radius,sizeof(radius));snprintf(label,sizeof(label),"NEARBY <= %s  %u nodes",radius,(unsigned)count);}
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 12, 20, 64, label);
    snprintf(sort, sizeof(sort), "Sort: %s", mesh_node_sort_name(node_browser.sort));
    float width = pax_text_size(pax_font_sky_mono, 11, sort).x;
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, logical_h_res - 20 - width, 64, sort);
    snprintf(label, sizeof(label), "Favorites first: %s (F) | %s", node_browser.favorites_first ? "On" : "Off", !node_browser.own_position_available?"Your location unavailable":"Approx. distances; * old/unknown fix");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, 84, label);
    snprintf(range, sizeof(range), "%u-%u/%u", (unsigned)(count ? first + 1 : 0), (unsigned)end, (unsigned)count);
    width = pax_text_size(pax_font_sky_mono, 10, range).x;
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, logical_h_res - 20 - width, 84, range);
    pax_draw_rect(&fb, node_search_editing ? UI_SELECTED : UI_PANEL, 20, 104, logical_h_res - 40, 32);
    if (node_search_editing) pax_outline_rect(&fb, UI_GREEN, 20, 104, logical_h_res - 40, 32);
    char search[64];
    if (node_search_editing || node_browser.query[0]) {
        const char *tail = node_browser.query;
        for (;;) {
            snprintf(search, sizeof(search), "Search: %s%s%s", tail != node_browser.query ? "..." : "", tail, node_search_editing ? "_" : "");
            if (!*tail || pax_text_size(pax_font_sky_mono, 12, search).x <= logical_h_res - 64) break;
            do { tail++; } while (((unsigned char)*tail & 0xc0) == 0x80);
        }
    } else snprintf(search, sizeof(search), "/: search name or node ID");
    pax_draw_text(&fb, node_search_editing ? UI_GREEN : UI_MUTED, pax_font_sky_mono, 12, 32, 114, search);
    if (!count) {
        pax_draw_text(&fb, WHITE, pax_font_sky_mono, 14, 20, 170, node_browser.filter==MESH_NODES_NEARBY&&!node_browser.own_position_available?"Nearby needs your radio's location.":"No matching nodes.");
        draw_rich_text(
            node_browser.filter==MESH_NODES_NEARBY&&!node_browser.own_position_available ? "Set a fixed position or wait for a GPS fix. Left/Right changes the filter." :
            node_browser.filter==MESH_NODES_NEARBY ? "No reported positions within this range. +/- changes radius; / edits search." :
            node_browser.query[0] ? "/: edit search; Backspace erases characters." :
            node_browser.filter == MESH_NODES_IGNORED ? "No ignored nodes. Left/Right changes the filter." : node_browser.filter == MESH_NODES_FAVORITES ? "Only radio-confirmed favorites appear here." : "Try All Seen or wait for radio updates.",20,198,12,logical_h_res-40,22,3,UI_MUTED);
    }
    for (size_t i = first; i < end; i++) {
        mesh_node_t node;
        if (!get_filtered_node(i, now, &node)) continue;
        int y = 152 + (int)(i - first) * 54;
        pax_draw_rect(&fb, i == node_selection ? UI_SELECTED : UI_PANEL, 20, y - 6, logical_h_res - 40, 48);
        pax_draw_rect(&fb, i == node_selection ? UI_GREEN : UI_BORDER, 20, y - 6, 3, 48);
        char hops[24];
        if (node.hops_away_available) snprintf(hops, sizeof(hops), "%lu hops", (unsigned long)node.hops_away);
        else snprintf(hops, sizeof(hops), "? hops");
        int hops_x = (int)logical_h_res - 32 - (int)ceilf(pax_text_size(pax_font_sky_mono, 12, hops).x);
        char name[48];
        snprintf(name, sizeof(name), "%s%s", node.favorite_known && node.is_favorite ? "* " : "", node.name[0] ? node.name : node.short_name);
        size_t length = strlen(name);
        while (length && pax_text_size(pax_font_sky_mono, 14, name).x > hops_x - 46) {
            do { length--; } while (length && ((unsigned char)name[length] & 0xc0) == 0x80);
            name[length] = '\0';
        }
        pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 14, 32, y, name);
        pax_draw_text(&fb, WHITE, pax_font_sky_mono, 12, hops_x, y + 1, hops);
        char seen[32], detail[48], snr[24];
        format_node_last_seen(node.last_heard, now, seen, sizeof(seen));
        snprintf(detail, sizeof(detail), "Seen %s", seen);
        pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, 32, y + 22, detail);
        if (node.snr_available) snprintf(snr, sizeof(snr), "SNR %.1fdB", node.snr);
        else snprintf(snr, sizeof(snr), "SNR unavailable");
        width = pax_text_size(pax_font_sky_mono, 10, snr).x;
        int snr_x=(int)logical_h_res-32-(int)ceilf(width);
        pax_draw_text(&fb, WHITE, pax_font_sky_mono, 10, snr_x, y + 23, snr);
        char distance[64],value[40];
        if(isfinite(node_browser.distances[i])){mesh_units_distance(distance_units,node_browser.distances[i],value,sizeof(value));snprintf(distance,sizeof(distance),"~%s%s",value,node_browser.position_old[i]?" *":"");}
        else snprintf(distance,sizeof(distance),"Distance unavailable");
        width=pax_text_size(pax_font_sky_mono,10,distance).x;
        pax_draw_text(&fb,UI_MUTED,pax_font_sky_mono,10,snr_x-24-width,y+23,distance);
    }
    pax_draw_text(&fb, WHITE, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET,
        node_search_editing ? "Type to filter  Enter:done  Esc:cancel" : node_browser.filter==MESH_NODES_NEARBY?"L/R:filter  Up/Down:node  +/-:radius  Tab:sort  /:search  Enter:actions":"L/R:filter  Up/Down:node  Tab:sort  /:search  Enter:actions");
}

