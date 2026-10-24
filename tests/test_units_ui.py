from ui_source import read_main_ui
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];main=read_main_ui(r);enum=main[main.index('typedef enum {'):main.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
ui=(r/'main/units_ui.inc').read_text().split('static void draw_units_settings')[0]
h='#define main earlier_tests\n#include "'+str(r/'tests/test_units.c')+'"\n#undef main\n'+enum+'''
static meshdeck_screen_t current_screen=SCREEN_HOME;
static uint8_t distance_units,temperature_units;
enum {BSP_INPUT_NAVIGATION_KEY_UP=1,BSP_INPUT_NAVIGATION_KEY_DOWN,BSP_INPUT_NAVIGATION_KEY_LEFT,BSP_INPUT_NAVIGATION_KEY_RIGHT,BSP_INPUT_NAVIGATION_KEY_RETURN,BSP_INPUT_NAVIGATION_KEY_ESC};
'''+ui+'''
int main(void){open_units_settings();assert(current_screen==SCREEN_UNITS&&!units_draft&&!units_field);
 units_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(units_draft==1&&!distance_units);assert(!units_navigation(BSP_INPUT_NAVIGATION_KEY_ESC));open_units_settings();assert(!units_draft);
 units_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);units_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);units_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(temperature_draft==1&&!temperature_units);units_navigation(BSP_INPUT_NAVIGATION_KEY_DOWN);failure=7;units_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(!distance_units&&strstr(units_feedback,"Could not save"));failure=0;units_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(distance_units==1&&strstr(units_feedback,"saved"));open_units_settings();assert(units_draft==1);
 current_screen=SCREEN_HOME;assert(!units_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN));
}
'''
assert 'mesh_units_distance(distance_units,meters' in main
assert 'mesh_units_distance(distance_units,meters' in (r/'main/map_ui.inc').read_text()
assert 'mesh_units_precision(distance_units,channel_precision)' in main
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(r/'tests/stubs'),'-I',str(r/'main'),str(p/'test.c'),'-lm','-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],cwd=d,check=True)
