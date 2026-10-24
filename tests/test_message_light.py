from ui_source import read_main_ui
"""Check real helpers against the CH32 manual/automatic/shared-brightness model.

Model: official tanmatsu-coprocessor-firmware/src/application/leds.c.
Message is pixel 2; manual mode ignores the internal message register.
"""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = read_main_ui(root)
a = source.index('static void update_message_light(void)')
b = source.index('\n#if defined(CONFIG_BSP_TARGET_KAMI)', a)
light = source[a:b]
a = source.index('static esp_err_t apply_backlight_levels(')
b = source.index('static int draw_message_timestamp', a)
toggle = source[a:b]
a = source.index('            if (display_and_lights_off) {', source.index('void app_main'))
b = source.index('            if (event.type == INPUT_EVENT_TYPE_KEYBOARD ||', a)
wake = source[a:b]
harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "display_settings.h"
#include "notification_time.h"
#define MESH_CHANNEL_LIMIT 8
static uint8_t channel_alerts_mask=0xff;
#include "power_settings.h"
#define CONFIG_BSP_TARGET_TANMATSU 1
#define ESP_OK 0
#define ESP_LOGW(...) ((void)0)
typedef int esp_err_t;
typedef void *tanmatsu_coprocessor_handle_t;
static int64_t now = 1000000;
static size_t channel_unread, direct_unread;
static bool display_and_lights_off, automatic = true;
static uint8_t screen = 80, rgb = 70, keyboard = 60;
static uint8_t saved_display_brightness = 80, saved_led_brightness = 70, saved_keyboard_brightness = 60;
static mesh_display_settings_t display_settings;
static mesh_power_settings_t power_settings;
static int applied_screen=-1, applied_keyboard=-1;
static unsigned backlight_writes;
static uint8_t active_display_brightness=80, active_keyboard_brightness=60;
static int64_t last_activity_us, screen_retry_at;
static int nvs_error;
static uint8_t stored[8];
static size_t stored_size;
static bool exists;
#include "nvs.h"
int nvs_open(const char *ns, int mode, nvs_handle_t *h) { assert(!strcmp(ns,"meshdeck_mui")); *h=1; return nvs_error ? nvs_error : !exists && !mode ? ESP_ERR_NVS_NOT_FOUND : 0; }
int nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *size) {
    assert(!strcmp(key,"display_v1"));
    if (stored_size > *size) return 123;
    memcpy(out,stored,stored_size); *size=stored_size; return 0;
}
int nvs_set_blob(nvs_handle_t h, const char *key, const void *value, size_t size) { assert(size==8); memcpy(stored,value,size); stored_size=size; return nvs_error; }
int nvs_commit(nvs_handle_t h) { exists=true; return nvs_error; }
void nvs_close(nvs_handle_t h) {}
static uint32_t staged[6], user_data[6], internal_data[6];
static int writes, fail_write;
static size_t mesh_radio_unread_channel(uint8_t channel){return channel==0?channel_unread:0;}
static size_t mesh_radio_unread_total(bool direct) { return direct ? direct_unread : channel_unread; }
static bool mesh_radio_alert_unread(bool direct,uint8_t channel){return direct ? direct_unread!=0 : channel==0&&channel_unread!=0;}
static int64_t esp_timer_get_time(void) { return now; }
static int bsp_led_set_mode(bool mode) { automatic = mode; return 0; }
static int bsp_led_set_pixel(unsigned index, uint32_t color) { assert(index < 6); staged[index] = color; return 0; }
static int bsp_led_send(void) {
    writes++;
    if (fail_write) return 1;
    memcpy(user_data, staged, sizeof(user_data)); return 0;
}
static int bsp_display_get_backlight_brightness(uint8_t *out) { *out = screen; return 0; }
static int bsp_led_get_brightness(uint8_t *out) { *out = rgb; return 0; }
static int bsp_input_get_backlight_brightness(uint8_t *out) { *out = keyboard; return 0; }
static int bsp_display_set_backlight_brightness(uint8_t b) { screen = b; return 0; }
static int bsp_led_set_brightness(uint8_t b) { rgb = b; return 0; }
static int bsp_input_set_backlight_brightness(uint8_t b) { keyboard = b; return 0; }
static int mesh_backlight_set(bool is_keyboard, uint8_t value) {
    backlight_writes++;
    return is_keyboard ? bsp_input_set_backlight_brightness(value) : bsp_display_set_backlight_brightness(value);
}
// Former register API affects only the internal buffer, ignored in manual mode.
static int bsp_tanmatsu_coprocessor_get_handle(void **out) { *out = (void *)1; return 0; }
static int tanmatsu_coprocessor_set_message(void *h, bool r, bool g, bool b, bool rb, bool gb, bool bb, bool f, bool fh) {
    internal_data[2] = (r ? 0xff0000 : 0) | (g ? 0xff00 : 0) | (b ? 0xff : 0); return 0;
}
static uint32_t visible(unsigned index) {
    uint32_t c = automatic && index < 4 ? internal_data[index] : user_data[index];
    return ((((c >> 16) & 255) * rgb / 100) << 16) |
           ((((c >> 8) & 255) * rgb / 100) << 8) | ((c & 255) * rgb / 100);
}
static void only_blue(void) {
    assert(visible(2) && !(visible(2) & 0xffff00));
    for (unsigned i = 0; i < 6; i++) if (i != 2) assert(!visible(i));
    assert(!screen && !keyboard && display_and_lights_off);
}
'''+light+toggle+r'''
#define INPUT_EVENT_TYPE_NAVIGATION 1
#define INPUT_EVENT_TYPE_KEYBOARD 2
#define BSP_INPUT_NAVIGATION_KEY_F4 4
#define BSP_INPUT_NAVIGATION_KEY_F6 6
typedef struct { int type; struct { int key; bool state; } args_navigation; } test_event_t;
static unsigned draws;
static void draw_screen(void) { draws++; }
static void handle_sleep_input(test_event_t event) {
    for(int once=0; once<1; once++) {
'''+wake+r'''
    }
}
''' + r'''
int main(void) {
    display_settings = mesh_display_defaults(80,60);
    power_settings = mesh_power_defaults();
    // Reproduce v13: internal register plus manual mode plus brightness zero.
    automatic = false; rgb = 0;
    tanmatsu_coprocessor_set_message((void *)1, false, false, true, false, false, true, false, false);
    assert(!visible(2));
    rgb = 70; assert(!visible(2));
    update_message_light();
    assert(!automatic && !visible(2) && writes == 1 && rgb == 70);
    assert(visible(0) && visible(1) && visible(3));
    update_message_light(); assert(writes == 1);
    toggle_display_and_lights();
    assert(display_and_lights_off && !screen && !rgb && !keyboard);
    for (int i = 0; i < 6; i++) assert(!visible(i));
    channel_unread = 1; update_message_light(); only_blue(); assert(rgb == 20);
    // A per-channel mute changes only the light, including while dark.
    channel_alerts_mask = 0xfe; update_message_light();
    assert(channel_unread == 1 && !visible(2) && !rgb && !screen && !keyboard);
    direct_unread = 1; update_message_light(); only_blue();
    assert(channel_unread == 1); // DMs remain independent of the channel mask.
    direct_unread = 0; channel_alerts_mask = 0xff; update_message_light(); only_blue();
    int count = writes;
    for (int i = 0; i < 100; i++) update_message_light(); assert(writes == count);
    toggle_display_and_lights();
    assert(!display_and_lights_off && screen == 80 && rgb == 70 && keyboard == 60);
    assert(visible(2) && visible(0) && visible(1));
    channel_alerts_mask = 0xfe; update_message_light();
    assert(channel_unread == 1 && !visible(2) && screen == 80);
    channel_alerts_mask = 0xff; update_message_light(); assert(visible(2));
    channel_unread = 0; update_message_light(); assert(!visible(2));
    direct_unread = 1; update_message_light(); assert(visible(2));
    toggle_display_and_lights(); only_blue();
    direct_unread = 0; update_message_light(); assert(!visible(2) && !rgb && !screen && !keyboard);
    direct_unread = 1; fail_write = 1; update_message_light(); assert(!visible(2));
    count = writes;
    for (int i = 0; i < 100; i++) update_message_light(); assert(writes == count);
    fail_write = 0; now += 5000000; update_message_light(); only_blue();
    direct_unread = 0; update_message_light(); assert(!visible(2) && !rgb);
    // Filtering changes physical output immediately while dark, without
    // changing unread counts or needing another incoming packet.
    display_settings.alert_sources = MESH_ALERT_CHANNEL;
    direct_unread = 1; update_message_light(); assert(!visible(2));
    channel_unread = 1; update_message_light(); only_blue();
    display_settings.night_brightness = 45; update_message_light(); only_blue(); assert(rgb == 45);
    display_settings.unread_light = 0; update_message_light(); assert(!rgb && !visible(2));
    display_settings.unread_light = 1; display_settings.alert_sources = MESH_ALERT_DIRECT;
    direct_unread = 0; update_message_light(); assert(!visible(2));
    direct_unread = 1; update_message_light(); only_blue();
    display_settings.alert_sources = MESH_ALERT_BOTH;
    channel_unread = direct_unread = 0; update_message_light();
    // New state bypasses a previous state's retry delay.
    toggle_display_and_lights();
    direct_unread = 1; fail_write = 1; update_message_light();
    fail_write = 0; toggle_display_and_lights(); only_blue();
    // User input blocks the timer, radio arrivals do not reset it. Wake starts
    // a fresh interval; keyboard brightness zero must remain zero on wake.
    toggle_display_and_lights();
    display_settings.timeout_seconds = 60;
    saved_keyboard_brightness = active_keyboard_brightness = 0;
    now = last_activity_us + 59999999; check_screen_timeout(now, false); assert(!display_and_lights_off);
    now++; check_screen_timeout(now, true); assert(!display_and_lights_off);
    check_screen_timeout(now, false); assert(display_and_lights_off);
    direct_unread++; update_message_light(); only_blue();
    handle_sleep_input((test_event_t){INPUT_EVENT_TYPE_NAVIGATION,{BSP_INPUT_NAVIGATION_KEY_F4,true}});
    assert(display_and_lights_off && !draws);
    handle_sleep_input((test_event_t){INPUT_EVENT_TYPE_NAVIGATION,{BSP_INPUT_NAVIGATION_KEY_F6,false}});
    assert(display_and_lights_off && !draws);
    handle_sleep_input((test_event_t){INPUT_EVENT_TYPE_KEYBOARD,{0,true}});
    assert(display_and_lights_off && !draws);
    handle_sleep_input((test_event_t){INPUT_EVENT_TYPE_NAVIGATION,{BSP_INPUT_NAVIGATION_KEY_F6,true}});
    assert(!display_and_lights_off && screen==80 && keyboard==0 && draws==1);
    check_screen_timeout(now, false); assert(!display_and_lights_off);
    display_settings.timeout_seconds = 0; now += 9999999999LL;
    check_screen_timeout(now, false); assert(!display_and_lights_off);
    // All decorative pixels, including A/B, can be dark while unread blue
    // stays independent. Changes take effect without a new incoming packet.
    power_settings.decorative_lights=0; direct_unread=1; update_message_light();
    assert(visible(2)); for(unsigned i=0;i<6;i++) if(i!=2) assert(!visible(i));
    direct_unread=channel_unread=0; update_message_light();
    for(unsigned i=0;i<6;i++) assert(!visible(i));
    power_settings.decorative_lights=1; update_message_light(); assert(visible(0) && visible(4) && visible(5));
    // Idle policies cache writes, restore from ordinary activity, and do not
    // turn a fully blank screen on for queued keys.
    power_settings.keyboard_timeout_seconds=power_settings.dim_timeout_seconds=15;
    active_keyboard_brightness=60; last_activity_us=now;
    update_idle_backlights(now,false); assert(screen==80 && keyboard==60);
    now+=15000000; update_idle_backlights(now,false); assert(screen==20 && keyboard==0);
    unsigned calls=backlight_writes;
    for(int i=0;i<100;i++) update_idle_backlights(now,false);
    assert(backlight_writes==calls);
    update_idle_backlights(now,true); assert(screen==80 && keyboard==60);
    last_activity_us=now; toggle_display_and_lights();
    update_idle_backlights(now,true); assert(!screen && !keyboard && display_and_lights_off);
    toggle_display_and_lights(); assert(screen==80 && keyboard==60);
}
'''
with tempfile.TemporaryDirectory(prefix='meshdeck-message-light-') as folder:
    path = Path(folder)
    (path / 'test.c').write_text(harness)
    subprocess.run(['clang', '-std=c11', '-fsanitize=address,undefined', '-I', str(root/'main'), '-I', str(root/'tests/stubs'), str(root/'main/display_settings.c'), str(root/'main/notification_time.c'), str(root/'main/power_settings.c'), str(root/'main/backlight_curve.c'), '-lm', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: filtering, disabled light, night brightness, idle timeout and wake timer reset; reproduces manual-mode/zero-brightness failure; isolated blue while screen/keyboard off, channel+DM unread, wake restore, read clearing and retry behavior')
