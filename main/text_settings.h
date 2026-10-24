#pragma once
#include <stdint.h>
#include "esp_err.h"
enum { MESH_TEXT_SMALL, MESH_TEXT_MEDIUM, MESH_TEXT_LARGE };
int mesh_message_font(uint8_t size);
int mesh_message_line_height(uint8_t size);
const char *mesh_message_size_name(uint8_t size);
esp_err_t mesh_text_size_load(uint8_t *size);
esp_err_t mesh_text_size_save(uint8_t size);
