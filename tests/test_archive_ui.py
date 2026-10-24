"""Exercise production archive review, search, page/detail and back controls."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'main/main.c').read_text()
enum=s[s.index('typedef enum {'):s.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
h='#define main archive_storage_tests\n#include "'+str(r/'tests/test_message_archive.c')+'"\n#undef main\n#include "composer_view.c"\n'+enum+r'''
#define BSP_INPUT_NAVIGATION_KEY_UP 1
#define BSP_INPUT_NAVIGATION_KEY_DOWN 2
#define BSP_INPUT_NAVIGATION_KEY_LEFT 3
#define BSP_INPUT_NAVIGATION_KEY_RIGHT 4
#define BSP_INPUT_NAVIGATION_KEY_RETURN 5
#define BSP_INPUT_NAVIGATION_KEY_ESC 6
#define BSP_INPUT_NAVIGATION_KEY_F1 7
#define BSP_INPUT_NAVIGATION_KEY_F2 8
#define BSP_INPUT_NAVIGATION_KEY_F3 9
#define BSP_INPUT_NAVIGATION_KEY_F4 10
#define BSP_INPUT_NAVIGATION_KEY_F5 11
#define BSP_INPUT_NAVIGATION_KEY_F6 12
static meshdeck_screen_t current_screen=SCREEN_HOME;
static uint32_t message_peer=42;static uint8_t message_channel=1;static unsigned backs;
static int64_t esp_timer_get_time(void){return now;}
static void go_back(void){backs++;current_screen=SCREEN_DISPLAY_SETTINGS_MENU;}
#include "archive_ui.inc"
static void nav(int key){assert(archive_navigation(key));}
static void enter(void){nav(BSP_INPUT_NAVIGATION_KEY_RETURN);}
static void tick(void){for(unsigned i=0;i<3;i++){now+=1000000;archive_ui_poll(now);}}
int main(void){
 mesh_archive_init();tick();open_archive_settings();assert(!archive_enabled_draft&&!archive_review);
 nav(BSP_INPUT_NAVIGATION_KEY_RIGHT);archive_field=2;enter();assert(archive_review&&!archive_confirm&&!ar_status.enabled);enter();assert(!archive_review&&!ar_status.enabled);
 enter();nav(BSP_INPUT_NAVIGATION_KEY_RIGHT);enter();tick();assert(ar_status.enabled&&!archive_review);
 for(unsigned i=1;i<=22;i++)capture(i,i%2?42:0,i%2?1:2);tick();assert(ar_status.count==22);
 archive_field=3;enter();assert(current_screen==SCREEN_ARCHIVE_BROWSER&&archive_all);tick();assert(ar_page.ready&&ar_page.matches==22);
 nav(BSP_INPUT_NAVIGATION_KEY_RIGHT);tick();assert(ar_page.page==1&&ar_page.count==6);nav(BSP_INPUT_NAVIGATION_KEY_DOWN);assert(archive_selection==1);enter();assert(archive_detail&&!archive_detail_scroll);nav(BSP_INPUT_NAVIGATION_KEY_DOWN);nav(BSP_INPUT_NAVIGATION_KEY_DOWN);nav(BSP_INPUT_NAVIGATION_KEY_UP);assert(archive_detail_scroll==1);nav(BSP_INPUT_NAVIGATION_KEY_ESC);assert(!archive_detail&&!backs);
 archive_key('\t');tick();assert(!archive_all&&ar_page.matches==11);assert(archive_key('2'));tick();assert(ar_page.matches==2); // DM text 21 and sender !11223344 both searched: sender matches all instead!
 assert(!archive_navigation(BSP_INPUT_NAVIGATION_KEY_F5));assert(!archive_navigation(BSP_INPUT_NAVIGATION_KEY_F1));
 archive_query[0]=0;archive_key('z');tick();assert(!ar_page.matches&&!archive_detail);enter();assert(!archive_detail);
 nav(BSP_INPUT_NAVIGATION_KEY_ESC);assert(backs==1&&current_screen==SCREEN_DISPLAY_SETTINGS_MENU);
 open_archive_settings();archive_field=4;enter();assert(archive_review&&archive_clear_review&&!archive_confirm);enter();assert(ar_status.count==22);
 enter();nav(BSP_INPUT_NAVIGATION_KEY_RIGHT);enter();tick();assert(!ar_status.count&&!ar_status.pending);
 open_archive_settings();archive_field=1;nav(BSP_INPUT_NAVIGATION_KEY_LEFT);assert(archive_limit_draft==2000);archive_field=2;enter();assert(archive_review&&!archive_confirm);nav(BSP_INPUT_NAVIGATION_KEY_ESC);assert(ar_status.limit==5000);
 current_screen=SCREEN_HOME;assert(!archive_key('x')&&!archive_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN));
 puts("PASS: actual archive enable/lower-limit/clear reviews default Cancel; scope/search, newest page navigation, detail/back, no transmit/read side effects and global shortcut passthrough");
}
'''
# Query a text phrase instead of a digit also present in every sender ID.
h=h.replace("assert(archive_key('2'));tick();assert(ar_page.matches==2); // DM text 21 and sender !11223344 both searched: sender matches all instead!", "const char *q=\"Message 2\";while(*q)archive_key(*q++);tick();assert(ar_page.matches==1);")
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(r/'tests/stubs'),'-I',str(r/'main'),str(p/'test.c'),str(r/'main/emoji_text.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
