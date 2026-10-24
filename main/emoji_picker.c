#include "emoji_picker.h"
#include "radio_link.h"
#include <string.h>

static const mesh_picker_emoji_t choices[] = {
    {"👍", "Thumbs up"}, {"👎", "Thumbs down"}, {"❤️", "Heart"}, {"😂", "Laughing"},
    {"😀", "Smile"}, {"😊", "Happy"}, {"😉", "Wink"}, {"😎", "Cool"},
    {"🎉", "Celebrate"}, {"🔥", "Fire"}, {"💯", "One hundred"}, {"✅", "Check"},
    {"❓", "Question"}, {"❗", "Exclamation"}, {"🙏", "Thanks"}, {"👏", "Applause"},
    {"👋", "Wave"}, {"👌", "OK"}, {"💪", "Strong"}, {"🤔", "Thinking"},
    {"😢", "Sad"}, {"😭", "Crying"}, {"😮", "Surprised"}, {"😴", "Sleeping"},
    {"🚀", "Rocket"}, {"🏠", "Home"}, {"📍", "Location"}, {"🚗", "Car"},
    {"☀️", "Sun"}, {"🌧️", "Rain"}, {"☕", "Coffee"}, {"🇨🇦", "Canada"},
};
size_t mesh_picker_count(void) { return sizeof(choices) / sizeof(choices[0]); }
const mesh_picker_emoji_t *mesh_picker_get(size_t index) { return index < mesh_picker_count() ? &choices[index] : NULL; }
bool mesh_picker_insert(char *draft, size_t capacity, size_t index) {
    const mesh_picker_emoji_t *emoji = mesh_picker_get(index);
    if (!draft || !capacity || !emoji) return false;
    size_t used = 0;
    while (used < capacity && draft[used]) used++;
    size_t extra = strlen(emoji->text);
    if (used == capacity || used + extra > MESH_TEXT_MAX_LENGTH || extra >= capacity - used) return false;
    memcpy(draft + used, emoji->text, extra + 1);
    return true;
}
