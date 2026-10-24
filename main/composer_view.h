#pragma once
#include "emoji_text.h"

typedef struct {
    int total_lines, first_line, visible_lines;
    int caret_x, caret_y;
} mesh_compose_view_t;

// Show the tail of an append-only draft without clipping its insertion point.
mesh_compose_view_t mesh_compose_layout(const char *text, int width, int advance, int max_visible,
                                       mesh_text_draw_fn draw, void *context);
void mesh_compose_backspace(char *text);
