from ui_source import read_main_ui
"""Exercise the production telemetry form against the real radio backend."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = read_main_ui(root)
enum = source[source.index("typedef enum {"):source.index("} meshdeck_screen_t;") + len("} meshdeck_screen_t;")]
harness = '#define main backend_tests\n#include "' + str(root / 'tests/test_direct_messages.c') + '"\n#undef main\n' + enum + '\n'
for index, key in enumerate(["UP", "DOWN", "LEFT", "RIGHT", "RETURN", "ESC", "F1", "F2", "F3", "F4", "F5", "F6"]):
    harness += f"#define BSP_INPUT_NAVIGATION_KEY_{key} {index + 1}\n"
harness += r'''
static meshdeck_screen_t current_screen = SCREEN_HOME;
static int fb, logical_h_res=800, logical_v_res=480;
static const void *pax_font_sky_mono;
#define UI_SELECTED 1
#define UI_PANEL 2
#define UI_GREEN 3
#define UI_BORDER 4
#define UI_MUTED 5
#define UI_HINT_OFFSET 39
#define pax_draw_rect(...) ((void)0)
typedef struct { float x,y; } test_vec_t;
#define pax_text_size(...) ((test_vec_t){.x=20})
static char drawn[40][160]; static size_t drawn_count;
static void record(const char *text) { assert(drawn_count<40);snprintf(drawn[drawn_count++],160,"%s",text); }
static void pax_draw_text(void *buf,int color,const void *font,float size,float x,float y,const char *text) { record(text); }
static void draw_rich_text(const char *text,float x,float y,float size,float width,float height,int lines,int color) { record(text); }
static bool shown(const char *text) { for(size_t i=0;i<drawn_count;i++)if(strstr(drawn[i],text))return true;return false; }
'''
harness += (root / "main/telemetry_settings_ui.inc").read_text()
harness += r'''
static void draw(void) { drawn_count=0;draw_telemetry_settings(); }
static void config(unsigned interval,bool enabled,bool with_switch) {
    uint8_t raw[20],m[40];size_t n=0,mn=0;
    append_varint(raw,sizeof(raw),&n,8);append_varint(raw,sizeof(raw),&n,interval);
    if(with_switch){append_varint(raw,sizeof(raw),&n,112);append_varint(raw,sizeof(raw),&n,enabled);}
    append_bytes_field(m,sizeof(m),&mn,6,raw,n);assert(decode_telemetry_config(m,mn,0));
}
static void ready(void) {
    reset();fake_time=1000000;last_radio_activity_us=last_rx_byte_us=last_heartbeat_us=fake_time;
    lora_config.loaded=true;session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;
    radio_metadata.loaded=true;strcpy(radio_metadata.firmware_version,"2.7.18");
    config(1800,false,false);open_telemetry_settings();draw();
    assert(current_screen==SCREEN_TELEMETRY_SETTINGS&&telemetry_settings_loaded&&!telemetry_settings_edited);
}
static void select_save(void) { telemetry_settings_field=2;telemetry_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN); }
int main(void) {
    reset();current_screen=SCREEN_HOME;
    assert(!telemetry_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT)&&!telemetry_settings_key(' '));
    open_telemetry_settings();draw();assert(!telemetry_settings_loaded&&shown("Loading..."));
    uint32_t writes=telemetry_config_op.write_id;telemetry_settings_step(true);select_save();
    assert(!telemetry_settings_edited&&!telemetry_settings_pending&&telemetry_config_op.write_id==writes);
    assert(!telemetry_settings_navigation(BSP_INPUT_NAVIGATION_KEY_ESC));

    ready();assert(shown("Off")&&shown("30 min"));
    telemetry_settings_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
    assert(telemetry_draft.enabled&&telemetry_settings_edited&&telemetry_save.state==MESH_TELEMETRY_CONFIG_IDLE);
    telemetry_settings_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);assert(!telemetry_draft.enabled);
    // Enter on interval is a draft edit; only Save is a radio mutation.
    telemetry_settings_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);telemetry_settings_key('\r');
    assert(telemetry_draft.interval_secs==3600&&telemetry_save.state==MESH_TELEMETRY_CONFIG_IDLE);
    telemetry_settings_key(' ');assert(telemetry_draft.interval_secs==10800);
    telemetry_draft.interval_secs=86400;telemetry_settings_step(true);assert(!telemetry_draft.interval_secs);
    draw();assert(shown("Radio default"));telemetry_settings_step(false);assert(telemetry_draft.interval_secs==86400);
    telemetry_draft.interval_secs=1234;telemetry_settings_step(false);assert(!telemetry_draft.interval_secs);
    telemetry_draft.interval_secs=1234;telemetry_settings_step(true);assert(!telemetry_draft.interval_secs);
    telemetry_settings_field=2;telemetry_settings_step(true);assert(!telemetry_settings_pending);
    telemetry_settings_key('\t');assert(telemetry_settings_field==0);telemetry_settings_navigation(BSP_INPUT_NAVIGATION_KEY_UP);assert(!telemetry_settings_field);
    telemetry_settings_field=2;telemetry_settings_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);assert(telemetry_settings_field==2);

    // On old/unknown firmware, enable is read-only while interval stays editable.
    ready();radio_metadata.loaded=false;telemetry_enable_seen=false;telemetry_config.enable_supported=false;
    draw();assert(shown("Firmware controlled")&&!telemetry_draft.enable_supported);
    telemetry_settings_step(true);assert(!telemetry_draft.enabled&&!telemetry_settings_edited);
    assert(strstr(telemetry_settings_feedback,"interval changes only"));
    telemetry_settings_field=1;telemetry_settings_step(true);assert(telemetry_settings_edited&&telemetry_draft.interval_secs==3600);
    // Late metadata can enable editing without destroying the existing draft.
    radio_metadata.loaded=true;strcpy(radio_metadata.firmware_version,"2.7.18");draw();
    assert(telemetry_draft.enable_supported&&telemetry_draft.interval_secs==3600);

    ready();telemetry_settings_step(true);select_save();
    assert(telemetry_settings_pending&&telemetry_save.state==MESH_TELEMETRY_CONFIG_SAVING);
    writes=telemetry_config_op.write_id;draw();assert(shown("Verifying...")&&!shown("settings verified"));
    bool enabled=telemetry_draft.enabled;unsigned interval=telemetry_draft.interval_secs;
    telemetry_settings_field=0;telemetry_settings_step(true);telemetry_settings_field=1;telemetry_settings_step(true);select_save();
    assert(telemetry_draft.enabled==enabled&&telemetry_draft.interval_secs==interval&&telemetry_config_op.write_id==writes);
    ack(local_node_number,writes,0);draw();assert(telemetry_settings_pending&&!shown("settings verified"));
    // The backend alone grants VERIFIED; an ACK is insufficient.
    config(1800,true,true);telemetry_save.state=MESH_TELEMETRY_CONFIG_SAVED;draw();
    assert(!telemetry_settings_pending&&!telemetry_settings_edited&&telemetry_draft.enabled&&shown("settings verified"));

    const mesh_telemetry_config_state_t failures[]={MESH_TELEMETRY_CONFIG_TIMEOUT,MESH_TELEMETRY_CONFIG_FAILED,MESH_TELEMETRY_CONFIG_INTERRUPTED};
    const char *messages[]={"No verified reply","did not confirm","verification interrupted"};
    for(size_t i=0;i<3;i++) {
        ready();telemetry_settings_step(true);select_save();telemetry_save.state=failures[i];draw();
        assert(!telemetry_settings_pending&&telemetry_settings_edited&&shown(messages[i])&&!shown("settings verified"));
    }
    // A stale revision fails without an optimistic success or deferred mutation.
    ready();telemetry_settings_step(true);config(3600,false,false);writes=telemetry_config_op.write_id;select_save();
    assert(!telemetry_settings_pending&&telemetry_save.state==MESH_TELEMETRY_CONFIG_IDLE&&telemetry_config_op.write_id==writes);
    assert(strstr(telemetry_settings_feedback,"Not saved")&&telemetry_draft.enabled&&telemetry_draft.revision==telemetry_config.revision);
    // Opening the form discards unsaved state and reloads current radio values.
    open_telemetry_settings();assert(!telemetry_settings_loaded&&!telemetry_settings_edited&&!telemetry_settings_feedback[0]);draw();
    assert(!telemetry_draft.enabled&&telemetry_draft.interval_secs==3600&&telemetry_settings_field==0);
    assert(!message_count&&!reaction_count&&!mesh_radio_take_message_alerts());
    puts("PASS: actual telemetry form loading/support gates, draft-only navigation, interval wrap/custom reset, explicit Save/busy guard, stale revisions, verified-only success, failure states and reopen discard");
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "test.c").write_text(harness)
    env = dict(os.environ, DEVELOPER_DIR="/Library/Developer/CommandLineTools")
    subprocess.run(["clang", "-std=c11", "-fsanitize=address,undefined", "-I", str(root / "tests/stubs"), "-I", str(root / "main"), str(path / "test.c"), "-lm", "-o", str(path / "test")], check=True, env=env)
    subprocess.run([str(path / "test")], cwd=directory, check=True, env=env)
