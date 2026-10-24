from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=r'''#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "composer_view.h"
enum {SCREEN_HOME,SCREEN_NODES,NODE_NICKNAME=5};
static int current_screen=SCREEN_NODES,node_modal=NODE_NICKNAME;
static bool nickname_replace=true;
static char nickname_draft[40]="Camp North";
'''+ '#include "'+str(r/'main/node_nickname_ui.inc')+'"\n'+r'''
int main(void){
 assert(node_nickname_key('A')&&!strcmp(nickname_draft,"A")&&!nickname_replace);
 assert(node_nickname_key('B')&&!strcmp(nickname_draft,"AB"));node_nickname_key(127);assert(!strcmp(nickname_draft,"A"));
 strcpy(nickname_draft,"Camp North");nickname_replace=true;node_nickname_key('\b');assert(!nickname_draft[0]);
 strcpy(nickname_draft,"Caf\xc3\xa9");nickname_replace=false;node_nickname_key('\b');assert(!strcmp(nickname_draft,"Caf"));
 memset(nickname_draft,'A',39);nickname_draft[39]=0;node_nickname_key('B');assert(strlen(nickname_draft)==39);
 assert(!node_nickname_key('\r'));current_screen=SCREEN_HOME;assert(!node_nickname_key('X'));assert(strlen(nickname_draft)==39);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(s)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(r/'main'),str(p/'test.c'),str(r/'main/composer_view.c'),str(r/'main/emoji_text.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: actual nickname editor selected-text replacement/clear, single UTF-8 delete, byte bound, Enter handoff and offscreen guard')
