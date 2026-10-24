#include "node_details.h"
#include <stdio.h>
const char *mesh_node_role_name(const mesh_node_t *node) {
    static const char *roles[] = {"Client", "Client Mute", "Router", "Router Client", "Repeater", "Tracker", "Sensor", "TAK", "Client Hidden", "Lost and Found", "TAK Tracker", "Router Late", "Client Base"};
    if (!node || !node->user_info_available) return "Unavailable";
    return node->role < sizeof(roles) / sizeof(roles[0]) ? roles[node->role] : "Unknown role";
}
void mesh_node_public_key_text(const mesh_node_t *node, char out[45]) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (!node || !node->public_key_available) { snprintf(out, 45, "Unavailable"); return; }
    size_t pos = 0;
    for (size_t i = 0; i < 32; i += 3) {
        uint32_t n = (uint32_t)node->public_key[i] << 16;
        if (i + 1 < 32) n |= (uint32_t)node->public_key[i + 1] << 8;
        if (i + 2 < 32) n |= node->public_key[i + 2];
        out[pos++] = alphabet[n >> 18]; out[pos++] = alphabet[(n >> 12) & 63];
        out[pos++] = i + 1 < 32 ? alphabet[(n >> 6) & 63] : '=';
        out[pos++] = i + 2 < 32 ? alphabet[n & 63] : '=';
    }
    out[pos] = '\0';
}

const char *mesh_hardware_model_name(uint32_t model) {
    static const struct { uint32_t id; const char *name; } models[] = {
#include "hardware_models.inc"
    };
    for (size_t i = 0; i < sizeof(models) / sizeof(models[0]); i++) if (models[i].id == model) return models[i].name;
    return "Unknown model";
}
