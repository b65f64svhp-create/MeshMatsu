#include "node_browser.h"
#include "location_utils.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static unsigned char fold(unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
static int compare_text(const char *a, const char *b) {
    while (*a && fold(*a) == fold(*b)) { a++; b++; }
    return (int)fold(*a) - (int)fold(*b);
}
static bool contains(const char *text, const char *query) {
    if (!*query) return true;
    for (; *text; text++) {
        size_t i = 0;
        while (query[i] && text[i] && fold(text[i]) == fold(query[i])) i++;
        if (!query[i]) return true;
    }
    return false;
}
static bool matches(const mesh_node_t *node, const char *query) {
    char hex[12], decimal[16];
    snprintf(hex, sizeof(hex), "!%08lx", (unsigned long)node->number);
    snprintf(decimal, sizeof(decimal), "%lu", (unsigned long)node->number);
    return contains(node->name, query) || contains(node->advertised_name, query) || contains(node->short_name, query) ||
        contains(node->user_id, query) || contains(hex, query) || contains(decimal, query);
}
static bool favorite(const mesh_node_t *node) { return node->favorite_known && node->is_favorite; }
static uint32_t heard(const mesh_node_t *node, time_t now) {
    return now >= 1700000000 && node->last_heard >= 1700000000U &&
        (uint64_t)node->last_heard <= (uint64_t)now ? node->last_heard : 0;
}
static int compare(const mesh_node_t *a, const mesh_node_t *b, double ad, double bd, const mesh_node_browser_t *view, time_t now) {
    if(view->sort==MESH_NODES_DISTANCE&&isfinite(ad)!=isfinite(bd))return isfinite(ad)?-1:1;
    if (view->favorites_first && favorite(a) != favorite(b)) return favorite(a) ? -1 : 1;
    if (view->sort == MESH_NODES_LAST_HEARD) {
        uint32_t at = heard(a, now), bt = heard(b, now);
        if (at != bt) return at > bt ? -1 : 1;
    } else if (view->sort == MESH_NODES_HOPS) {
        if (a->hops_away_available != b->hops_away_available) return a->hops_away_available ? -1 : 1;
        if (a->hops_away_available && a->hops_away != b->hops_away) return a->hops_away < b->hops_away ? -1 : 1;
    } else if (view->sort == MESH_NODES_DISTANCE) {
        bool ak=isfinite(ad),bk=isfinite(bd);
        if(ak!=bk)return ak?-1:1;
        if(ak&&ad!=bd)return ad<bd?-1:1;
    }
    const char *an = a->name[0] ? a->name : a->short_name;
    const char *bn = b->name[0] ? b->name : b->short_name;
    int text = compare_text(an, bn);
    if (text) return text;
    return a->number < b->number ? -1 : a->number != b->number;
}
static bool has_position(const mesh_node_t *node){
    return node->position_available&&!node->is_ignored&&node->latitude_i>=-900000000&&node->latitude_i<=900000000&&
        node->longitude_i>=-1800000000&&node->longitude_i<=1800000000;
}
static bool old_position(const mesh_node_t *node,time_t now){
    return now<1700000000||node->position_time<1700000000U||(uint64_t)node->position_time>(uint64_t)now||
        (uint64_t)now-node->position_time>3600;
}
static const uint32_t nearby_radii[]={5000,10000,25000,50000,100000,250000};
uint32_t mesh_node_browser_radius(const mesh_node_browser_t *view){
    for(unsigned i=0;i<sizeof(nearby_radii)/sizeof(*nearby_radii);i++)if(view->nearby_meters==nearby_radii[i])return view->nearby_meters;
    return 25000;
}
void mesh_node_browser_step_radius(mesh_node_browser_t *view,bool forward){
    unsigned count=sizeof(nearby_radii)/sizeof(*nearby_radii),at=2;
    uint32_t radius=mesh_node_browser_radius(view);
    for(unsigned i=0;i<count;i++)if(radius==nearby_radii[i]){at=i;break;}
    view->nearby_meters=nearby_radii[(at+(forward?1:count-1))%count];
}
size_t mesh_node_browser_refresh(mesh_node_browser_t *view, time_t now, size_t fallback) {
    view->count = 0;
    size_t total = mesh_radio_node_count();
    uint32_t local = mesh_radio_local_node();
    mesh_node_t own={0};
    view->own_position_available=local&&local!=MESH_BROADCAST_NODE&&mesh_radio_get_node_by_number(local,&own)&&own.number==local&&has_position(&own);
    view->own_position_old=view->own_position_available&&old_position(&own,now);
    for (size_t i = 0; i < total && view->count < MESH_NODE_LIMIT; i++) {
        mesh_node_t node;
        if (!mesh_radio_get_node(i, &node) || !node.number || node.number == MESH_BROADCAST_NODE || node.number == local) continue;
        if ((view->filter == MESH_NODES_IGNORED) != node.is_ignored) continue;
        uint32_t last = heard(&node, now);
        if (view->filter == MESH_NODES_ACTIVE && (!last || (uint64_t)now - last > 7200)) continue;
        if (view->filter == MESH_NODES_FAVORITES && !favorite(&node)) continue;
        if (!matches(&node, view->query)) continue;
        double distance=NAN,bearing;
        if(view->own_position_available&&has_position(&node))mesh_location_distance(&own,&node,&distance,&bearing);
        bool old=view->own_position_old||old_position(&node,now);
        if(view->filter==MESH_NODES_NEARBY&&(!isfinite(distance)||distance>mesh_node_browser_radius(view)))continue;
        size_t position = view->count++;
        while (position && compare(&node, &view->nodes[position - 1], distance, view->distances[position-1], view, now) < 0) {
            view->nodes[position] = view->nodes[position - 1];
            view->distances[position]=view->distances[position-1];
            view->position_old[position]=view->position_old[position-1];
            position--;
        }
        view->nodes[position] = node;
        view->distances[position]=distance;
        view->position_old[position]=old;
    }
    for (size_t i = 0; view->selected && i < view->count; i++) if (view->nodes[i].number == view->selected) return i;
    size_t selected = fallback < view->count ? fallback : view->count ? view->count - 1 : 0;
    view->selected = view->count ? view->nodes[selected].number : 0;
    return selected;
}
const char *mesh_node_filter_name(mesh_node_filter_t filter) {
    return filter == MESH_NODES_NEARBY ? "NEARBY" : filter == MESH_NODES_IGNORED ? "IGNORED (RADIO)" : filter == MESH_NODES_ACTIVE ? "ACTIVE (<2H)" : filter == MESH_NODES_FAVORITES ? "FAVORITES" : "ALL SEEN";
}
const char *mesh_node_sort_name(mesh_node_sort_t sort) {
    return sort == MESH_NODES_DISTANCE ? "Distance" : sort == MESH_NODES_NAME ? "Name" : sort == MESH_NODES_HOPS ? "Hop count" : "Last heard";
}
