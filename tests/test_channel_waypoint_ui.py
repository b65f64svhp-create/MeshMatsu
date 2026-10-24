from ui_source import read_main_ui
"""Exercise the actual review flows, with fake UART and real channel/waypoint backend."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];main=read_main_ui(root)
enum=main[main.index('typedef enum {'):main.index('} meshdeck_screen_t;')+len('} meshdeck_screen_t;')]
keys=['UP','DOWN','LEFT','RIGHT','RETURN','ESC','F1','F2','F3','F4','F5','F6']
h='#define main earlier_tests\n#include "'+str(root/'tests/test_direct_messages.c')+'"\n#undef main\n'
h+='#include "'+str(root/'main/map_view.c')+'"\n#include "'+str(root/'main/third_party/qrcodegen.h')+'"\n'+enum+'\n'
h+=''.join(f'#define BSP_INPUT_NAVIGATION_KEY_{k} {i+1}\n' for i,k in enumerate(keys))
h+=r'''
static meshdeck_screen_t current_screen=SCREEN_CHANNEL_EDIT;
static size_t channel_selection=1,channel_field=4;
static uint8_t message_channel;
static mesh_map_view_t map_view;
static struct { bool waypoints_hidden; } map_filters;
static uint32_t map_waypoint_id,map_waypoint_peer,map_selected_node;
static uint8_t map_waypoint_channel;
static void setup(void){reset();session_passkey_size=4;memcpy(session_passkey,"test",4);session_passkey_received_us=fake_time;
 for(uint8_t i=0;i<8;i++){uint8_t ch[]={8,i,0x12,7,0x12,1,1,0x1a,2,'B','L',24,i?0:1};decode_channel(ch,sizeof(ch));}
 // Enable a secondary, so share and key actions use a real baseline.
 channels[1].info.role=2;channels[1].raw[channels[1].size-1]=2;
 mesh_map_center(&map_view,55.17,-118.8,10);
}
'''
h+='#include "'+str(root/'main/channel_tools_ui.inc')+'"\n#include "'+str(root/'main/waypoint_ui.inc')+'"\n'
h+=r'''
int main(void){
 setup();size_t writes=write_count;
 assert(channel_tools_key('\r')&&current_screen==SCREEN_CHANNEL_TOOLS&&ct_mode==CT_MENU);writes=write_count;
 ct_selection=0;channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(ct_mode==CT_QR&&ct_qr_valid&&write_count==writes);
 assert(qrcodegen_getSize(ct_qr)>=21);char link[MESH_CHANNEL_URL_MAX];strcpy(link,ct_url);
 // Losing the snapshot must hide the old QR, then regenerate even for the same URL.
 channels[1].info.loaded=false;fake_time+=4000000;ct_refresh_share();
 assert(!ct_qr_valid&&strstr(ct_share_status,"Reading channel 1")&&write_count==writes+1);
 writes=write_count;ct_refresh_share();assert(write_count==writes); // No per-frame radio spam.
 fake_time+=3000001;ct_refresh_share();assert(write_count==writes+1);writes=write_count;
 channels[1].info.loaded=true;ct_refresh_share();
 assert(ct_qr_valid&&!strcmp(ct_url,link)&&!ct_share_status[0]&&write_count==writes);
 // A disabled slot stays selected, explains how to enable/import, and never writes.
 ct_slot=2;ct_refresh_share();assert(!ct_qr_valid&&ct_slot==2&&strstr(ct_share_status,"disabled")&&strstr(ct_share_status,"Change Channel Role")&&write_count==writes);
 ct_slot=1;ct_refresh_share();assert(ct_qr_valid&&!strcmp(ct_url,link)&&write_count==writes);
 // A changed key must replace the URL/QR rather than leave the old invitation visible.
 uint8_t changed[]={8,1,0x12,7,0x12,1,2,0x1a,2,'B','L',24,2};decode_channel(changed,sizeof(changed));
 ct_refresh_share();assert(ct_qr_valid&&strcmp(ct_url,link)&&write_count==writes);
 changed[6]=1;decode_channel(changed,sizeof(changed));ct_refresh_share();assert(ct_qr_valid&&!strcmp(ct_url,link));
 channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_ESC);ct_selection=3;channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(ct_mode==CT_KEY&&mesh_radio_channel_key_valid(ct_key)&&write_count==writes);
 channel_tools_key(' ');assert(ct_show_key);channel_tools_key(' ');assert(!ct_show_key);
 channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(ct_mode==CT_CONFIRM&&!ct_confirm&&write_count==writes);
 channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(ct_mode==CT_MENU&&write_count==writes); // Cancel first
 ct_selection=3;channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
 channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);
 assert(ct_mode==CT_WAIT&&channel_operation.state==MESH_FAVORITE_SAVING&&write_count==writes+1&&!ct_key[0]);
 writes=write_count;channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_RETURN);assert(write_count==writes&&ct_mode==CT_WAIT);
 // Full link entry/import review, one selected record and frozen destination.
 setup();current_screen=SCREEN_CHANNELS;assert(channel_tools_key('i')&&ct_mode==CT_LINK);strcpy(ct_url,link);
 writes=write_count;channel_tools_key('\r');assert(ct_mode==CT_IMPORT&&ct_share.count==1&&ct_slot==2&&write_count==writes);
 channel_tools_key('\t');assert(ct_field==1);channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);assert(ct_slot==3);
 channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_LEFT);assert(ct_slot==2);channel_tools_key('\t');channel_tools_key('\r');
 assert(ct_mode==CT_CONFIRM&&!ct_confirm&&write_count==writes);channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);channel_tools_key('\r');
 assert(channel_operation.index==2&&ct_mode==CT_WAIT&&write_count==writes+1);
 // Map placement is the chosen centre, without changing node location or sending.
 setup();current_screen=SCREEN_MAP;writes=write_count;assert(waypoint_key('n')&&current_screen==SCREEN_WAYPOINTS&&wp_mode==WP_EDIT);
 assert(wp_draft.latitude_i==551700000&&wp_draft.longitude_i==-1188000000&&wp_draft.locked_to==local_node_number&&wp_draft.id&&write_count==writes&&!node_count);
 waypoint_key('C');waypoint_key('a');waypoint_key('m');waypoint_key('p');assert(!strcmp(wp_draft.name,"Camp"));
 waypoint_key('\t');waypoint_key('M');waypoint_key('e');waypoint_key('e');waypoint_key('t');assert(!strcmp(wp_draft.description,"Meet"));
 wp_field=2;waypoint_key(' ');assert(wp_draft.latitude_i==551700000&&wp_draft.longitude_i==-1188000000);
 wp_field=7;waypoint_key('\r');assert(wp_mode==WP_CONFIRM&&!wp_confirm&&write_count==writes);
 waypoint_key('\r');assert(wp_mode==WP_EDIT&&write_count==writes); // Cancel
 waypoint_key('\r');waypoint_navigation(BSP_INPUT_NAVIGATION_KEY_RIGHT);waypoint_key('\r');assert(wp_mode==WP_VIEW&&write_count==writes+1&&mesh_radio_waypoint_count()==1);
 writes=write_count;wp_action=2;waypoint_key('\r');assert(wp_mode==WP_DELETE&&!wp_confirm&&write_count==writes);
 waypoint_navigation(BSP_INPUT_NAVIGATION_KEY_ESC);wp_action=0;map_filters.waypoints_hidden=true;waypoint_key('\r');assert(current_screen==SCREEN_MAP&&!map_filters.waypoints_hidden&&write_count==writes);
 // Locked markers reject local editing and mesh-wide deletion.
 current_screen=SCREEN_WAYPOINTS;wp_mode=WP_VIEW;wp_selected.locked_to=0xaabbccdd;wp_action=1;waypoint_key('\r');assert(wp_mode==WP_VIEW&&strstr(wp_feedback,"Locked")&&write_count==writes);
 wp_action=2;waypoint_key('\r');assert(wp_mode==WP_VIEW&&write_count==writes);
 assert(!waypoint_navigation(BSP_INPUT_NAVIGATION_KEY_F1)&&!channel_tools_navigation(BSP_INPUT_NAVIGATION_KEY_F5));
 puts("PASS: QR loading recovery/same URL/changed key, disabled slot explanation and bounded read-only retries; actual QR/key/link review with no premature writes, default Cancel and frozen import destination; chosen-centre waypoint placement/name/description, explicit send, map viewing and lock protection");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(h)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'tests/stubs'),str(p/'test.c'),str(root/'main/third_party/qrcodegen.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=d,check=True)
