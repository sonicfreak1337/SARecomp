"""Keep original private fault continuations for the closed contact family."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

def module(name,file):
    spec=importlib.util.spec_from_file_location(name,Path(__file__).with_name(file))
    out=importlib.util.module_from_spec(spec);spec.loader.exec_module(out);return out
source=module('contact_author','prepare-movement-contact.py')
resumes=module('contact_continuations','prepare-render-hierarchy-bridge.py')
resumes.author=source
ROOTS={'unit-v8C073018-8C073018-457a60bdbd02a56b.cpp':0x8C073018,
       'unit-v8C074214-8C075154-1bc56e9d4bd882fb.cpp':0x8C074214,
       'unit-v8C033122-8C0342E0-7edcb8468a2b4534.cpp':0x8C0342E0}
sha=lambda data:hashlib.sha256(data).hexdigest()

def readonly_resume_routers(text):
    # Some SDK polynomial blocks have neither stores nor a pre-existing local
    # router. Give the private continuation author an empty routing point after
    # exit provenance, before any original instruction or arithmetic scope.
    blocks=list(re.finditer(r'(?m)^        katana_block_(8C[0-9A-F]{6}):\n        \{\n',text))
    marker='                runtime_dispatch_detail::active_exit_site_class = katana::runtime::DynamicDispatchSiteClass::NotDynamic;\n'
    for i in range(len(blocks)-1,-1,-1):
        a=blocks[i].end();b=blocks[i+1].start() if i+1<len(blocks) else len(text)
        part=text[a:b]
        if 'Memory::DirectLinearWriteBatch* const katana_direct_ram_writes' in part or 'switch (katana::runtime::unrelocate_code_address_inline(cpu.pc))' in part:continue
        if marker not in part:continue
        at=a+part.index(marker)+len(marker)
        text=text[:at]+'                switch (katana::runtime::unrelocate_code_address_inline(cpu.pc)) {\n                default: break;\n                }\n'+text[at:]
    return text

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for k in ('source-root','output','ram'):p.add_argument('--'+k,type=Path,required=True)
    p.add_argument('--input',type=Path);a=p.parse_args()
    ram=a.ram.read_bytes()
    if sha(ram)!=source.shared.RAM_SHA:raise ValueError('Original PAL RAM identity')
    generation,texts=source.originals(a.source_root)
    proof=[]
    if a.input:
        if a.input.name not in ROOTS:raise ValueError('Unexpected public root')
        root=ROOTS[a.input.name];data=a.input.read_bytes();original=(a.source_root/'code'/a.input.name).read_bytes()
        if data!=original:
            if a.input.parent.name=='movement-bridge':
                prior=json.loads(a.input.with_suffix('.json').read_text())
                if root!=0x8C073018 or prior['generation']!=generation or prior['original_sha256']!=sha(original) or prior['output_sha256']!=sha(data):raise ValueError('Movement bridge provenance')
            else:
                if a.input.parent.name not in ('region-writes','region-extended-writes'):raise ValueError('Unknown input provenance')
                prior=json.loads((a.input.parent/'preparation.json').read_text());rows=[r for r in prior['units'] if r['unit']==a.input.name]
                if prior['generation']!=generation or prior['mode']!='region' or prior.get('guard_probe',False) or len(rows)!=1 or rows[0]['source_sha256']!=sha(original) or rows[0]['output_sha256']!=sha(data):raise ValueError('RAM region provenance')
        text=data.decode().replace('\r\n','\n');start=text.index(f'BlockExit fn_{root:08X}_runtime_entry(CpuState& cpu, BlockExecutionContext& context) {{')
        point=text.index('    static_cast<void>(services);',start)
        # Must precede the older isolated root hook when chaining its prepared unit.
        old=text.find('    if (sonic::movement::enabled())',start,point)
        if old>=0:point=old
        feature='object_enabled' if root==0x8C0342E0 else 'enabled'
        injection=f"""    if (sonic::movement_contact::{feature}() && cpu.pc==0x{root:08X}u) {{
        const auto outcome=sonic::movement_contact::try_dispatch(cpu,*services);
        if(outcome!=sonic::movement_contact::Outcome::Declined) {{
            const bool complete=outcome==sonic::movement_contact::Outcome::Complete;
            const auto pc=complete?sonic::movement_contact::return_site:cpu.pc;
            runtime_dispatch_detail::active_exit_source={{pc,pc&0x1FFFFFFFu}};
            runtime_dispatch_detail::active_exit_kind=complete?katana::runtime::BlockEndKind::Return:katana::runtime::BlockEndKind::Fallthrough;
            runtime_dispatch_detail::active_exit_site_class=katana::runtime::DynamicDispatchSiteClass::NotDynamic;
            return;
        }}
    }}
