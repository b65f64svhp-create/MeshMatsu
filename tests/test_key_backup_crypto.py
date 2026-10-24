"""Build the bundled PSA library and test real crypto + SD files, with independent Python decryption."""
from pathlib import Path
import subprocess,tempfile,os,sys,importlib.util
root=Path(__file__).resolve().parents[1]
idf_setting=os.environ.get('IDF_PATH')
if idf_setting:
 idf=Path(idf_setting)
elif (root/'.IDF_PATH').is_file():
 idf=Path((root/'.IDF_PATH').read_text().strip())
else:
 idf=root/'vendor/esp-idf'
if not importlib.util.find_spec('cryptography'):
 raise RuntimeError('Install the cryptography Python package to run this separate crypto harness.')
psa=idf/'components/mbedtls/mbedtls/tf-psa-crypto'
cmake='/opt/homebrew/bin/cmake' if Path('/opt/homebrew/bin/cmake').exists() else 'cmake'
def run(args,**kw):
 p=subprocess.run(args,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,**kw)
 if p.returncode:raise RuntimeError(p.stdout)
 return p
with tempfile.TemporaryDirectory(prefix='meshmatsu-key-test-') as d:
 p=Path(d);shim=p/'shim/mbedtls';shim.mkdir(parents=True)
 # ESP's upstream checkout includes wrapper references to these two old public paths.
 for h in ('bignum','ecp'):(shim/(h+'.h')).write_text('#define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS\n#include "mbedtls/private/'+h+'.h"\n')
 build=p/'psa'
 run([cmake,'-S',str(psa),'-B',str(build),'-DENABLE_TESTING=OFF','-DENABLE_PROGRAMS=OFF','-DGEN_FILES=OFF','-DTF_PSA_CRYPTO_FATAL_WARNINGS=OFF','-DCMAKE_C_FLAGS=-I'+str(p/'shim')])
 run([cmake,'--build',str(build),'-j','4'])
 (p/'backups').mkdir()
 run(['clang','-O2','-std=c11','-fsanitize=address,undefined','-I',str(root/'main'),'-I',str(build/'include'),'-I',str(psa/'include'),'-I',str(psa/'drivers/builtin/include'),'-DMESH_KEY_BACKUP_DIR="'+str(p/'backups')+'"',str(root/'tests/key_backup_crypto_harness.c'),str(root/'main/key_backup.c'),str(root/'main/key_backup_store.c'),str(build/'core/libtfpsacrypto.a'),'-o',str(p/'test')])
 print(run([str(p/'test')],cwd=d).stdout.strip())
 python=sys.executable
 code=r'''
import hashlib, pathlib, struct
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
file=pathlib.Path('fixture.mmk').read_bytes()
assert file[:8]==b'MMKEY001' and struct.unpack_from('<I',file,8)[0]==600000
key=hashlib.pbkdf2_hmac('sha256',b'a long test passphrase',file[12:28],600000,32)
plain=AESGCM(key).decrypt(file[28:40],file[44:],file[:44])
assert len(plain)==486 and plain[:4]==b'KEY1' and plain[12:20]==b'TESTNODE'
assert plain[85:117].hex()=='77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a'
assert plain[117]==2 and plain[178:210]==bytes([42])*32
assert b'TESTNODE' not in file and plain[85:117] not in file
print('PASS: independent Python PBKDF2-SHA256 and AES-256-GCM decryption matches the actual PSA-produced file')
'''
 print(run([python,'-c',code],cwd=d).stdout.strip())
