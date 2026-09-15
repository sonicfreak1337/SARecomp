"""Stage the accepted candidate's portable C++ inputs, never Windows objects.

The private retained input pack is shared with incremental Windows builds.
Every staged file is recorded by digest so the Linux build is reviewable.
"""
from pathlib import Path
import hashlib
import json
import sys

root, output = map(Path, sys.argv[1:])
source=root/'build-performance/generated'
# Refresh the small bridge from its authenticated input when Linux is built
# first. Never wait for a Windows compile or silently reuse an older policy.
import subprocess
subprocess.run([sys.executable,str(root/'tools/prepare-motion-dispatch.py'),
    str(root/'.local/working-product/generated/code/native-port-dispatch.cpp'),
    str(source/'motion-dispatch/native-port-dispatch.cpp')],check=True)
groups=('audio-buses','collision-candidates','motion-sampling','fpu-body',
        'fpu-calls-inverse','inverse-arithmetic','fpu-runtime','minicart','motion-dispatch')
files=[]
for group in groups:
    files.extend(p for p in (source/group).iterdir() if p.suffix in ('.cpp','.hpp','.inc','.json') and 'reference' not in p.name)
files.extend(source/p for p in ('native_provider_identity.hpp','native_latent_texture_dispatch_provider_identity.hpp','native_spg_status_provider_identity.hpp'))
if len(files)<25: raise RuntimeError('Current candidate source pack is incomplete')
records=[]
for path in sorted(files):
    relative=path.relative_to(source);data=path.read_bytes();target=output/relative
    target.parent.mkdir(parents=True,exist_ok=True)
    if not target.exists() or target.read_bytes()!=data:target.write_bytes(data)
    records.append({'file':relative.as_posix(),'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
manifest={'candidate':'57210a04d58d1d6efa71d6be46135ca335e27236','kind':'retained-cxx-inputs-only',
          'fpu_calls':'inverse','fpu_runtime':'fast','files':records}
report=json.dumps(manifest,indent=2)+'\n';target=output/'linux-title-inputs.json'
if not target.exists() or target.read_text()!=report:target.write_text(report)
print('SONIC_LINUX_TITLE_INPUTS files='+str(len(records))+' windows_objects=0 analysis=0')
