"""Route authenticated animation/pose owners to shared semantic C++ kernels."""
import argparse
import hashlib
import json
from pathlib import Path
import re

def sha(data):return hashlib.sha256(data).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--source-root',type=Path,required=True);p.add_argument('--input',type=Path,required=True)
 p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 root=a.source_root;original_path=root/'code'/a.input.name;original=original_path.read_bytes()
 lines=(root/'.katana-generated-artifacts').read_text().splitlines()
 generation='generation\tsha256:'+sha(('\n'.join(lines[2:])+'\n').encode())
 if lines[:2]!=['katana-codegen-artifacts-v2',generation]:raise ValueError('Retained manifest identity')
 records={r[0]:r for line in lines[2:] if len(r:=line.split('\t'))>=3}
 if records.get('code/'+a.input.name,[])[1:3]!=[str(len(original)),'sha256:'+sha(original)]:raise ValueError('Retained unit identity')
 old=original.decode().replace('\r\n','\n');data=a.input.read_bytes();text=data.decode().replace('\r\n','\n')
 if data!=original:
  if a.input.parent.name!='region-writes':raise ValueError('Unreviewed input preparation')
  prior=json.loads((a.input.parent/'preparation.json').read_text())
  entries=[e for e in prior['units'] if e['unit']==a.input.name]
  if (prior['generation']!=generation or prior['mode']!='region' or prior.get('guard_probe',False) or
      len(entries)!=1 or entries[0]['source_sha256']!=sha(original) or entries[0]['output_sha256']!=sha(data)):
   raise ValueError('RAM region provenance')
 owners={
  'unit-v8C056ED4-8C0585E0-d3674ae50a86c851.cpp':('8C057B00','8C05801A','animation_hierarchy','61216dd422e5f672dcbb90786b3dd58d293b4aac20794a0b63bf35cefbcdfc5a'),
  'unit-v8C0412C8-8C0425A0-1c2be1678b040d69.cpp':('8C0417C8','8C041968','pose_blend','68da4ac5973bbb6e33b7ecb4e4066f2ba9fb90b9da2d3601cc57dd49e2c80b6b'),
 }
 owner,return_pc,family,owner_sha=owners[a.input.name]
 pattern=r'(?m)^BlockExit fn_'+owner+r'_runtime_entry\(CpuState& cpu, BlockExecutionContext& context\) \{\n'
 start=re.search(pattern,old).start();end=old.index('\nBlockExit fn_',start+1)+1
 if sha(old[start:end].encode())!=owner_sha:raise ValueError('Owner audit identity')
 start=re.search(pattern,text).start();boundary=text.index('    static_cast<void>(services);',start)
 injection='''    if (sonic::animation_hierarchy::try_dispatch(cpu, services->immutable_write_guard())) {
        runtime_dispatch_detail::active_exit_source = {0x8C05801Au, 0x0C05801Au};
        runtime_dispatch_detail::active_exit_kind = katana::runtime::BlockEndKind::Return;
        runtime_dispatch_detail::active_exit_site_class = katana::runtime::DynamicDispatchSiteClass::NotDynamic;
        return;
    }
'''
 injection=injection.replace('animation_hierarchy',family).replace('8C05801A',return_pc).replace('0C05801A',f'{int(return_pc,16)&0x1FFFFFFF:08X}')
 text=text[:boundary]+injection+text[boundary:]
 text='#include "sonic_'+family+'.hpp"\n'+text
 text=text.replace('#include "../include/','#include "')
 a.output.parent.mkdir(parents=True,exist_ok=True);out=text.encode()
 if not a.output.exists() or a.output.read_bytes()!=out:a.output.write_bytes(out)
 report={'owner':owner,'generation':generation,'input_sha256':sha(data),'original_sha256':sha(original),'output_sha256':sha(out)}
 a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
 print('SONIC_ANIMATION_BRIDGE_READY owner='+owner+' retained_fallback=1')
if __name__=='__main__':main()
