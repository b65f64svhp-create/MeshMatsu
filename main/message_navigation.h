#pragma once
#include "radio_link.h"
typedef struct {bool valid;mesh_message_t message;} mesh_message_anchor_t;
size_t mesh_message_open_offset(uint32_t peer,uint8_t channel,mesh_message_anchor_t *anchor);
size_t mesh_message_resolve_offset(uint32_t peer,uint8_t channel,size_t offset,mesh_message_anchor_t *anchor);
void mesh_message_capture_anchor(uint32_t peer,uint8_t channel,size_t offset,mesh_message_anchor_t *anchor);