"""
        out='#include "sonic_movement_contact.hpp"\n'+(text[:point]+injection+text[point:]).replace('#include "../include/','#include "')
    else:
        owners={o[1]:o for o in source.OWNERS};definitions={};prefix=None;declarations=set()
        pattern=re.compile(r'^BlockExit fn_(8C[0-9A-F]{6})_runtime_entry\(CpuState& cpu, BlockExecutionContext& context\) \{',re.M)
        for name,text in texts.items():
            if prefix is None:prefix=text[:text.index('BlockExit fn_')]
            declarations.update(re.findall(r'^BlockExit fn_[^\n]+;',text,re.M))
            for match in pattern.finditer(text):
                entry=int(match[1],16)
                if entry not in owners:continue
                end=text.index('    return exit;\n}',match.end())+len('    return exit;\n}')
                body,local=resumes.local_resumes(readonly_resume_routers(text[match.start():end]),ram,owners[entry])
                definitions[entry]=body;proof.append(dict(owner=f'{entry:08X}',unit=name,resumes=local))
        if set(definitions)!=set(owners):raise ValueError('Missing private owner')
        joined='\n'.join(definitions.values())
        external=set(int(x,16) for x in re.findall(r'fn_(8C[0-9A-F]{6})_runtime_entry',joined))-set(owners)
        out=prefix+'\n'.join(sorted(declarations))+'\n'+joined+'\n}\n'
        for entry in owners:out=out.replace(f'fn_{entry:08X}_runtime_entry',f'contact_original_{entry:08X}')
        out='#include "sonic_movement_contact.hpp"\n'+out
        out+='namespace sonic::movement_contact {\nbool resume_original(katana::runtime::CpuState& cpu,std::uint32_t owner) {\nkatana::runtime::BlockExecutionContext block;\nswitch(owner){\n'
        for entry in owners:out+=f'case 0x{entry:08X}u:katana_port_generated::contact_original_{entry:08X}(cpu,block);return_site=katana_port_generated::runtime_dispatch_detail::active_exit_source.virtual_address;return true;\n'
        out+='default:return false;\n}}}\n'
        a.output.parent.mkdir(parents=True,exist_ok=True)
        (a.output.parent/'contact-test-externals.inc').write_text('\n'.join(f'BlockExit fn_{x:08X}_runtime_entry(CpuState& c,BlockExecutionContext&){{external(c,0x{x:08X}u);return {{}};}}' for x in sorted(external))+'\n')
    a.output.parent.mkdir(parents=True,exist_ok=True);data=out.encode()
    if not a.output.exists() or a.output.read_bytes()!=data:a.output.write_bytes(data)
    a.output.with_suffix('.json').write_text(json.dumps(dict(generation=generation,units=source.UNITS,output_sha256=sha(data),owners=proof),indent=2)+'\n')
    print(f'SONIC_MOVEMENT_CONTACT_BRIDGE_READY owners={len(proof)} public={int(a.input is not None)}')
if __name__=='__main__':main()
