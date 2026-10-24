#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#define MESH_DRAFT_PATH "drafts.bin"
#include "draft_store.c"
static mesh_draft_t drafts[MESH_DRAFT_LIMIT],loaded[MESH_DRAFT_LIMIT];
static uint32_t replies[MESH_DRAFT_LIMIT],loaded_replies[MESH_DRAFT_LIMIT];
static void save(size_t count){assert(mesh_draft_store_start(drafts,replies,count)&&mesh_draft_store_busy());bool ok=false;assert(mesh_draft_store_poll(&ok)&&ok&&!mesh_draft_store_busy());}
int main(void){
 size_t count=999;bool recovered=true;assert(mesh_draft_store_load(loaded,loaded_replies,MESH_DRAFT_LIMIT,&count,&recovered)&&!count&&!recovered);
 drafts[0]=(mesh_draft_t){.channel=1};strcpy(drafts[0].text,"it's working 🙂");replies[0]=123;
 drafts[1]=(mesh_draft_t){.peer=42};strcpy(drafts[1].text,"private draft");drafts[2]=(mesh_draft_t){.channel=2};replies[2]=456;save(3);
 memset(loaded,0,sizeof(loaded));count=0;assert(mesh_draft_store_load(loaded,loaded_replies,MESH_DRAFT_LIMIT,&count,&recovered)&&count==3&&!recovered&&!strcmp(loaded[0].text,drafts[0].text)&&loaded_replies[0]==123&&loaded[1].peer==42&&loaded_replies[2]==456&&!loaded[2].text[0]);
 // Input is frozen at queue time and prior snapshot remains recoverable.
 assert(mesh_draft_store_start(drafts,replies,3));strcpy(drafts[1].text,"later edit");bool ok;assert(mesh_draft_store_poll(&ok)&&ok);count=0;assert(mesh_draft_store_load(loaded,loaded_replies,MESH_DRAFT_LIMIT,&count,&recovered)&&!strcmp(loaded[1].text,"private draft"));
 // A write failure preserves the durable snapshot and permits a later retry.
 assert(mkdir(MESH_DRAFT_PATH ".tmp",0700)==0);assert(mesh_draft_store_start(drafts,replies,3));assert(mesh_draft_store_poll(&ok)&&!ok);rmdir(MESH_DRAFT_PATH ".tmp");save(3);
 FILE *f=fopen(MESH_DRAFT_PATH,"r+b");assert(f);fseek(f,32,SEEK_SET);fputc(255,f);fclose(f);
 assert(mesh_draft_store_load(loaded,loaded_replies,MESH_DRAFT_LIMIT,&count,&recovered)&&recovered&&!strcmp(loaded[1].text,"private draft"));save(3);assert(mesh_draft_store_load(loaded,loaded_replies,MESH_DRAFT_LIMIT,&count,&recovered)&&!recovered&&!strcmp(loaded[1].text,"later edit"));
 // A future-format primary is never replaced by an older backup.
 f=fopen(MESH_DRAFT_PATH,"r+b");assert(f);fseek(f,4,SEEK_SET);fputc(2,f);fclose(f);
 assert(!mesh_draft_store_load(loaded,loaded_replies,MESH_DRAFT_LIMIT,&count,&recovered)&&!mesh_draft_store_start(drafts,replies,3));
 f=fopen(MESH_DRAFT_PATH,"r+b");assert(f);fseek(f,4,SEEK_SET);fputc(1,f);fclose(f);assert(mesh_draft_store_load(loaded,loaded_replies,MESH_DRAFT_LIMIT,&count,&recovered));
 // Empty saves remove sent/discarded entries, including a quote-only draft.
 memset(drafts,0,sizeof(drafts));memset(replies,0,sizeof(replies));save(3);assert(mesh_draft_store_load(loaded,loaded_replies,MESH_DRAFT_LIMIT,&count,&recovered)&&!count);
 // Reject duplicates, bad UTF-8 and overlong text before starting any worker.
 drafts[0].channel=1;strcpy(drafts[0].text,"one");drafts[1]=drafts[0];assert(!mesh_draft_store_start(drafts,replies,2));
 strcpy(drafts[0].text,"bad\xed\xa0\x80");assert(!mesh_draft_store_start(drafts,replies,1));memset(drafts[0].text,'x',sizeof(drafts[0].text));assert(!mesh_draft_store_start(drafts,replies,1));
 // Both unreadable archives block overwrite, keeping session drafts in RAM.
 f=fopen(MESH_DRAFT_PATH,"wb");assert(f);fputs("future version",f);fclose(f);f=fopen(MESH_DRAFT_PATH ".bak","wb");assert(f);fputs("corrupt",f);fclose(f);count=99;assert(!mesh_draft_store_load(loaded,loaded_replies,MESH_DRAFT_LIMIT,&count,&recovered)&&count==99);strcpy(drafts[0].text,"session");assert(!mesh_draft_store_start(drafts,replies,1));
 remove(MESH_DRAFT_PATH);remove(MESH_DRAFT_PATH ".bak");assert(mesh_draft_store_load(loaded,loaded_replies,MESH_DRAFT_LIMIT,&count,&recovered));
 for(size_t i=0;i<MESH_DRAFT_LIMIT;i++){drafts[i]=(mesh_draft_t){.peer=(uint32_t)i+1};memset(drafts[i].text,'x',200);drafts[i].text[200]=0;replies[i]=UINT32_MAX;}save(MESH_DRAFT_LIMIT);assert(mesh_draft_store_load(loaded,loaded_replies,MESH_DRAFT_LIMIT,&count,&recovered)&&count==MESH_DRAFT_LIMIT&&strlen(loaded[count-1].text)==200);
 puts("PASS: draft/quote/UTF-8 roundtrip, immutable snapshot, transactional failure/retry, corrupt-primary backup recovery, empty clearing, invalid/duplicate/future-file guards and full 188-draft capacity");
}
