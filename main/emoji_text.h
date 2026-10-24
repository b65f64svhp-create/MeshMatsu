#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MESH_EMOJI_SIZE 20
typedef struct {
    size_t bytes;
    int glyph; // -1 for ordinary text or a visible unsupported-codepoint label.
    char text[16];
    int width;
    bool space, newline;
} mesh_text_token_t;
typedef void (*mesh_text_draw_fn)(const mesh_text_token_t *token, int x, int y, void *context);

mesh_text_token_t mesh_text_next(const char *text, int advance);
uint8_t mesh_emoji_alpha(int glyph, unsigned x, unsigned y);
int mesh_text_line_count(const char *text, int width, int advance);
int mesh_text_layout(const char *text, int width, int advance, int line_height, int max_lines,
                     mesh_text_draw_fn draw, void *context);
