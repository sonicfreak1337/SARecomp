"""Create a LOCAL development backup. Contains installed retail data: never upload."""
import hashlib
import json
from pathlib import Path
import re
import time
import zipfile

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root/'baseline/r354.json').read_text())
devtools = json.loads((root/'baseline/dev-tools.json').read_text(encoding='utf-8-sig'))
rows = [row for row in manifest['files'] if not (
    row['path'].startswith('.local/baseline/r354/saves/') or
    row['path'].startswith('.local/baseline/r354/product/user-data/'))] + devtools['files']
for row in rows:
    name = Path(row['path']).name.lower()
    if Path(name).suffix in ('.gdi','.cdi','.chd','.iso') or re.fullmatch(r'track\d+\.(bin|raw)',name):
        raise SystemExit('Original disc image in archive selection')
output = root/'.local/archives'
output.mkdir(parents=True,exist_ok=True)
archive = output/'r354-development.zip'
if archive.exists(): raise SystemExit('Preserve the existing archive; do not overwrite')
last = time.monotonic()
with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=1,allowZip64=True) as z:
    for index,row in enumerate(rows):
        z.write(root/row['path'],row['path'])
        if time.monotonic()-last>5:
            print(f'SONIC_ARCHIVE files={index+1}/{len(rows)} bytes={archive.stat().st_size}',flush=True)
            last=time.monotonic()
    z.writestr('baseline/r354-portable.json',json.dumps({'schema':'sarecomp-private-bundle-v1','files':rows},indent=2))
parts=[]
with archive.open('rb') as stream:
    index=1
    while data:=stream.read(256*1024*1024):
        part=output/f'r354-development.zip.part{index:03}'
        with part.open('xb') as dst: dst.write(data)
        parts.append({'name':part.name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
        index+=1
with archive.open('rb') as stream: digest=hashlib.file_digest(stream,'sha256').hexdigest()
record={'schema':'sarecomp-private-release-bundle-v1','tag':'r354-baseline','archive_sha256':digest,
        'archive_bytes':archive.stat().st_size,'parts':parts,'files':rows}
(root/'baseline/development-bundle.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(f'SONIC_ARCHIVE_OK bytes={archive.stat().st_size} parts={len(parts)} sha256={digest}',flush=True)
