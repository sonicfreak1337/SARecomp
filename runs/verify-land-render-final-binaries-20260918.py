from pathlib import Path
import struct,mmap,hashlib,json
paths=[Path('build-linux/game'),Path('out/land-render-final-linux-20260918/game'),Path('out/model-submission-windows-20260918/game.exe')]
def sections(path):
 with path.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as m:
  h=struct.unpack_from('<16sHHIQQQIHHHHHH',m); rows=[struct.unpack_from('<IIQQQQIIQQ',m,h[6]+i*h[11]) for i in range(h[12])]; strings=rows[h[13]];data=m[strings[4]:strings[4]+strings[5]]
  return {data[r[0]:].split(b'\0',1)[0].decode():{'type':r[1],'flags':r[2],'address':r[3],'size':r[5],'sha256':hashlib.sha256(m[r[4]:r[4]+r[5]]).hexdigest() if r[1]!=8 else None} for r in rows if r[2]&2}
r={'allocated_sections_identical':sections(paths[0])==sections(paths[1]),'sections':len(sections(paths[0])),'files':[]}
for p in paths:
 with p.open('rb') as f:r['files'].append({'path':p.as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.file_digest(f,'sha256').hexdigest()})
Path('runs/land-render-final-executables-20260918.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));assert r['allocated_sections_identical']
