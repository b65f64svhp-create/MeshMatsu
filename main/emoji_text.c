#include "emoji_text.h"
#include <stdio.h>
#include <string.h>

#include "emoji_glyphs.inc"
#define GLYPH_COUNT (sizeof(emoji_glyphs) / sizeof(emoji_glyphs[0]))

static size_t utf8_scalar(const char *text, uint32_t *cp) {
    const uint8_t *s = (const uint8_t *)text;
    if (!s[0]) { *cp = 0; return 0; }
    if (s[0] < 128) { *cp = s[0]; return 1; }
    unsigned n = s[0] >= 0xc2 && s[0] <= 0xdf ? 2 : s[0] >= 0xe0 && s[0] <= 0xef ? 3 : s[0] >= 0xf0 && s[0] <= 0xf4 ? 4 : 0;
    uint32_t value = n ? s[0] & ((1u << (7 - n)) - 1) : 0;
    for (unsigned i = 1; i < n; i++) {
        if (!s[i] || (s[i] & 0xc0) != 0x80) { n = 0; break; }
        value = (value << 6) | (s[i] & 63);
    }
    if (!n || (n == 2 && value < 0x80) || (n == 3 && value < 0x800) || (n == 4 && value < 0x10000) ||
        value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) { *cp = 0xfffd; return 1; }
    *cp = value;
    return n;
}

static size_t glyph_lower_bound(const char *key) {
    size_t lo = 0, hi = GLYPH_COUNT;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (strcmp(emoji_glyphs[mid].text, key) < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

static mesh_text_token_t fit_token(mesh_text_token_t token, int width, int advance) {
    if (token.width > width) {
        token.glyph = -1;
        strcpy(token.text, width >= advance * 3 ? "[?]" : "?");
        token.width = (int)strlen(token.text) * advance;
    }
    return token;
}

mesh_text_token_t mesh_text_next(const char *text, int advance) {
    mesh_text_token_t token = {.glyph = -1};
    if (!text || !text[0]) return token;
    uint32_t first;
    size_t first_size = utf8_scalar(text, &first);
    token.bytes = first_size;
    token.space = first == ' ' || first == '\t';
    token.newline = first == '\n' || first == '\r';
    token.width = token.newline ? 0 : advance;
    // Ordinary keyboard text cannot begin an emoji, apart from keycap keys.
    // Avoid searching thousands of glyphs for every ASCII letter in a draft.
    if (first < 128 && first != '#' && first != '*' && !(first >= '0' && first <= '9')) {
        if (token.space) strcpy(token.text, " ");
        else if (!token.newline && first >= 32 && first != 127) { token.text[0] = (char)first; token.text[1] = 0; }
        else if (!token.newline) {
            snprintf(token.text, sizeof(token.text), "[U+%04lX]", (unsigned long)first);
            token.width = (int)strlen(token.text) * advance;
        }
        return token;
    }
    // Longest matching emoji sequence; ignore optional emoji presentation VS16.
    char key[80] = {0};
    size_t used = 0, pos = 0;
    while (text[pos]) {
        uint32_t cp;
        size_t n = utf8_scalar(text + pos, &cp);
        if (cp == 0xfe0f && pos) {
            pos += n;
            if (token.glyph >= 0) token.bytes = pos;
            continue;
        }
        if (used + n >= sizeof(key)) break;
        memcpy(key + used, text + pos, n); used += n; key[used] = 0;
        size_t index = glyph_lower_bound(key);
        if (index >= GLYPH_COUNT || strncmp(emoji_glyphs[index].text, key, used)) break;
        pos += n;
        if (!strcmp(emoji_glyphs[index].text, key)) {
            token.glyph = (int)index;
            token.bytes = pos;
            token.width = MESH_EMOJI_SIZE + 2;
        }
    }
    if (token.glyph >= 0) { token.space = token.newline = false; return token; }
    // Phone keyboards send typographic punctuation outside Sky Mono's Latin-1
    // font. Use readable font equivalents without changing the original UTF-8
    // bytes in history, drafts, or radio payloads. Keep emoji matching first.
    const char *punctuation = NULL;
    if (first >= 0x2018 && first <= 0x201b) punctuation = "'";
    else if (first >= 0x201c && first <= 0x201f) punctuation = "\"";
    else if ((first >= 0x2010 && first <= 0x2015) || first == 0x2212) punctuation = "-";
    else if (first == 0x2026) punctuation = "...";
    else if (first == 0x00a0 || first == 0x202f) { punctuation = " "; token.space = true; }
    if (punctuation) {
        strcpy(token.text, punctuation);
        token.width = (int)strlen(token.text) * advance;
        return token;
    }
    if (first < 0x100 && first >= 32 && first != 127) {
        memcpy(token.text, text, first_size); token.text[first_size] = 0;
    } else if (token.space) strcpy(token.text, " ");
    else if (!token.newline) {
        snprintf(token.text, sizeof(token.text), "[U+%04lX]", (unsigned long)first);
        token.width = (int)strlen(token.text) * advance;
    }
    return token;
}

uint8_t mesh_emoji_alpha(int glyph, unsigned x, unsigned y) {
    if (glyph < 0 || (size_t)glyph >= GLYPH_COUNT || x >= 20 || y >= 20) return 0;
    unsigned pixel = y * 20 + x;
    uint8_t packed = emoji_pixels[emoji_glyphs[glyph].offset + pixel / 4];
    return ((packed >> ((3 - pixel % 4) * 2)) & 3) * 85;
}

int mesh_text_layout(const char *text, int width, int advance, int line_height, int max_lines,
                     mesh_text_draw_fn draw, void *context) {
    if (!text || width < 1 || advance < 1 || max_lines < 1) return 0;
    size_t offset = 0;
    int lines = 0;
    do {
        size_t pos = offset, end = offset, next = offset, last_space = SIZE_MAX;
        int x = 0;
        while (text[pos]) {
            mesh_text_token_t token = fit_token(mesh_text_next(text + pos, advance), width, advance);
            if (token.newline) { end = pos; next = pos + token.bytes; break; }
            if (x + token.width > width && pos > offset) {
                end = next = pos;
                if (last_space != SIZE_MAX && last_space > offset) end = next = last_space;
                break;
            }
            if (token.space) last_space = pos;
            x += token.width; pos += token.bytes;
            end = next = pos;
        }
        if (draw) {
            x = 0;
            for (pos = offset; pos < end;) {
                mesh_text_token_t token = fit_token(mesh_text_next(text + pos, advance), width, advance);
                draw(&token, x, lines * line_height, context);
                x += token.width; pos += token.bytes;
            }
        }
        offset = next;
        while (text[offset] == ' ' || text[offset] == '\t') offset++;
        lines++;
    } while (text[offset] && lines < max_lines);
    return lines;
}

int mesh_text_line_count(const char *text, int width, int advance) {
    return mesh_text_layout(text, width, advance, 1, 1024, NULL, NULL);
}
