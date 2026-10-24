#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct { const char *text, *name; } mesh_picker_emoji_t;
size_t mesh_picker_count(void);
const mesh_picker_emoji_t *mesh_picker_get(size_t index);
// Inserts into a draft only; never transmits it.
bool mesh_picker_insert(char *draft, size_t capacity, size_t index);
