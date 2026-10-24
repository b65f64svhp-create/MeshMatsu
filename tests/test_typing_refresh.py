from ui_source import read_main_ui
"""Check actual refresh scheduling and text batching without screen/radio writes."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = read_main_ui(root)
a = source.index('typedef struct {\n    rich_text_context_t base;')
b = source.index('static int message_compose_height', a)
paint = source[a:b]
a = source.index('static void queue_compose_redraw(void)')
b = source.index('static void submit_message_draft', a)
refresh = source[a:b]
harness = '#include "'+str(root / 'main/emoji_text.c')+'"\n#include "'+str(root / 'main/composer_view.c')+'"\n'
harness += r'''
#include <assert.h>
#include <math.h>
typedef unsigned pax_col_t;
typedef struct { float x, y; } pax_vec2f;
typedef struct { int x, y, font_size; pax_col_t color; } rich_text_context_t;
static int fb;
static int *pax_font_sky_mono;
static unsigned text_calls, glyph_calls, full_draws, widget_draws, blits, clears;
static int caret_min_x, caret_max_x;
static pax_vec2f pax_text_size(int *f, int size, const char *s) { return (pax_vec2f){strlen(s) * 12.444f, 16}; }
static void pax_draw_text(int *f, unsigned c, int *font, int size, float x, float y, const char *s) { text_calls++; }
static void draw_rich_token(const mesh_text_token_t *t, int x, int y, void *context) { glyph_calls++; }
#define BLACK 0
#define UI_HINT_OFFSET 60
#define UI_FOOTER_HEIGHT 42
#define SCREEN_MESSAGES 1
#define SCREEN_HOME 0
static bool compose_redraw_pending, display_and_lights_off, displayed_chat = true;
static struct {unsigned modal;} chat_actions;
static int64_t compose_redraw_due_us, last_text_edit_us, fake_now;
static int current_screen = SCREEN_MESSAGES, displayed_compose_height = 34, desired_height = 34;
static unsigned message_peer = 1, displayed_peer = 1;
static unsigned char message_channel, displayed_channel;
static size_t logical_h_res = 800, logical_v_res = 480;
static int64_t esp_timer_get_time(void) { return fake_now; }
static int message_compose_height(void) { return desired_height; }
static void draw_screen(void) { full_draws++; compose_redraw_pending = false; }
static void draw_message_composer(int height) { assert(height == desired_height); widget_draws++; }
static void blit(void) { blits++; }
static void pax_draw_rect(int *f, unsigned c, int x, int y, int w, int h) {
    assert(y == 480 - 60 - 8 - desired_height);
    assert(y + h == 480 - 42 && x == 0 && w == 800); clears++;
}
'''+paint+refresh+r'''
int main(void) {
    fake_now = 1000000; queue_compose_redraw();
    int64_t first_due = compose_redraw_due_us;
    assert(first_due == 1020000 && !compose_refresh_due(1019999, false));
    // More text, separated by scancode/release events, keeps the first deadline.
    for (int i = 1; i <= 19; i++) { fake_now += 1000; queue_compose_redraw(); assert(compose_redraw_due_us == first_due); }
    assert(compose_refresh_due(first_due, false) && !compose_refresh_due(first_due, true));
    assert(!compose_refresh_due(first_due + 59999, true) && compose_refresh_due(first_due + 60000, true));
    redraw_composer(); assert(widget_draws == 1 && blits == 1 && clears == 1 && full_draws == 0 && !compose_redraw_pending);
    queue_compose_redraw(); desired_height = 56; redraw_composer();
    assert(full_draws == 1 && widget_draws == 1);
    desired_height = 34; displayed_peer = 2; queue_compose_redraw(); redraw_composer(); assert(full_draws == 2);
    displayed_peer = message_peer; display_and_lights_off = true; queue_compose_redraw(); redraw_composer();
    assert(!compose_redraw_pending && widget_draws == 1 && blits == 1);
    display_and_lights_off = false; current_screen = SCREEN_HOME; queue_compose_redraw(); redraw_composer();
    assert(!compose_redraw_pending && full_draws == 2);
    current_screen = SCREEN_MESSAGES; chat_actions.modal = 1; queue_compose_redraw(); redraw_composer();
    assert(!compose_redraw_pending && widget_draws == 1 && blits == 1);
    chat_actions.modal = 0;
    // A full 200-character ASCII draft now uses three text draws, not 200.
    char text[201]; memset(text, 'a', 200); text[200] = 0;
    compose_paint_t painter = {.base = {34, 340, 16, 0xffffff}};
    mesh_compose_view_t view = mesh_compose_layout(text, 724, 13, 3, draw_compose_token, &painter);
    flush_compose_run(&painter);
    assert(text_calls == 3 && glyph_calls == 0 && view.first_line > 0);
    assert(painter.caret_x <= 724 && painter.caret_y == 2);
    text_calls = glyph_calls = 0;
    painter = (compose_paint_t){.base = {34, 340, 16, 0xffffff}};
    mesh_compose_layout("Text before 👍🏽 and after", 724, 13, 3, draw_compose_token, &painter);
    flush_compose_run(&painter);
    assert(glyph_calls == 1 && text_calls == 2 && painter.caret_x <= 724);
}
'''
with tempfile.TemporaryDirectory(prefix='meshdeck-typing-refresh-') as folder:
    path = Path(folder)
    (path / 'test.c').write_text(harness)
    subprocess.run(['clang', '-std=c11', '-fsanitize=address,undefined', str(path / 'test.c'), '-lm', '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: fixed refresh deadline across input bursts, bounded backlog delay, composer-only redraw, full layout on resize/peer change, no redraw asleep, 200-character draft in 3 text calls')
