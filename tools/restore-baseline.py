"""Restore the private, hash-pinned r354 development bundle into a fresh clone."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import zipfile

root=Path(__file__).resolve().parents[1]
record=json.loads((root/'baseline/development-bundle.json').read_text())
base=root/'.local/baseline/r354'
toolchain=root/'.local/toolchain'
staging=root/'.local/restore-r354'
for target in (base,toolchain,staging):
    if target.exists() or target.is_symlink() or target.is_junction():
        raise SystemExit(f'Refusing to replace existing data: {target}')
def checked_path(folder,name):
    relative=PurePosixPath(name)
    if relative.is_absolute() or '..' in relative.parts or '\\' in name or ':' in name:
        raise SystemExit('Unsafe archive path')
    target=folder/relative
    if not target.resolve().is_relative_to(folder.resolve()): raise SystemExit('Archive path escapes destination')
    return target
def verify(path,size,sha):
    if path.stat().st_size!=size: raise SystemExit(f'Incorrect size: {path.name}')
    with path.open('rb') as f: actual=hashlib.file_digest(f,'sha256').hexdigest()
    if actual!=sha: raise SystemExit(f'Incorrect SHA-256: {path.name}')
archives=root/'.local/archives'
archives.mkdir(parents=True,exist_ok=True)
for part in record['parts']:
    name=part['name']
    if Path(name).name!=name: raise SystemExit('Invalid archive part name')
    destination=checked_path(archives,name)
    if not destination.exists():
        subprocess.run(['gh','release','download',record['tag'],'--repo','sonicfreak1337/SARecomp',
                        '--pattern',name,'--dir',str(archives)],check=True)
    verify(destination,part['bytes'],part['sha256'])
archive=archives/'r354-development.zip'
if not archive.exists():
    with archive.open('xb') as output:
        for part in record['parts']:
            with (archives/part['name']).open('rb') as source: shutil.copyfileobj(source,output,1024*1024)
verify(archive,record['archive_bytes'],record['archive_sha256'])
expected={row['path']:row for row in record['files']}
with zipfile.ZipFile(archive) as z:
    if len(z.namelist())!=len(set(z.namelist())) or set(z.namelist())!=set(expected)|{'baseline/r354-portable.json'}:
        raise SystemExit('Unexpected archive file set')
    staging.mkdir(parents=True)
    for name,row in expected.items():
        if not name.startswith(('.local/baseline/r354/','.local/toolchain/')):
            raise SystemExit('Unexpected baseline destination')
        item=z.getinfo(name)
        if item.file_size!=row['bytes'] or stat.S_ISLNK(item.external_attr>>16):
            raise SystemExit('Unexpected archive entry')
        target=checked_path(staging,name)
        target.parent.mkdir(parents=True,exist_ok=True)
        with z.open(item) as source,target.open('xb') as output:
            shutil.copyfileobj(source,output,1024*1024)
        verify(target,row['bytes'],row['sha256'])
    portable=json.loads(z.read('baseline/r354-portable.json'))
    if portable['files']!=record['files']: raise SystemExit('Portable manifest differs')
base.parent.mkdir(parents=True,exist_ok=True)
os.rename(staging/'.local/baseline/r354',base)
os.rename(staging/'.local/toolchain',toolchain)
for directory in (base,toolchain):
    for file in directory.rglob('*'):
        if file.is_file(): file.chmod(stat.S_IREAD)
(base.parent/'r354-restored.json').write_text(json.dumps({'archive_sha256':record['archive_sha256']})+'\n')
# Remove only the known empty staging directories; never recursive cleanup.
(staging/'.local/baseline').rmdir()
(staging/'.local').rmdir()
staging.rmdir()
print(f'SONIC_BASELINE_RESTORED files={len(expected)} personal_saves=excluded')
