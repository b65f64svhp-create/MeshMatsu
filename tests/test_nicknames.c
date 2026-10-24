#define NODE_ALIASES_PATH "nicknames.bin"
#define NODE_PREFS_PATH "muted.bin"
#define main existing_tests
#include "test_direct_messages.c"
#undef main
int main(void){
 reset();history_fs_ready=true;node_aliases=(node_aliases_t){0};
 uint32_t peer=0x11223344;uint8_t user[]={18,5,'R','a','d','i','o',26,2,'R','D'};update_node_user(peer,user,sizeof(user));size_t writes=write_count;
 assert(mesh_radio_set_nickname(peer,"Camp North"));mesh_node_t n;assert(mesh_radio_get_node_by_number(peer,&n)&&!strcmp(n.name,"Camp North")&&!strcmp(n.advertised_name,"Radio"));
 mesh_message_t m={.peer_node=peer};char from[40];mesh_radio_message_sender_name(&m,from,sizeof(from));assert(!strcmp(from,"Camp North"));
 user[6]='2';update_node_user(peer,user,sizeof(user));assert(mesh_radio_get_node(0,&n)&&!strcmp(n.name,"Camp North")&&!strcmp(n.advertised_name,"Radi2"));
 memset(&node_aliases,0,sizeof(node_aliases));load_node_aliases();assert(!strcmp(mesh_radio_nickname(peer),"Camp North"));
 uint8_t blob[6144];size_t size=mesh_radio_nicknames_export(blob,sizeof(blob));assert(size&&mesh_radio_set_nickname(peer,"Changed")&&mesh_radio_nicknames_import(blob,size));assert(!strcmp(mesh_radio_nickname(peer),"Camp North"));
 blob[10]^=1;assert(!mesh_radio_nicknames_import(blob,size));blob[10]^=1;assert(!mesh_radio_nicknames_import(blob,size-1));
 assert(!mesh_radio_set_nickname(peer,"bad\nname")&&!mesh_radio_set_nickname(peer,"\xc0\x80")&&!mesh_radio_set_nickname(peer,"\xf0\x9f"));
 assert(mesh_radio_set_nickname(peer,"Caf\xc3\xa9"));assert(!mesh_radio_set_nickname(local_node_number,"Me")&&!mesh_radio_set_nickname(0,"Zero"));
 assert(mkdir(NODE_ALIASES_PATH ".tmp",0700)==0);assert(!mesh_radio_set_nickname(peer,"Unsaved")&&!strcmp(mesh_radio_nickname(peer),"Caf\xc3\xa9"));assert(!rmdir(NODE_ALIASES_PATH ".tmp"));
 assert(mesh_radio_set_nickname(peer,""));assert(mesh_radio_get_node_by_number(peer,&n)&&!strcmp(n.name,"Radi2"));assert(write_count==writes);
 assert(mesh_radio_set_node_muted(peer,true));size=mesh_radio_mutes_export(blob,sizeof(blob));assert(size&&mesh_radio_set_node_muted(peer,false)&&mesh_radio_mutes_import(blob,size)&&mesh_radio_node_muted(peer));
 assert(!rename(NODE_ALIASES_PATH,NODE_ALIASES_PATH ".saved"));assert(!rename(NODE_ALIASES_PATH ".bak",NODE_ALIASES_PATH ".bak2"));assert(!rename(NODE_ALIASES_PATH ".saved",NODE_ALIASES_PATH ".bak"));load_node_aliases();assert(!mesh_radio_nickname(peer)[0]);
 puts("PASS: durable local UTF-8 nicknames, advertised-name preservation across radio updates, sender lookup, clearing, failed writes, malformed/import/checksum rejection, muted-list restore and no radio writes");
}
