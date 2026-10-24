#pragma once
#include "radio_link.h"
#include <time.h>

typedef struct {
    bool favorites_only;
    bool recent_only;
    bool waypoints_hidden;
} mesh_map_filters_t;

static inline bool mesh_map_position_recent(const mesh_node_t *node, time_t now) {
    return now >= 1700000000 && node->position_time >= 1700000000 &&
        now >= (time_t)node->position_time && now - (time_t)node->position_time <= 3600;
}

static inline bool mesh_map_node_visible(const mesh_map_filters_t *filters,
                                        const mesh_node_t *node, time_t now) {
    return !node->is_ignored && node->position_available &&
        (!filters->favorites_only || node->is_favorite) &&
        (!filters->recent_only || mesh_map_position_recent(node, now));
}
