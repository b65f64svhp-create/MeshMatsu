#pragma once
#include "radio_link.h"
const char *mesh_node_role_name(const mesh_node_t *node);
// Standard Base64 of the reported 32-byte public key. No key verification implied.
void mesh_node_public_key_text(const mesh_node_t *node, char out[45]);

const char *mesh_hardware_model_name(uint32_t model);
