#include "composer_view.h"

typedef struct {
    mesh_compose_view_t view;
    mesh_text_draw_fn draw;
    void *context;
} compose_context_t;

static void draw_tail(const mesh_text_token_t *token, int x, int y, void *opaque) {
    compose_context_t *context = opaque;
    int row = y - context->view.first_line;
    context->view.caret_x = x + token->width;
    context->view.caret_y = row;
    if (row >= 0 && row < context->view.visible_lines && context->draw) context->draw(token, x, row * 22, context->context);
}

mesh_compose_view_t mesh_compose_layout(const char *text, int width, int advance, int max_visible,
                                       mesh_text_draw_fn draw, void *context) {
    compose_context_t state = {.draw = draw, .context = context};
    state.view.total_lines = mesh_text_line_count(text, width, advance);
    if (max_visible < 1) max_visible = 1;
    state.view.visible_lines = state.view.total_lines < max_visible ? state.view.total_lines : max_visible;
    state.view.first_line = state.view.total_lines - state.view.visible_lines;
    mesh_text_layout(text, width, advance, 1, 1024, draw_tail, &state);
    // Empty drafts have no glyph callback but still need an insertion cursor.
    if (state.view.caret_y < 0) state.view.caret_y = 0;
    return state.view;
}

void mesh_compose_backspace(char *text) {
    if (!text) return;
    size_t offset = 0, previous = 0;
    while (text[offset]) {
        previous = offset;
        mesh_text_token_t token = mesh_text_next(text + offset, 1);
        if (!token.bytes) break;
        offset += token.bytes;
    }
    text[previous] = 0;
}
