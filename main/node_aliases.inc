// Local labels are separate from received User data and the radio's identity.
#ifndef NODE_ALIASES_PATH
#define NODE_ALIASES_PATH "/locfd/meshmatsu-nicknames.bin"
#endif
typedef struct {
    uint32_t node;
    char name[40];
}
node_alias_t;
typedef struct {
    uint32_t magic,count;
    node_alias_t entries[MESH_NODE_LIMIT];
    uint32_t checksum;
}
node_aliases_t;
static node_aliases_t node_aliases;
static bool alias_text_valid(const char *s){
    size_t n=strnlen(s,40);
    if(n>=40)return false;
    for(size_t i=0;i<n;i++)if((unsigned char)s[i]<32||(unsigned char)s[i]==127)return false;
    // UTF-8 validation rejects truncated codepoints, overlong encodings and surrogates.
    for(size_t i=0;i<n;){
        unsigned c=(unsigned char)s[i++],code=c,bytes=0,min=0;
        if(c<128)continue;
        if(c>=0xc2&&c<=0xdf){
            bytes=1;
            code=c&31;
            min=128;
        }
        else if(c>=0xe0&&c<=0xef){
            bytes=2;
            code=c&15;
            min=2048;
        }
        else if(c>=0xf0&&c<=0xf4){
            bytes=3;
            code=c&7;
            min=65536;
        }
        else return false;
        if(bytes>n-i)return false;
        while(bytes--){
            c=(unsigned char)s[i++];
            if((c&0xc0)!=0x80)return false;
            code=(code<<6)|(c&63);
        }
        if(code<min||code>0x10ffff||(code>=0xd800&&code<=0xdfff))return false;
    }
    return true;
}
static bool aliases_valid(const node_aliases_t *a){
    if(a->magic!=0x4d414c31||a->count>MESH_NODE_LIMIT||a->checksum!=history_checksum(2166136261U,a,offsetof(node_aliases_t,checksum)))return false;
    for(size_t i=0;i<a->count;i++){
        if(!a->entries[i].node||a->entries[i].node==MESH_BROADCAST_NODE||!a->entries[i].name[0]||!alias_text_valid(a->entries[i].name))return false;
        for(size_t j=0;j<i;j++)if(a->entries[j].node==a->entries[i].node)return false;
    }
    return true;
}
static bool local_preferences_write(const char *path,const void *data,size_t size){
    if(!history_fs_ready)return false;
    char temp[128],backup[128];
    snprintf(temp,sizeof(temp),"%s.tmp",path);
    snprintf(backup,sizeof(backup),"%s.bak",path);
    FILE *f=fopen(temp,"wb");
    if(!f)return false;
    bool ok=fwrite(data,1,size,f)==size&&fflush(f)==0&&fsync(fileno(f))==0;
    if(fclose(f))ok=false;
    // Read back every byte before rotating the durable file.
    f=ok?fopen(temp,"rb"):NULL;
    if(ok&&!f)ok=false;
    if(f){
        const uint8_t *bytes=data;
        for(size_t i=0;ok&&i<size;i++)if(fgetc(f)!=bytes[i])ok=false;
        if(ok&&fgetc(f)!=EOF)ok=false;
        if(ferror(f))ok=false;
        if(fclose(f))ok=false;
    }
    bool rotated=false;
    if(ok&&access(path,F_OK)==0){
        remove(backup);
        rotated=rename(path,backup)==0;
        ok=rotated;
    }
    if(ok)ok=rename(temp,path)==0;
    if(!ok){
        if(rotated)rename(backup,path);
        remove(temp);
    }
    return ok;
}
static void load_node_aliases(void){
    memset(&node_aliases,0,sizeof(node_aliases));
    const char *paths[]={
        NODE_ALIASES_PATH,NODE_ALIASES_PATH ".bak"
    }
    ;
    for(unsigned i=0;i<2&&history_fs_ready;i++){
        FILE *f=fopen(paths[i],"rb");
        if(!f)continue;
        node_aliases_t a={
            0
        }
        ;
        bool ok=fread(&a,1,sizeof(a),f)==sizeof(a)&&fgetc(f)==EOF&&!ferror(f);
        if(fclose(f))ok=false;
        if(ok&&aliases_valid(&a)){
            node_aliases=a;
            return;
        }
    }
}
const char *mesh_radio_nickname(uint32_t node){
    if(node==local_node_number)return "";
    for(size_t i=0;i<node_aliases.count;i++)if(node_aliases.entries[i].node==node)return node_aliases.entries[i].name;
    return "";
}
static void node_apply_alias(mesh_node_t *node){
    snprintf(node->advertised_name,sizeof(node->advertised_name),"%s",node->name);
    const char *alias=mesh_radio_nickname(node->number);
    if(alias[0])snprintf(node->name,sizeof(node->name),"%s",alias);
}
bool mesh_radio_set_nickname(uint32_t node,const char *name){
    if(!node||node==MESH_BROADCAST_NODE||node==local_node_number||!name||!alias_text_valid(name))return false;
    static node_aliases_t a;
    a=node_aliases;
    size_t i=0;
    while(i<a.count&&a.entries[i].node!=node)i++;
    if(!name[0]){
        if(i==a.count)return true;
        memmove(&a.entries[i],&a.entries[i+1],(a.count-i-1)*sizeof(a.entries[0]));
        memset(&a.entries[--a.count],0,sizeof(a.entries[0]));
    }
    else{
        if(i==a.count){
            if(i==MESH_NODE_LIMIT)return false;
            a.count++;
        }
        a.entries[i].node=node;
        memset(a.entries[i].name,0,sizeof(a.entries[i].name));
        snprintf(a.entries[i].name,40,"%s",name);
    }
    a.magic=0x4d414c31;
    a.checksum=history_checksum(2166136261U,&a,offsetof(node_aliases_t,checksum));
    if(!local_preferences_write(NODE_ALIASES_PATH,&a,sizeof(a)))return false;
    node_aliases=a;
    history_revision++;
    return true;
}
size_t mesh_radio_nicknames_export(void *out,size_t capacity){
    if(!out||capacity<sizeof(node_aliases))return 0;
    static node_aliases_t a;
    a=node_aliases;
    a.magic=0x4d414c31;
    a.checksum=history_checksum(2166136261U,&a,offsetof(node_aliases_t,checksum));
    memcpy(out,&a,sizeof(a));
    return sizeof(a);
}
bool mesh_radio_nicknames_import(const void *data,size_t size){
    if(!data||size!=sizeof(node_aliases))return false;
    static node_aliases_t a;
    memcpy(&a,data,size);
    if(!aliases_valid(&a)||!local_preferences_write(NODE_ALIASES_PATH,&a,sizeof(a)))return false;
    node_aliases=a;
    history_revision++;
    return true;
}
size_t mesh_radio_mutes_export(void *out,size_t capacity){
    if(!out||capacity<sizeof(node_preferences))return 0;
    __typeof__(node_preferences) p=node_preferences;
    p.magic=0x4d4e5031;
    p.checksum=history_checksum(2166136261U,&p,offsetof(__typeof__(p),checksum));
    memcpy(out,&p,sizeof(p));
    return sizeof(p);
}
bool mesh_radio_mutes_import(const void *data,size_t size){
    if(!data||size!=sizeof(node_preferences))return false;
    __typeof__(node_preferences) p;
    memcpy(&p,data,size);
    if(p.magic!=0x4d4e5031||p.count>MESH_NODE_LIMIT||p.checksum!=history_checksum(2166136261U,&p,offsetof(__typeof__(p),checksum)))return false;
    for(size_t i=0;i<p.count;i++){
        if(!p.muted[i]||p.muted[i]==MESH_BROADCAST_NODE)return false;
        for(size_t j=0;j<i;j++)if(p.muted[i]==p.muted[j])return false;
    }
    if(!local_preferences_write(NODE_PREFS_PATH,&p,sizeof(p)))return false;
    node_preferences=p;
    history_revision++;
    return true;
}
