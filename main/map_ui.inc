// Offline map UI. Coordinates remain exactly as shared by each radio.
#include "map_filters.h"
static mesh_map_filters_t map_filters; // Session-only view choices; radio settings are unchanged.
static mesh_map_view_t map_view;
static bool map_opened;
enum {MAP_TRAILS_OFF,MAP_TRAILS_SELECTED,MAP_TRAILS_ALL};
static unsigned map_trail_mode=MAP_TRAILS_SELECTED;
static uint32_t map_selected_node;
static uint32_t map_waypoint_id, map_waypoint_peer;
static uint8_t map_waypoint_channel;
static bool map_focused_waypoint(mesh_waypoint_t *out) {
    if(map_filters.waypoints_hidden||!map_waypoint_id||!mesh_radio_find_waypoint(map_waypoint_id,map_waypoint_peer,map_waypoint_channel,out)||out->deleted)return false;
    uint32_t now=mesh_radio_time();
    return !out->expire||out->expire==INT32_MAX||now<1700000000||out->expire>now;
}
static bool map_located_node(uint32_t number, mesh_node_t *node) {
    return number && mesh_radio_get_node_by_number(number, node) && mesh_map_node_visible(&map_filters, node, mesh_radio_time());
}
static void map_center_selected(void) {
    mesh_node_t node;
    if (map_located_node(map_selected_node, &node)) {
        map_waypoint_id=0;
        mesh_map_center(&map_view, node.latitude_i / 1e7, node.longitude_i / 1e7, map_view.zoom);
    }
}
static void map_next_node(void) {
    size_t count = mesh_radio_node_count(), start = 0;
    mesh_node_t node;
    for (size_t i = 0; i < count; i++) if (mesh_radio_get_node(i, &node) && node.number == map_selected_node) { start = i + 1; break; }
    for (size_t i = 0; i < count; i++) if (mesh_radio_get_node((start + i) % count, &node) && mesh_map_node_visible(&map_filters, &node, mesh_radio_time())) {
        map_selected_node = node.number; map_center_selected(); return;
    }
    map_selected_node = 0;
}
static void open_map(void) {
    uint32_t target = 0;
    if (current_screen == SCREEN_NODES) {
        mesh_node_t node;
        if (node_modal) target = action_node.number;
        else if (get_filtered_node(node_selection, time(NULL), &node)) target = node.number;
    }
    if (!map_opened) {
        mesh_map_center(&map_view, 54.95, -118.55, 8); // Requested regional overview; not a device fix.
        map_selected_node = mesh_radio_local_node();
        mesh_node_t local;
        if (!map_located_node(map_selected_node, &local)) { map_selected_node = 0; map_next_node(); }
        else map_center_selected();
        map_opened = true;
    }
    mesh_node_t node;
    if (map_located_node(target, &node)) { map_selected_node = target; map_center_selected(); }
    chat_actions.modal = 0; node_modal = 0; node_search_editing = false;
    current_screen = SCREEN_MAP;
}
static void map_open_actions(void) {
    mesh_node_t node;
    if (!map_located_node(map_selected_node, &node) || node.number == mesh_radio_local_node()) {
        map_center_selected(); return;
    }
    action_node = node; node_modal = NODE_ACTIONS; node_action_selection = 0;
    node_action_feedback[0] = '\0'; map_action_origin = true;
    current_screen = SCREEN_NODES;
}
static bool map_navigation(int key) {
    if (key == BSP_INPUT_NAVIGATION_KEY_F5) { open_map(); return true; }
    if (current_screen != SCREEN_MAP) return false;
    if (key == BSP_INPUT_NAVIGATION_KEY_LEFT) mesh_map_pan(&map_view, -96, 0);
    else if (key == BSP_INPUT_NAVIGATION_KEY_RIGHT) mesh_map_pan(&map_view, 96, 0);
    else if (key == BSP_INPUT_NAVIGATION_KEY_UP) mesh_map_pan(&map_view, 0, -96);
    else if (key == BSP_INPUT_NAVIGATION_KEY_DOWN) mesh_map_pan(&map_view, 0, 96);
    else if (key == BSP_INPUT_NAVIGATION_KEY_RETURN) map_open_actions();
    else return false;
    return true;
}
static bool map_key(char key) {
    if (current_screen != SCREEN_MAP) return false;
    if (key == '+' || key == '=') mesh_map_zoom(&map_view, 1);
    else if (key == '-' || key == '_') mesh_map_zoom(&map_view, -1);
    else if (key == 'r' || key == 'R') mesh_map_tiles_retry();
    else if (key == '\t') map_next_node();
    else if (key == 't' || key == 'T') map_trail_mode=(map_trail_mode+1)%3;
    else if (key == 'f' || key == 'F') map_filters.favorites_only = !map_filters.favorites_only;
    else if (key == 'o' || key == 'O') map_filters.recent_only = !map_filters.recent_only;
    else if (key == 'v' || key == 'V') {
        map_filters.waypoints_hidden = !map_filters.waypoints_hidden;
        if (map_filters.waypoints_hidden) map_waypoint_id = 0;
    }
    else if (key == 'c' || key == 'C') map_center_selected();
    else if (key == '\r' || key == '\n') map_open_actions();
    mesh_node_t selected;
    if (map_selected_node && !map_located_node(map_selected_node, &selected)) map_selected_node = 0;
    return true; // Other text must not trigger hidden menus or modify a draft.
}
#include "map_trails_draw.inc"

