"""Private retained SDK continuations; no new public dispatcher entries/hooks."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

spec=importlib.util.spec_from_file_location('world_bridge',Path(__file__).with_name('prepare-collision-world-bridge.py'))
bridge=importlib.util.module_from_spec(spec);spec.loader.exec_module(bridge)
UNITS={
 'unit-v8C638480-8C638FD0-66c87795706b359b.cpp':'fb758d61313de5a84f38981f39b2efdb863cbacdf5dd441a92a062c05fe6d4c2',
 'unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp':'79ecc1ebbdf3e06c517d5edcc42850c08eb50c7dfe6fe4359c59b0e626472558',
 'unit-v8C639F38-8C63B05C-e3ea66f80473641e.cpp':'8716f427e7212908572f5b03d060864aa455c94458a55157135268b2c040b852',
}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('source-root','output','ram'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();ram=a.ram.read_bytes()
    if bridge.sha(ram)!=bridge.world.shared.RAM_SHA:raise ValueError('Original RAM identity')
    lines=(a.source_root/'.katana-generated-artifacts').read_text().splitlines()
    generation='generation\tsha256:'+bridge.sha(('\n'.join(lines[2:])+'\n').encode())
    if lines[:2]!=['katana-codegen-artifacts-v2',generation]:raise ValueError('Manifest identity')
    records={r[0]:r for line in lines[2:] if len(r:=line.split('\t'))>=3}
    owners={o[1]:o for o in bridge.world.SDK_OWNERS};definitions={};proof=[];prefix=None
    pattern=re.compile(r'^BlockExit fn_(8C[0-9A-F]{6})_runtime_entry\(CpuState& cpu, BlockExecutionContext& context\) \{',re.M)
    for name,digest in UNITS.items():
        data=(a.source_root/'code'/name).read_bytes()
        if bridge.sha(data)!=digest or records.get('code/'+name,[])[1:3]!=[str(len(data)),'sha256:'+digest]:
            raise ValueError('Retained SDK unit identity: '+name)
        text=data.decode().replace('\r\n','\n')
        if prefix is None:prefix=text[:text.index('BlockExit fn_')]
        for match in pattern.finditer(text):
            entry=int(match[1],16)
            if entry not in owners:continue
            closing='    return exit;\n}'
            end=text.index(closing,match.end())+len(closing)
            body,resumes=bridge.local_resumes(text[match.start():end],ram,owners[entry])
            # The original definitions remain in their normal archive. This
            # private copy only resumes an already active native SDK operation.
            body=body.replace(f'fn_{entry:08X}_runtime_entry',f'world_sdk_{entry:08X}_resume')
            definitions[entry]=body
            proof.append(dict(owner=f'{entry:08X}',unit=name,local_resumes=resumes))
    if set(definitions)!=set(owners):raise ValueError('Incomplete SDK closure')
    out='#include "sonic_collision_world.hpp"\n'+prefix+'\n'.join(definitions.values())+'\n}\n'
    out+='namespace sonic::collision_world {\nbool resume_sdk(katana::runtime::CpuState& cpu,std::uint32_t owner) {\nkatana::runtime::BlockExecutionContext block;\nswitch(owner) {\n'
    for entry in sorted(owners):out+=f'case 0x{entry:08X}u:katana_port_generated::world_sdk_{entry:08X}_resume(cpu,block);return true;\n'
    out+='default:return false;\n}\n}\n}\n'
    a.output.parent.mkdir(parents=True,exist_ok=True)
    data=out.encode()
    if not a.output.exists() or a.output.read_bytes()!=data:a.output.write_bytes(data)
    a.output.with_suffix('.json').write_text(json.dumps(dict(generation=generation,original_units=UNITS,output_sha256=bridge.sha(data),owners=proof),indent=2)+'\n')
    print(f'SONIC_WORLD_SDK_BRIDGE_READY owners={len(owners)} resumes={sum(len(x["local_resumes"]) for x in proof)}')
if __name__=='__main__':main()
