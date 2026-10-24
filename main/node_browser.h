#pragma once
#include "radio_link.h"
#include <time.h>

typedef enum { MESH_NODES_ALL, MESH_NODES_ACTIVE, MESH_NODES_FAVORITES, MESH_NODES_NEARBY, MESH_NODES_IGNORED, MESH_NODE_FILTER_COUNT } mesh_node_filter_t;
typedef enum { MESH_NODES_LAST_HEARD, MESH_NODES_NAME, MESH_NODES_HOPS, MESH_NODES_DISTANCE, MESH_NODE_SORT_COUNT } mesh_node_sort_t;
typedef struct {
    mesh_node_filter_t filter;
    mesh_node_sort_t sort;
    bool favorites_first;
    uint32_t nearby_meters; // Zero defaults to 25 km; +/- cycles bounded presets.
    bool own_position_available, own_position_old;
    char query[40];
    uint32_t selected;
    size_t count;
    mesh_node_t nodes[MESH_NODE_LIMIT];
    double distances[MESH_NODE_LIMIT]; // Metres; NAN if either position is unavailable.
    bool position_old[MESH_NODE_LIMIT]; // Either fix is >1 hour, future or unknown age.
} mesh_node_browser_t;

// Uses radio snapshots only; never writes radio configuration or favorites.
size_t mesh_node_browser_refresh(mesh_node_browser_t *view, time_t now, size_t fallback);
const char *mesh_node_filter_name(mesh_node_filter_t filter);
const char *mesh_node_sort_name(mesh_node_sort_t sort);

uint32_t mesh_node_browser_radius(const mesh_node_browser_t *view);
void mesh_node_browser_step_radius(mesh_node_browser_t *view,bool forward);
