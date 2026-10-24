from ui_source import read_main_ui
"""Actual game controls, score persistence/retry and audio; no radio/display."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];s=read_main_ui(root);enum=s[s.index('typedef enum {'):s.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
ui=(root/'main/packet_runner_ui.inc').read_text().split('#include "packet_runner_draw.inc"')[0]
source='#define main score_tests\n#include "'+str(root/'tests/test_packet_scores.c')+'"\n#undef main\n'+'''
#include "sound_player.h"
static unsigned sounds;
static mesh_sound_event_t last_sound;
static bool mesh_sound_play_stub(mesh_sound_event_t event){sounds++;last_sound=event;return true;}
#define mesh_sound_play mesh_sound_play_stub
'''+enum+'''
static meshdeck_screen_t current_screen=SCREEN_ABOUT;
enum {BSP_INPUT_NAVIGATION_KEY_LEFT=1,BSP_INPUT_NAVIGATION_KEY_RIGHT,BSP_INPUT_NAVIGATION_KEY_UP,BSP_INPUT_NAVIGATION_KEY_DOWN,BSP_INPUT_NAVIGATION_KEY_RETURN,BSP_INPUT_NAVIGATION_KEY_ESC};
'''+ui+r'''
static void enter_game(void){current_screen=SCREEN_ABOUT;const char *secret="PACKET";for(int i=0;i<6;i++)assert(packet_key(secret[i])==(i==5));assert(current_screen==SCREEN_PACKET_RUNNER);}
static void finish_route(unsigned previous_moves,unsigned previous_drops){packet_game=(packet_runner_t){.x=15,.y=5,.relays=3,.moves=previous_moves,.route_moves=previous_moves,.drops=previous_drops,.route_drops=previous_drops};assert(packet_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT)&&packet_game.delivered);}
int main(void){
 enter_game();assert(!packet_game.moves&&packet_scores_ready&&!writes);
 assert(packet_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT)&&packet_game.shield&&sounds==1&&last_sound==MESH_SOUND_GAME_RELAY);
 assert(packet_key('R')&&!packet_game.x&&packet_game.moves==1&&packet_game.drops==1&&sounds==1);
 assert(packet_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN)&&!packet_game.level);
 packet_key('H');assert(packet_scores_open);packet_runner_t before=packet_game;
 packet_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);packet_key(' ');packet_key('R');assert(!memcmp(&before,&packet_game,sizeof(before)));
 assert(packet_navigation(BSP_INPUT_NAVIGATION_KEY_ESC)&&!packet_scores_open);assert(!packet_navigation(BSP_INPUT_NAVIGATION_KEY_ESC));
 packet_game=(packet_runner_t){.x=1,.y=3};packet_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(last_sound==MESH_SOUND_GAME_RELAY);unsigned played=sounds;packet_key(' ');assert(sounds==played); // waiting on an already-collected relay is quiet
 packet_game=(packet_runner_t){.x=7,.y=1,.shield=true};packet_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(packet_game.shielded&&last_sound==MESH_SOUND_GAME_DROP);packet_key(' ');assert(packet_game.dropped&&last_sound==MESH_SOUND_GAME_DROP);
 commit_failure=7;finish_route(19,0);assert(last_sound==MESH_SOUND_GAME_DELIVERED&&packet_new_best&&packet_scores_dirty&&packet_scores.route[0].moves==20&&!saved_size&&writes==1);
 packet_key('h');assert(packet_scores_open&&packet_scores_dirty);commit_failure=0;packet_key('S');assert(!packet_scores_dirty&&saved_size==44);unsigned written=writes;
 packet_key('h');packet_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(packet_game.level==1&&!packet_game.route_moves);packet_key('R');assert(!packet_game.route_drops&&writes==written);
 packet_scores=(packet_scores_t){0};packet_scores_ready=packet_scores_dirty=false;enter_game();assert(packet_scores.route[0].moves==20&&!packet_scores_dirty);written=writes;finish_route(25,1);assert(!packet_new_best&&!packet_scores_dirty&&writes==written); // poorer result never writes
 // A failed initial read must not overwrite a better stored record; retry merges it first.
 packet_scores=(packet_scores_t){0};packet_scores_ready=false;read_failure=7;enter_game();assert(!packet_scores_ready);written=writes;finish_route(59,1);assert(packet_scores_dirty&&!packet_scores_ready&&writes==written&&strstr(packet_scores_feedback,"Cannot read"));read_failure=0;packet_key('H');assert(packet_scores_ready&&!packet_scores_dirty&&packet_scores.route[0].moves==20&&!packet_scores.route[0].drops);
 packet_scores_t loaded;assert(!packet_scores_load(&loaded)&&loaded.route[0].moves==20&&!loaded.route[0].drops);
 packet_key('h');assert(!packet_scores_open);packet_key('\r');assert(packet_game.level==1);
 assert(!packet_navigation(99));current_screen=SCREEN_HOME;assert(!packet_key('p')&&!packet_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT));
 current_screen=SCREEN_ABOUT;assert(!packet_key('p'));assert(!packet_key('x'));assert(!packet_key('a'));assert(current_screen==SCREEN_ABOUT);
 puts("PASS: hidden entry, chaser/shield gameplay controls, event-only audio, records modal freezes play/keeps global navigation, no per-turn writes, delivery-only saved records, retry/merge failures and reload persistence");
}
'''
a=s.index('if (enter && chat_action_enter_us');assert a<s.index('if (packet_key(key))',a)<s.index('if (home_key(key))',a)
assert s.index('if (packet_navigation(event.args_navigation.key))')<s.index('if (map_navigation(event.args_navigation.key))')
assert 'if (current_screen != SCREEN_ABOUT) packet_secret_progress = 0;' in s
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(source)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'main'),'-I',str(root/'tests/stubs'),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],cwd=d,check=True)
