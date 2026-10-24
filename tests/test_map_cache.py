"""Check real archive reads, cache reuse, viewport pins and fallback paths."""
from pathlib import Path
import struct,subprocess,tempfile,zlib
root=Path(__file__).resolve().parents[1]
tiles={}
for z in [8,9]:
 for x in range(6):
  pixels=struct.pack('<H',z*100+x)*65536
  tiles[z,x,0]=(zlib.compress(pixels),zlib.crc32(pixels))
pixels=struct.pack('<H',999)*65536
tiles[8,6,0]=(zlib.compress(pixels),0) # Invalid CRC must never enter the cache.
keys=sorted(tiles);payload=bytearray();entries=[];offset=16+24*len(keys)
for key in keys:
 data,crc=tiles[key];entries.append(struct.pack('<6I',*key,offset+len(payload),len(data),crc));payload.extend(data)
pack=struct.pack('<8sII',b'MMAP0001',len(keys),256)+b''.join(entries)+payload
harness=r'''
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "map_cache.h"
static void check(const uint16_t *pixels,unsigned value){assert(pixels);for(unsigned i=0;i<65536;i++)assert(pixels[i]==value);}
int main(int argc,char **argv){
 mesh_map_archive_t archive;assert(mesh_map_archive_open(&archive,argv[1])&&archive.index&&archive.index_reads==1);
 uint16_t *pool=malloc(MESH_MAP_TILE_BYTES*4),*scratch=malloc(MESH_MAP_TILE_BYTES);
 uint8_t *compressed=malloc(MESH_MAP_COMPRESSED_MAX);mesh_map_cache_t cache;
 mesh_map_cache_init(&cache,pool,4);mesh_map_cache_begin_view(&cache);
 for(unsigned x=0;x<4;x++)check(mesh_map_cache_tile(&cache,&archive,8,x,0,compressed,scratch),800+x);
 assert(archive.tile_reads==4&&archive.decoded_tiles==4&&cache.misses==4&&!cache.hits);
 // Loading the new edge first must not evict any overlapping, pinned tile.
 mesh_map_cache_begin_view(&cache);for(unsigned x=1;x<4;x++)mesh_map_cache_pin(&cache,8,x,0);
 check(mesh_map_cache_tile(&cache,&archive,8,4,0,compressed,scratch),804);
 for(unsigned x=1;x<4;x++)check(mesh_map_cache_tile(&cache,&archive,8,x,0,compressed,scratch),800+x);
 assert(archive.tile_reads==5&&cache.hits==3&&archive.index_reads==1);
 mesh_map_cache_begin_view(&cache);for(unsigned x=0;x<4;x++)mesh_map_cache_pin(&cache,8,x,0);
 for(unsigned x=0;x<4;x++)check(mesh_map_cache_tile(&cache,&archive,8,x,0,compressed,scratch),800+x);
 assert(archive.tile_reads==6&&cache.hits==6);
 // Same view: all four come from RAM; no new payload reads/decompression.
 mesh_map_cache_begin_view(&cache);for(unsigned x=0;x<4;x++)mesh_map_cache_pin(&cache,8,x,0);
 for(unsigned x=0;x<4;x++)check(mesh_map_cache_tile(&cache,&archive,8,x,0,compressed,scratch),800+x);
 assert(archive.tile_reads==6&&archive.decoded_tiles==6&&cache.hits==10);
 // All cache slots pinned: an unrelated tile uses the scratch buffer safely.
 assert(mesh_map_cache_tile(&cache,&archive,9,0,0,compressed,scratch)==scratch);check(scratch,900);
 assert(!mesh_map_cache_tile(&cache,&archive,8,6,0,compressed,scratch));
 assert(!mesh_map_cache_tile(&cache,&archive,8,7,0,compressed,scratch));
 // Bounded cache eviction and zoom-key isolation.
 mesh_map_cache_begin_view(&cache);check(mesh_map_cache_tile(&cache,&archive,9,0,0,compressed,scratch),900);
 mesh_map_cache_begin_view(&cache);check(mesh_map_cache_tile(&cache,&archive,8,0,0,compressed,scratch),800);
 // Optional allocations may fail: both cache and RAM index can be absent.
 mesh_map_cache_init(&cache,NULL,4);assert(!cache.capacity);mesh_map_cache_begin_view(&cache);
 free(archive.index);archive.index=NULL;uint64_t reads=archive.index_reads;
 check(mesh_map_cache_tile(&cache,&archive,8,2,0,compressed,scratch),802);assert(archive.index_reads>reads);
 mesh_map_archive_close(&archive);assert(!archive.file&&!archive.index);
 free(pool);free(scratch);free(compressed);
 puts("PASS: RAM index lookup, overlapping-view pinning, zero-read warm views, LRU/zoom keys, bounded scratch fallback, bad/missing tiles and low-memory fallback");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'map.mmap').write_bytes(pack);(p/'test.c').write_text(harness)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'main'),str(p/'test.c'),str(root/'main/map_archive.c'),str(root/'main/map_cache.c'),'-lz','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),str(p/'map.mmap')],check=True)
