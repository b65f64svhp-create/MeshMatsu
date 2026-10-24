from ui_source import read_main_ui
"""Exercise actual edit handlers, scrolling layout, and long-message wire bytes."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = read_main_ui(root)
a = source.index('static void submit_message_draft(void)')
b = source.index('static void save_settings_names', a)
editing = source[a:b]
harness = '#define main backend_tests\n#include "'+str(root / 'tests/test_direct_messages.c')+'"\n#undef main\n'
harness += '#include "'+str(root / 'main/emoji_text.c')+'"\n#include "'+str(root / 'main/composer_view.c')+'"\n'
harness += r'''
#define CONFIG_BSP_TARGET_TANMATSU 1
#define BSP_INPUT_NAVIGATION_KEY_BACKSPACE 7
static char draft_storage[MESH_TEXT_MAX_LENGTH + 1], message_feedback[64];
static char *message_draft = draft_storage;
static uint32_t message_peer;
static uint8_t message_channel;
static uint32_t draft_reply_ids[188];
static size_t draft_slot;
static size_t message_scroll_offset;
static void draft_ui_touch(void){}
'''+editing+r'''
static int draw_calls;
static void check_token(const mesh_text_token_t *token, int x, int y, void *context) {
    int width = *(int *)context;
    assert(x >= 0 && x + token->width <= width && y >= 0 && y <= 44);
    draw_calls++;
}
static protobuf_field_t find_field(const uint8_t *data, size_t size, uint32_t tag) {
    size_t pos = 0; protobuf_field_t field;
    while (pos < size) { assert(read_field(data, size, &pos, &field)); if (field.field == tag) return field; }
    assert(!"Missing required wire field"); return (protobuf_field_t){0};
}
static void check_sent(const char *text, uint32_t destination, unsigned channel) {
    size_t wire_size = ((size_t)sent_bytes[2] << 8) | sent_bytes[3];
    assert(sent_size == wire_size + 4);
    protobuf_field_t packet = find_field(sent_bytes + 4, wire_size, 1);
    protobuf_field_t to = find_field(packet.value, packet.length, 2);
    uint32_t actual; memcpy(&actual, to.value, 4); assert(actual == destination);
    if (channel) { protobuf_field_t ch = find_field(packet.value, packet.length, 3); assert(ch.integer == channel); }
    protobuf_field_t data = find_field(packet.value, packet.length, 4);
    protobuf_field_t payload = find_field(data.value, data.length, 2);
    assert(payload.length == strlen(text) && !memcmp(payload.value, text, payload.length));
}
int main(void) {
    backend_tests(); reset();
    message_peer = 0x11223344;
    strcpy(message_draft, "abcdef");
    // Real Tanmatsu sequence is NAVIGATION down, then KEYBOARD '\b'.
    if (!keyboard_owns_delete(BSP_INPUT_NAVIGATION_KEY_BACKSPACE)) apply_message_key('\b');
    apply_message_key('\b'); assert(!strcmp(message_draft, "abcde"));
    assert(!keyboard_owns_delete(99));
    apply_message_key('\b'); apply_message_key('\b'); assert(!strcmp(message_draft, "abc"));
    strcpy(message_draft, "it’s");
    apply_message_key('\b'); assert(!strcmp(message_draft, "it’"));
    apply_message_key('\b'); assert(!strcmp(message_draft, "it"));
    strcpy(message_draft, "Aé👍🏽"); apply_message_key('\b'); assert(!strcmp(message_draft, "Aé"));
    apply_message_key('\b'); assert(!strcmp(message_draft, "A"));
    apply_message_key('\b'); apply_message_key('\b'); assert(!message_draft[0]);
    for (int i = 0; i < 200; i++) apply_message_key('x');
    assert(strlen(message_draft) == 200);
    apply_message_key('y'); assert(strlen(message_draft) == 200 && strstr(message_feedback, "limit"));
    short_write = true; submit_message_draft(); assert(strlen(message_draft) == 200);
    short_write = false; submit_message_draft(); assert(!message_draft[0] && strlen(messages[0].text) == 200);
    // Failed reply sends retain both the draft and quote, successful sends clear both.
    draft_reply_ids[0]=123;strcpy(message_draft,"A quoted response");
    short_write=true;submit_message_draft();assert(draft_reply_ids[0]==123&&message_draft[0]);
    short_write=false;submit_message_draft();assert(!draft_reply_ids[0]&&!message_draft[0]&&messages[1].reply_id==123);
    const int lengths[] = {1, 70, 126, 127, 128, 199, 200};
    const uint8_t secondary[] = {8,1,24,2}; decode_channel(secondary, sizeof(secondary));
    for (unsigned i = 0; i < sizeof(lengths)/sizeof(lengths[0]); i++) {
        char text[202]; memset(text, 'a' + i, lengths[i]); text[lengths[i]] = 0;
        assert(mesh_radio_send_direct(message_peer, text)); check_sent(text, message_peer, 0);
        assert(mesh_radio_send_channel(1, text)); check_sent(text, UINT32_MAX, 1);
    }
    char unicode[201] = {0}; for (int i = 0; i < 50; i++) memcpy(unicode + i*4, "👍", 4);
    assert(mesh_radio_send_direct(message_peer, unicode)); check_sent(unicode, message_peer, 0);
    char too_long[202]; memset(too_long, 'x', 201); too_long[201] = 0;
    size_t before = message_count; assert(!mesh_radio_send_direct(message_peer, too_long) && message_count == before);
    for (int width = 404; width <= 724; width += 320) {
        for (int len = 0; len <= 200; len++) {
            char text[201]; memset(text, 'a', len); text[len] = 0;
            draw_calls = 0;
            mesh_compose_view_t view = mesh_compose_layout(text, width, 13, 3, check_token, &width);
            assert(view.total_lines == mesh_text_line_count(text, width, 13));
            assert(view.visible_lines >= 1 && view.visible_lines <= 3);
            assert(view.first_line == view.total_lines - view.visible_lines);
            assert(view.caret_x >= 0 && view.caret_x <= width && view.caret_y >= 0 && view.caret_y < view.visible_lines);
            assert(draw_calls || !len);
        }
        mesh_compose_view_t view = mesh_compose_layout(too_long, width, 13, 3, check_token, &width);
        assert(view.first_line > 0 && view.caret_y == 2);
        view = mesh_compose_layout("Words wrap safely without losing the insertion point 👍🏽", width, 13, 3, check_token, &width);
        assert(view.caret_x <= width);
    }
    puts("PASS: one delete per physical key, Unicode-safe erase, 200-byte editing/send boundaries, long protobuf lengths, exact DM/channel payloads, growing/scrolled draft and caret bounds");
}
'''
with tempfile.TemporaryDirectory(prefix='meshdeck-composer-') as folder:
    path = Path(folder)
    (path / 'test.c').write_text(harness)
    subprocess.run(['clang', '-std=c11', '-fsanitize=address,undefined', '-I', str(root / 'tests/stubs'), str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], cwd=folder, check=True)
