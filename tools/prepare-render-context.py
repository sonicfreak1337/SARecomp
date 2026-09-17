"""Authenticate complete PAL render-context capture/commit owners."""
import argparse
import hashlib
from pathlib import Path
import prepare_collision_candidates as shared

RANGES=(
 (0x8C605CEC,0x5E,'37c2ea8f4df88bd1e65f46e007269e1c2d9af7a218937c21b8fa2fa32d789671'),
 (0x8C605D4A,0x5E,'a1dab4652c258acc3162d1307f65a1f93e1e1558ab41225deb5bfb2bec58bc55'),
 (0x8C605DF4,0x14,'f17bee4c6aaeb9f0615037ce65ab5e773e0503273d71f727b296aa4396b1bf99'),
)
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--ram',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();ram=a.ram.read_bytes()
if hashlib.sha256(ram).hexdigest()!=shared.RAM_SHA:raise ValueError('PAL RAM identity')
for addr,size,digest in RANGES:
 if hashlib.sha256(ram[addr-shared.BASE:addr-shared.BASE+size]).hexdigest()!=digest:
  raise ValueError(f'Render-context identity: {addr:08X}')
text=shared.emit_identities(ram,[(addr,size) for addr,size,_ in RANGES])
a.output.parent.mkdir(parents=True,exist_ok=True)
if not a.output.exists() or a.output.read_text()!=text:a.output.write_text(text,encoding='ascii',newline='\n')
print('SONIC_RENDER_CONTEXT_IDENTITIES_READY owners=2')