static void draw_map_screen(void) {
    const int x = 20, y = 94, w = (int)logical_h_res - 40, h = (int)logical_v_res - 252;
    char status_text[80], label[160];
    time_t now = mesh_radio_time();
    mesh_node_t visible_selection;
    if (map_selected_node && !map_located_node(map_selected_node, &visible_selection)) map_selected_node = 0;
    size_t located = 0;
    for (size_t i = 0; i < mesh_radio_node_count(); i++) {
        mesh_node_t node; if (mesh_radio_get_node(i, &node) && mesh_map_node_visible(&map_filters, &node, now)) located++;
    }
    snprintf(label, sizeof(label), "Z%u | %u nodes | %s | %s | Waypoints:%s | Trails:%s", map_view.zoom,
        (unsigned)located, map_filters.favorites_only ? "Favorites" : "All nodes",
        map_filters.recent_only ? "Recent <=1h" : "All ages", map_filters.waypoints_hidden ? "Off" : "On",
        map_trail_mode==MAP_TRAILS_OFF ? "Off" : map_trail_mode==MAP_TRAILS_SELECTED ? "Selected" : "All");
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 11, 20, 70, label);
    pax_draw_rect(&fb, UI_PANEL, x, y, w, h);
    mesh_map_tiles_request(&map_view, w, h);
    mesh_map_tiles_draw(&fb, x, y, &map_view, w, h, status_text, sizeof(status_text));
    pax_clip(&fb, x, y, w, h);
    draw_map_trails(x,y,w,h,now);
    for (size_t pass = 0; pass < 2; pass++) for (size_t i = 0; i < mesh_radio_node_count(); i++) {
        mesh_node_t node;
        if (!mesh_radio_get_node(i, &node) || !mesh_map_node_visible(&map_filters, &node, now) || (node.number == map_selected_node) != (pass == 1)) continue;
        double px, py;
        if (!mesh_map_marker(&map_view, node.latitude_i, node.longitude_i, w, h, &px, &py)) continue;
        bool old = !mesh_map_position_recent(&node, now);
        pax_col_t color = node.number == mesh_radio_local_node() ? 0xFF36B8FF : old ? 0xFFAA9B79 : UI_GREEN;
        int mx = x + (int)px, my = y + (int)py;
        pax_draw_circle(&fb, 0xFF101513, mx, my, pass ? 8 : 6);
        pax_draw_circle(&fb, color, mx, my, pass ? 5 : 3);
        if (pass) {
            pax_outline_circle(&fb, WHITE, mx, my, 9);
            snprintf(label, sizeof(label), "%.20s", node.name);
            int tw = (int)pax_text_size(pax_font_sky_mono, 12, label).x;
            int lx = mx + 12; if (lx + tw + 8 > x + w) lx = x + w - tw - 8;
            if (lx < x) lx = x;
            int ly = my > y + 25 ? my - 23 : my + 12;
            pax_draw_rect(&fb, 0xFF101513, lx, ly, tw + 8, 20);
            pax_draw_text(&fb, color, pax_font_sky_mono, 12, lx + 4, ly + 3, label);
        }
    }
    pax_noclip(&fb);
    pax_outline_rect(&fb, UI_BORDER, x, y, w, h);
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, x, y+h+8, status_text);
    mesh_node_t selected;
    mesh_waypoint_t focused;
    bool waypoint_focus=map_focused_waypoint(&focused);
    if(waypoint_focus)snprintf(label,sizeof(label),"Waypoint: %.18s  %.5f, %.5f / %s",focused.name,focused.latitude_i/1e7,focused.longitude_i/1e7,focused.locked_to?"locked":"open");
    else if (map_located_node(map_selected_node, &selected)) {
        char age[32];
        if (!selected.position_time || now < (time_t)selected.position_time) snprintf(age, sizeof(age), "age unknown");
        else {
            unsigned long elapsed = (unsigned long)(now - selected.position_time);
            if (elapsed < 3600) snprintf(age, sizeof(age), "%lum ago", elapsed/60);
            else if (elapsed < 86400) snprintf(age, sizeof(age), "%luh ago", elapsed/3600);
            else snprintf(age, sizeof(age), "%lud ago", elapsed/86400);
        }
        snprintf(label, sizeof(label), "%.18s  %.5f, %.5f  / %s", selected.name,
            selected.latitude_i/1e7, selected.longitude_i/1e7, age);
        char trail[64];map_trail_summary(selected.number,now,trail,sizeof(trail));
        size_t length=strlen(label);if(trail[0])snprintf(label+length,sizeof(label)-length," | %s",trail);
    } else snprintf(label, sizeof(label), "%s", !located && (map_filters.favorites_only || map_filters.recent_only)
        ? "No nodes match filters. F: all/favorites  O: all/recent positions."
        : "Tab: select a visible node. Map center is not your location.");
    pax_draw_text(&fb, WHITE, pax_font_sky_mono, 11, x, y+h+27, label);
    mesh_node_t own; double meters, bearing;
    if(waypoint_focus)selected=(mesh_node_t){.position_available=true,.latitude_i=focused.latitude_i,.longitude_i=focused.longitude_i,.position_time=focused.timestamp};
    if ((waypoint_focus||map_located_node(map_selected_node, &selected)) && mesh_radio_get_node_by_number(mesh_radio_local_node(), &own) &&
        selected.number != own.number && mesh_location_distance(&own, &selected, &meters, &bearing)) {
        char formatted[32];mesh_units_distance(distance_units,meters,formatted,sizeof(formatted));
        snprintf(label, sizeof(label), "Approx. %s %s / %.0f deg true  (%s)", formatted,
            mesh_location_compass(bearing), bearing,
            !own.position_time || !selected.position_time || now < own.position_time || now < selected.position_time ||
            now-own.position_time>3600 || now-selected.position_time>3600 ? "old / unknown fix age" : waypoint_focus ? "chosen waypoint" : "reported positions");
    } else snprintf(label, sizeof(label), "Distance needs your location and a selected remote node position.");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, x, y+h+46, label);
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, x, y+h+66,
        "Map: (c) OpenStreetMap contributors / ODbL / tiles.coalition.space");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET - 16,
        "Arrows:pan  +/-:zoom  Tab:node  C:center  Enter:actions  W:waypoints  N:new");
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 10, 20, logical_v_res - UI_HINT_OFFSET,
        "F:all/favorites  O:all/recent (1h)  V:waypoint markers  T:trails  R:reload");
}
