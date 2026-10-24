from ui_source import read_main_ui
"""Exercise the real manual form with simulated radio snapshots and UART."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = read_main_ui(root)
a = source.index('typedef enum {'); b = source.index('} meshdeck_screen_t;', a) + len('} meshdeck_screen_t;')
enum = source[a:b]
a = source.index('static size_t manual_field'); b = source.index('static char manual_feedback[80];', a) + len('static char manual_feedback[80];')
globals_ = source[a:b]
a = source.index('static void open_manual_channel(void)'); b = source.index('// Additional settings navigation.', a)
functions = source[a:b]
a = source.index('    } else if (current_screen == SCREEN_MANUAL_CHANNEL) {\n        mesh_channel_t slot;')
a = source.index('\n', a) + 1
b = source.index('        char target[64];', a)
update = source[a:b]
harness = '#define main backend_tests\n#include "'+str(root / 'tests/test_direct_messages.c')+'"\n#undef main\n'+enum+'''
static meshdeck_screen_t current_screen;
'''+globals_+functions+'\nstatic void update_form(void) {\n'+update+'''}
static protobuf_field_t find(const uint8_t *bytes, size_t size, uint32_t field) {
    size_t pos = 0; protobuf_field_t f;
    while (pos < size) { assert(read_field(bytes, size, &pos, &f)); if (f.field == field) return f; }
    assert(!"Expected field"); return f;
}
int main(void) {
    reset(); memset(channels, 0, sizeof(channels)); session_passkey_size = 8;
    const uint8_t enabled1[] = {8,1,24,2}; const uint8_t free2[] = {8,2};
    decode_channel(enabled1, sizeof(enabled1)); decode_channel(free2, sizeof(free2));
    open_manual_channel();
    assert(current_screen == SCREEN_MANUAL_CHANNEL && manual_slot == 2 && !manual_key_visible);
    manual_field = 1;
    const char *name = "Private"; for (size_t i=0; name[i]; i++) apply_manual_key(name[i]);
    apply_manual_key('\\t'); assert(manual_field == 2);
    apply_manual_key('!'); save_manual_channel();
    assert(manual_phase == 0 && strstr(manual_feedback, "Invalid key"));
    apply_manual_key('\\b');
    const char *key = "AQ=="; for (size_t i=0; key[i]; i++) apply_manual_key(key[i]);
    apply_manual_key(' '); assert(manual_key_visible);
    apply_manual_key('\\t'); assert(manual_field == 3 && !manual_key_visible);
    save_manual_channel(); assert(manual_phase == 1);
    update_form(); assert(manual_phase == 1); // No write from a stale cache snapshot.
    const uint8_t occupied2[] = {8,2,24,2}; decode_channel(occupied2, sizeof(occupied2));
    update_form(); assert(manual_phase == 0 && strstr(manual_feedback, "occupied"));
    const uint8_t free3[] = {8,3}; decode_channel(free3, sizeof(free3)); manual_slot = 3;
    save_manual_channel(); assert(manual_phase == 1);
    decode_channel(free3, sizeof(free3)); // Fresh read confirms the slot is still unused.
    update_form(); assert(manual_phase == 2 && manual_key[0]);
    protobuf_field_t packet = find(sent_bytes+4, sent_size-4, 1);
    protobuf_field_t data = find(packet.value, packet.length, 4);
    protobuf_field_t admin = find(data.value, data.length, 2);
    protobuf_field_t channel = find(admin.value, admin.length, 33);
    decode_channel(channel.value, channel.length);
    update_form(); assert(manual_phase == 0 && strstr(manual_feedback, "confirmed"));
    for (size_t i=0; i<sizeof(manual_key); i++) assert(manual_key[i] == 0);
    assert(!manual_key_visible);
    puts("PASS: manual form masking, validation, fresh unused-slot check, write/readback, key clearing");
}
'''
with tempfile.TemporaryDirectory(prefix='meshdeck-form-') as folder:
    path = Path(folder)
    (path / 'test.c').write_text(harness)
    subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'tests/stubs'),str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],cwd=folder,check=True)
