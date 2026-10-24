"""Compare actual map renderer output and I/O with caches enabled/disabled."""
from pathlib import Path
import struct,subprocess,tempfile,zlib
root=Path(__file__).resolve().parents[1]
tiles={}
for x in range(100,106):
 for y in range(80,84):
  data=struct.pack('<H',x*100+y)*65536
  tiles[8,x,y]=(zlib.compress(data),zlib.crc32(data))
entries=[];payload=bytearray();offset=16+len(tiles)*24
for key,(data,crc) in sorted(tiles.items()):
 entries.append(struct.pack('<6I',*key,offset+len(payload),len(data),crc));payload.extend(data)
pack=struct.pack('<8sII',b'MMAP0001',len(tiles),256)+b''.join(entries)+payload
harness=r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <zlib.h>
#include "'''+str(root/'main/map_tiles.c')+r'''"
static unsigned hash_frame(void){return crc32(0,(const unsigned char*)front,760*266*2);}
static void direct_render(mesh_map_view_t view){render((map_request_t){view,760,266,rendered_generation});}
int main(int argc,char **argv){
 assert(!setenv("MESHMATSU_MAP",argv[1],1));mesh_map_view_t a={102.5/256,81.5/256,8},b=a;
 mesh_map_pan(&b,96,0);mesh_map_tiles_request(&a,760,266);assert(ready&&archive.file&&archive.index);
 assert(mesh_map_tiles_needs_draw());pax_buf_t fb;char status[80];
 assert(mesh_map_tiles_draw(&fb,20,94,&a,760,266,status,sizeof(status))&&!mesh_map_tiles_needs_draw());
 unsigned hashes[3];hashes[0]=hash_frame();uint64_t cold=archive.tile_reads;assert(cold>0);
 mesh_map_tiles_request(&b,760,266);hashes[1]=hash_frame();uint64_t pan=archive.tile_reads-cold;assert(pan>0&&pan<cold);
 mesh_map_tiles_request(&a,760,266);hashes[2]=hash_frame();assert(hashes[0]==hashes[2]);
 uint64_t fast_reads=archive.tile_reads,fast_index=archive.index_reads;assert(fast_reads==cold+pan&&fast_index==1&&cache.hits>0);
 // A repeated request neither reloads nor redraws anything in the worker.
 mesh_map_tiles_request(&a,760,266);assert(archive.tile_reads==fast_reads);
 // Simulate the v33 path: no decoded cache and index lookup through the file.
 mesh_map_cache_init(&cache,NULL,0);free(archive.index);archive.index=NULL;
 archive.tile_reads=archive.index_reads=archive.decoded_tiles=0;
 direct_render(a);assert(hash_frame()==hashes[0]);direct_render(b);assert(hash_frame()==hashes[1]);direct_render(a);assert(hash_frame()==hashes[2]);
 uint64_t slow_reads=archive.tile_reads,slow_index=archive.index_reads;
 assert(slow_reads>fast_reads&&slow_index>fast_index);
 // R reloads the archive/index and invalidates decoded tiles in the worker.
 mesh_map_tiles_retry();mesh_map_tiles_request(&a,760,266);
 assert(archive.index&&archive.index_reads==1&&archive.decoded_tiles==cold&&hash_frame()==hashes[0]);
 assert(mesh_map_tiles_needs_draw());assert(mesh_map_tiles_draw(&fb,20,94,&a,760,266,status,sizeof(status))&&!mesh_map_tiles_needs_draw());
 assert(cache.capacity==MESH_MAP_CACHE_TILES);
 printf("PASS: identical map pixels; cold=%llu tile reads, pan=%llu new reads, return=0; 3-view total %llu -> %llu tile reads and %llu -> %llu index reads; explicit retry refreshes caches\n",
 (unsigned long long)cold,(unsigned long long)pan,(unsigned long long)slow_reads,(unsigned long long)fast_reads,(unsigned long long)slow_index,(unsigned long long)fast_index);
 mesh_map_archive_close(&archive);free(front);free(back);free(tile);free(compressed);free(cached_pixels);
}
'''
stub=r'''
#pragma once
#include <stdbool.h>
#include <stddef.h>
typedef struct {int unused;} pax_buf_t;
#define PAX_BUF_16_565RGB 0
static inline bool pax_buf_init(pax_buf_t *b,void *pixels,int w,int h,int type){return true;}
static inline void pax_draw_image_op(pax_buf_t *fb,const pax_buf_t *image,int x,int y){}
static inline void pax_buf_destroy(pax_buf_t *b){}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'pax_gfx.h').write_text(stub);(p/'map.mmap').write_bytes(pack);(p/'test.c').write_text(harness)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(p),'-I',str(root/'main'),str(p/'test.c'),str(root/'main/map_view.c'),str(root/'main/map_archive.c'),str(root/'main/map_cache.c'),'-lz','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),str(p/'map.mmap')],check=True)
