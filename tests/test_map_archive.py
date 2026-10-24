"""Exercise the production archive reader against valid and corrupted packs."""
from pathlib import Path
import struct,subprocess,tempfile,zlib
root=Path(__file__).resolve().parents[1]
pixels=struct.pack('<H',0x1234)*65536
hash_=zlib.crc32(pixels)
payload=zlib.compress(pixels)
header=struct.pack('<8sII',b'MMAP0001',1,256)
entry=struct.pack('<6I',8,43,80,40,len(payload),hash_)
valid=header+entry+payload
harness=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "map_archive.h"
int main(int argc,char **argv) {
 mesh_map_archive_t archive;bool opened=mesh_map_archive_open(&archive,argv[1]);
 uint8_t *compressed=malloc(MESH_MAP_COMPRESSED_MAX);uint16_t *pixels=malloc(MESH_MAP_TILE_BYTES);
 bool success=opened&&mesh_map_archive_tile(&archive,8,43,80,compressed,pixels);
 assert(success==(argv[2][0]=='1'));
 if(success){for(size_t i=0;i<65536;i++)assert(pixels[i]==0x1234);}
 assert(!mesh_map_archive_tile(&archive,8,0,0,compressed,pixels));
 assert(!mesh_map_archive_tile(&archive,31,0,0,compressed,pixels));
 assert(!mesh_map_archive_tile(&archive,8,256,0,compressed,pixels));
 mesh_map_archive_close(&archive);free(compressed);free(pixels);return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(harness)
 subprocess.run(['clang','-std=c11','-fsanitize=address,undefined','-I',str(root/'main'),str(p/'test.c'),str(root/'main/map_archive.c'),'-lz','-o',str(p/'test')],check=True)
 cases=[(valid,True),(b'',False),(b'bad'+valid[3:],False),(valid[:-1],False)]
 for index,value in [(8,1000001),(12,512),(28,0),(28,0xffffffff),(32,0),(32,0xffffffff),(36,hash_^1)]:
  bad=bytearray(valid);struct.pack_into('<I',bad,index,value);cases.append((bad,False))
 cases.append((header+struct.pack('<6I',8,43,80,40,len(payload),hash_)+bytes(len(payload)),False))
 for data,expected in cases:
  (p/'map.mmap').write_bytes(data);subprocess.run([str(p/'test'),str(p/'map.mmap'),'1' if expected else '0'],check=True)
print('PASS: actual RGB565 archive decode and checksum, missing tiles, invalid zoom/coordinates, truncated/corrupt header, offsets, lengths and payloads')
