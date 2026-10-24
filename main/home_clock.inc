// Use the existing device clock and time preferences. No new timer or radio query.
static int64_t home_clock_drawn_minute = INT64_MIN;
static int64_t home_clock_minute(time_t now) {
    return now >= (time_t)1700000000 && (uint64_t)now <= UINT32_MAX ? (int64_t)now / 60 : -1;
}
static bool home_clock_needs_draw(void) {
    return home_clock_minute(time(NULL)) != home_clock_drawn_minute;
}
static void draw_home_clock(void) {
    time_t now = time(NULL);
    home_clock_drawn_minute = home_clock_minute(now);
    char clock[16], date[24];
    mesh_time_clock(&time_settings, home_clock_drawn_minute < 0 ? 0 : (uint32_t)now,
                    clock, sizeof(clock), date, sizeof(date));
    // Logo ends at x=284; reserve the right side for radio/battery/version.
    int left = 304, right = (int)logical_h_res - 220;
    if (right - left < 100) return;
    float center = (left + right) / 2.0f;
    float clock_width = pax_text_size(pax_font_sky_mono, 28, clock).x;
    float date_width = pax_text_size(pax_font_sky_mono, 11, date).x;
    pax_draw_text(&fb, UI_GREEN, pax_font_sky_mono, 28, center - clock_width / 2, 16, clock);
    pax_draw_text(&fb, UI_MUTED, pax_font_sky_mono, 11, center - date_width / 2, 54, date);
}
