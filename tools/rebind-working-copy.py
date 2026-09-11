"""Import the exact r354 generation onto new Windows file objects, once.

Katana's v2 manifest hashes both bytes and kernel file identity/timestamps.
Copying those files deliberately changes the latter. Content authority comes
from the sealed baseline manifest; subsequent edits use the SDK's strict
rewrite_codegen_project_artifacts API, never this relocation step.
"""
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import os
from pathlib import Path

root = Path(__file__).resolve().parents[1]
working = root/'.local/working-product'
generated = working/'generated'
marker = working/'relocated.json'
if marker.exists():
    raise SystemExit(0)

class FileInfo(c.Structure):
    _fields_ = [('attributes', w.DWORD), ('creation', w.FILETIME),
                ('access', w.FILETIME), ('write', w.FILETIME),
                ('volume', w.DWORD), ('size_high', w.DWORD),
                ('size_low', w.DWORD), ('links', w.DWORD),
                ('index_high', w.DWORD), ('index_low', w.DWORD)]
class BasicInfo(c.Structure):
    _fields_ = [('creation', c.c_int64), ('access', c.c_int64),
                ('write', c.c_int64), ('change', c.c_int64), ('attributes', w.DWORD)]
kernel = c.WinDLL('kernel32', use_last_error=True)
kernel.CreateFileW.argtypes = [w.LPCWSTR,w.DWORD,w.DWORD,c.c_void_p,w.DWORD,w.DWORD,w.HANDLE]
kernel.CreateFileW.restype = w.HANDLE
kernel.GetFileInformationByHandle.argtypes = [w.HANDLE,c.POINTER(FileInfo)]
kernel.GetFileInformationByHandleEx.argtypes = [w.HANDLE,c.c_int,c.c_void_p,w.DWORD]
kernel.CloseHandle.argtypes = [w.HANDLE]
def binding(path):
    handle = kernel.CreateFileW(str(path),0x80,1|4,None,3,0x80|0x02000000,None)
    if handle == c.c_void_p(-1).value: raise c.WinError(c.get_last_error())
    try:
        info,basic = FileInfo(),BasicInfo()
        if not kernel.GetFileInformationByHandle(handle,c.byref(info)) or not kernel.GetFileInformationByHandleEx(handle,0,c.byref(basic),c.sizeof(basic)):
            raise c.WinError(c.get_last_error())
        if info.attributes & (0x10|0x400): raise RuntimeError('Not an independent regular file')
        size = (info.size_high<<32)|info.size_low
        fields = [info.volume,info.index_high,info.index_low,size,basic.creation,basic.write,basic.change]
        return size,'sha256:'+hashlib.sha256(('windows-v2|'+'|'.join(map(str,fields))).encode()).hexdigest()
    finally: kernel.CloseHandle(handle)
def digest(path):
    with path.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()

manifest = generated/'.katana-generated-artifacts'
record = json.loads((root/'baseline/r354.json').read_text())
expected = {row['path']:row for row in record['files']}
prefix = '.local/baseline/r354/product/generated/'
if digest(manifest) != expected[prefix+manifest.name]['sha256']:
    raise SystemExit('Relocation requires the untouched, authenticated r354 manifest')
lines = manifest.read_text().splitlines()
if lines[0] != 'katana-codegen-artifacts-v2': raise SystemExit('Unknown manifest format')
if lines[1] != 'generation\tsha256:'+hashlib.sha256(('\n'.join(lines[2:])+'\n').encode()).hexdigest():
    raise SystemExit('Invalid source generation')
records=[]
for line in lines[2:]:
    name,size,sha,_ = line.split('\t')
    path = generated/name
    if not path.resolve().is_relative_to(generated.resolve()) or path.is_symlink() or path.is_junction():
        raise SystemExit('Unsafe generated path')
    trusted = expected[prefix+name]
    if trusted['bytes'] != int(size) or 'sha256:'+trusted['sha256'] != sha:
        raise SystemExit('Unbound source artifact: '+name)
    before = binding(path)
    actual = digest(path)
    after = binding(path)
    if before != after or after[0] != int(size) or 'sha256:'+actual != sha:
        raise SystemExit('Changed copied artifact: '+name)
    records.append(f'{name}\t{size}\t{sha}\t{after[1]}\n')
body=''.join(records)
new='katana-codegen-artifacts-v2\ngeneration\tsha256:'+hashlib.sha256(body.encode()).hexdigest()+'\n'+body
temporary = manifest.with_name(manifest.name+'.relocating')
with temporary.open('x',encoding='utf-8',newline='\n') as f: f.write(new)
os.replace(temporary,manifest)
marker.write_text(json.dumps({'files':len(records),'source_manifest_sha256':expected[prefix+manifest.name]['sha256'],
                              'relocated_manifest_sha256':digest(manifest)},indent=2)+'\n')
print(f'SONIC_GENERATION_RELOCATED verified_files={len(records)} content_changes=0')
