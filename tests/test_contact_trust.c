#define MANAGEMENT_TEST_MAIN earlier_management_tests
#include "test_management.c"
#include <sys/stat.h>
int main(void){
 setup();uint8_t key[32];memcpy(key,nodes[0].public_key,32);assert(mesh_radio_verify_contact(peer,key));
 nodes[0].public_key[0]++;uint8_t second[32];memcpy(second,nodes[0].public_key,32);assert(mesh_radio_verify_contact(peer,second));
 // The old complete snapshot is recoverable if primary is corrupt.
 FILE *f=fopen(CONTACT_TRUST_PATH,"wb");assert(f);fwrite("broken",1,6,f);fclose(f);trust_load();assert(trust_recovered&&mesh_radio_contact_trust(peer,key,NULL)==MESH_TRUST_VERIFIED);
 assert(mesh_radio_verify_contact(peer,second));assert(trust_read(CONTACT_TRUST_PATH ".bak")&&mesh_radio_contact_trust(peer,key,NULL)==MESH_TRUST_VERIFIED);
 trust_load();assert(mesh_radio_contact_trust(peer,second,NULL)==MESH_TRUST_VERIFIED);
 // Failed persistence must not grant new trust in RAM or destroy either complete copy.
 nodes[0].public_key[0]++;uint8_t third[32];memcpy(third,nodes[0].public_key,32);mkdir(CONTACT_TRUST_PATH ".tmp",0700);
 assert(!mesh_radio_verify_contact(peer,third));assert(mesh_radio_contact_trust(peer,third,NULL)==MESH_TRUST_CHANGED);rmdir(CONTACT_TRUST_PATH ".tmp");trust_load();assert(mesh_radio_contact_trust(peer,second,NULL)==MESH_TRUST_VERIFIED);
 f=fopen(CONTACT_TRUST_PATH,"wb");assert(f);fwrite("MCT2",1,4,f);fclose(f);trust_load();assert(!mesh_radio_trust_storage_ok()&&!mesh_radio_verify_contact(peer,third));
 f=fopen(CONTACT_TRUST_PATH,"rb");char magic[4];assert(fread(magic,1,4,f)==4&&!memcmp(magic,"MCT2",4));fclose(f);
 // Invalid UTF-8, zero keys and excessive inputs cannot become contacts.
 setup();mesh_contact_t c;assert(mesh_radio_contact(peer,&c));strcpy(c.name,"\xc0\xaf");char url[256];assert(!mesh_radio_contact_url(&c,url,sizeof(url)));
 strcpy(c.name,"test");memset(c.key,0,32);assert(!mesh_radio_contact_url(&c,url,sizeof(url)));
 memset(url,'A',sizeof(url)-1);url[sizeof(url)-1]=0;memcpy(url,"https://meshtastic.org/v/#",25);assert(!mesh_radio_parse_contact_url(url,&c));
 // Every saved row fits and an exhausted trust store refuses a new verification.
 setup();contact_trust_count=MESH_NODE_LIMIT;for(unsigned i=0;i<MESH_NODE_LIMIT;i++){contact_trust[i].number=i+1;memset(contact_trust[i].key,i+1,32);contact_trust[i].verified=i%2;}
 assert(trust_save());contact_trust_count=0;assert(trust_read(CONTACT_TRUST_PATH)&&contact_trust_count==MESH_NODE_LIMIT);assert(!mesh_radio_verify_contact(peer,nodes[0].public_key));
 puts("PASS: internal trust capacity/restart/backup recovery, verified temp rotation, failed write rollback, future-format overwrite protection and strict public contact input validation");return 0;
}
