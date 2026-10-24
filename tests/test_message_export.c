#define main original_tests
#include "test_direct_messages.c"
#undef main
#define MESH_EXPORT_ROOT "chat-export"
#include "message_export.c"
#include <dirent.h>
static void cleanup(void){DIR *d=opendir(EXPORT_DIR);if(d){struct dirent *e;while((e=readdir(d)))if(e->d_name[0]!='.'){char path[300];snprintf(path,sizeof(path),EXPORT_DIR "/%s",e->d_name);remove(path);}closedir(d);}rmdir(EXPORT_DIR);rmdir(MESH_EXPORT_ROOT);}
static void add_message(const char *text,uint32_t peer,uint8_t channel,uint32_t id){assert(store_message("!11223344",(const uint8_t*)text,strlen(text),1800000000,peer,false,channel));messages[message_count-1].packet_id=id;}
int main(void){
 reset();cleanup();assert(mesh_chat_export_start(UINT32_MAX,0)==MESH_CHAT_EXPORT_INVALID&&mesh_chat_export_start(0,8)==MESH_CHAT_EXPORT_INVALID&&mesh_chat_export_start(0,0)==MESH_CHAT_EXPORT_EMPTY);
 add_message("it's working ❤️\nsecond line",0,1,11);add_message("other channel private",0,0,12);add_message("direct private",42,0,13);
 size_t writes=write_count;assert(mesh_chat_export_start(0,1)==MESH_CHAT_EXPORT_OK&&mesh_chat_export_busy());assert(mesh_chat_export_start(42,0)==MESH_CHAT_EXPORT_BUSY);add_message("later arrival",0,1,14);
 assert(mesh_chat_export_poll()&&!mesh_chat_export_poll()&&!mesh_chat_export_busy());mesh_chat_export_status_t status=mesh_chat_export_status();assert(status.success&&status.count==1&&!status.peer&&status.channel==1&&write_count==writes);
 char path[240],contents[5000];snprintf(path,sizeof(path),EXPORT_DIR "/%s",status.filename);FILE *f=fopen(path,"rb");assert(f);size_t n=fread(contents,1,sizeof(contents)-1,f);contents[n]=0;fclose(f);assert(strstr(contents,"it's working ❤️\nsecond line")&&!strstr(contents,"other channel private")&&!strstr(contents,"direct private")&&!strstr(contents,"later arrival")&&strstr(contents,"2027-01-15 08:00:00 UTC")&&strstr(contents,"Packet: 11"));
 assert(mesh_chat_export_start(42,0)==MESH_CHAT_EXPORT_OK);mesh_chat_export_poll();status=mesh_chat_export_status();assert(status.success&&status.count==1&&status.peer==42);snprintf(path,sizeof(path),EXPORT_DIR "/%s",status.filename);f=fopen(path,"rb");assert(f);n=fread(contents,1,sizeof(contents)-1,f);contents[n]=0;fclose(f);assert(strstr(contents,"direct private")&&!strstr(contents,"it's working"));
 // Numbered filenames preserve prior files and abandoned partial exports.
 chat_export_row_t row={.message={.packet_id=99}};strcpy(row.message.text,"=1+1 remains plain text");strcpy(row.sender,"Test");export_job_t job={.samples=&row,.count=1,.peer=42};assert(export_write(&job));char first[64];strcpy(first,job.filename);assert(export_write(&job)&&strcmp(first,job.filename));
 char held[248];snprintf(held,sizeof(held),EXPORT_DIR "/chat-0000002a-c0-0000000000-003.txt.part");f=fopen(held,"wb");assert(f);fputs("preserve",f);fclose(f);assert(export_write(&job)&&strstr(job.filename,"-004.txt"));snprintf(path,sizeof(path),EXPORT_DIR "/%s",job.filename);assert(export_verified(path,&job.csv));f=fopen(path,"ab");assert(f);fputc('x',f);fclose(f);assert(!export_verified(path,&job.csv));f=fopen(held,"rb");assert(f);assert(fread(contents,1,8,f)==8&&!memcmp(contents,"preserve",8));fclose(f);
 cleanup();f=fopen(MESH_EXPORT_ROOT,"wb");assert(f);fputs("preserve",f);fclose(f);assert(mesh_chat_export_start(42,0)==MESH_CHAT_EXPORT_OK);mesh_chat_export_poll();assert(!mesh_chat_export_status().success&&strstr(mesh_chat_export_status().message,"SD unavailable"));remove(MESH_EXPORT_ROOT);
 assert(mesh_chat_export_start(42,0)==MESH_CHAT_EXPORT_OK);mesh_chat_export_poll();assert(mesh_chat_export_status().success);cleanup();
 puts("PASS: exact channel/DM scope, immutable snapshot, UTF-8/multiline plain text, original UTC/packet metadata, no radio writes, busy lifecycle, unique files, checksum readback, partial preservation, SD failure and retry");
}
