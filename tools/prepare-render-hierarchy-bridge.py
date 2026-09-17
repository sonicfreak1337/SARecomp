"""Private original continuations and the sole public hierarchy admission."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
spec=importlib.util.spec_from_file_location('hierarchy_author',Path(__file__).with_name('prepare-render-hierarchy.py'))
author=importlib.util.module_from_spec(spec);spec.loader.exec_module(author)
def sha(data):return hashlib.sha256(data).hexdigest()
UNITS={'unit-v8C038802-8C03FF90-0329df60636b0242.cpp': '6e712b4cd2dc88ca28782efc79268ac7a49edac77bcc7cb1d4d38b31d66f4e4b', 'unit-v8C0400A0-8C04124E-c3a8c709f8ba2806.cpp': '9a8b8e51de23c7982413a8d2d15ba783be133a0b77a406d656a2769da510e785', 'unit-v8C638FF0-8C639E9C-df982d963eeb3342.cpp': '79ecc1ebbdf3e06c517d5edcc42850c08eb50c7dfe6fe4359c59b0e626472558', 'unit-v8C639F38-8C63B05C-e3ea66f80473641e.cpp': '8716f427e7212908572f5b03d060864aa455c94458a55157135268b2c040b852'}
ROOT_UNIT='unit-v8C0400A0-8C04124E-c3a8c709f8ba2806.cpp'
def local_resumes(text,ram,owner):
    _,entry,begin,end=owner
    instructions,delays,calls=author.inspect(ram,entry,begin,end)
    memory=lambda line:re.search(r'\b(?:load|load16|load8|store|store16|fload|fstore)\(',line) is not None
    restart={entry}
    for pc,op in instructions.items():
        if pc+2 in delays:
            word=struct.unpack_from('<H',ram,pc+2-author.shared.BASE)[0]
            if memory(author.emit_simple(pc+2,word,ram)):restart.add(pc)
        elif not (op>>8 in (0x89,0x8B)) and memory(author.emit_simple(pc,op,ram)):
            restart.add(pc)
    restart.update(int(c['pc'],16)+4 for c in calls if not c.get('tail'))
    for pc,op in instructions.items():
        if (op>>12==0xA and pc+4+2*author.shared.signed(op&4095,12)<=pc) or (op>>8 in (0x89,0x8B,0x8D,0x8F) and pc+4+2*author.shared.signed(op&255,8)<=pc):restart.add(pc)
    # These routes remain local to this exact function invocation. The global
    # dispatcher deliberately retains its original, smaller entry table.
    outer=text.index('        switch (katana::runtime::unrelocate_code_address_inline(cpu.pc)) {')
    outer_end=text.index('        katana_block_',outer)
    existing=set(int(x,16) for x in re.findall(r'case 0x(8C[0-9A-F]{6})u:',text[outer:outer_end]))
    blocks=list(re.finditer(r'(?m)^        katana_block_(8C[0-9A-F]{6}):\n        \{\n',text))
    edits=[];routes={};outer_cases=[];proof=[]
    for pc in sorted(restart-existing):
        comment=re.search(r'(?m)^( +)// katana-guest 0x'+f'{pc:08X}'+r'u\n',text)
        if comment is None:raise ValueError(f'Missing original instruction {pc:08X}')
        index=max(i for i,b in enumerate(blocks) if b.start()<comment.start())
        block=blocks[index];stop=blocks[index+1].start() if index+1<len(blocks) else len(text)
        owner=int(block[1],16);part=text[block.start():stop]
        prefix=re.search(r'(?m)^                Memory::DirectLinearWriteBatch\* const katana_direct_ram_writes = nullptr;\n',part)
        if prefix is not None:
            router=block.start()+prefix.end()
        else:
            # Read-only tails have no write-batch declaration. Insert before
            # their retained local router, after the common exit provenance.
            existing_router=re.search(r'(?m)^                switch \(katana::runtime::unrelocate_code_address_inline\(cpu.pc\)\) \{\n',part)
            if existing_router is None:raise ValueError(f'Unsupported original block {owner:08X}')
            router=block.start()+existing_router.start()
        existing_label='katana_block_'+f'{pc:08X}'+'_resume'
        if existing_label+':' in part:
            destination=existing_label
        else:
            destination='sonic_hierarchy_resume_'+f'{pc:08X}'
            if len(comment[1])==20 and text[comment.start()-18:comment.start()]=='                {\n':
                position=comment.start()-18
            elif len(comment[1])==16:
                # Terminal instruction: enter before its target/PR capture and
                # preflight, after the previous instruction's closed scope.
                closed=list(re.finditer(r'(?m)^                \}\n',text[router:comment.start()]))
                position=router+(closed[-1].end() if closed else 0)
            else:raise ValueError(f'Unknown restart scope {pc:08X}')
            edits.append((position,'                '+destination+':\n'))
        routes.setdefault(router,[]).append(f'                case 0x{pc:08X}u: goto {destination};\n')
        outer_cases.append(f'            case 0x{pc:08X}u: goto katana_block_{owner:08X};\n')
        proof.append(dict(pc=f'{pc:08X}',block=f'{owner:08X}',label=destination))
    edits.append((text.index('\n',outer)+1,''.join(outer_cases)))
    for position,cases in routes.items():
        edits.append((position,'                switch (cpu.pc) {\n'+''.join(cases)+'                default: break;\n                }\n'))
    for position,insertion in sorted(edits,key=lambda e:e[0],reverse=True):text=text[:position]+insertion+text[position:]
    return text,proof

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for k in ('source-root','output','ram'):p.add_argument('--'+k,type=Path,required=True)
    p.add_argument('--input',type=Path)
    a=p.parse_args();ram=a.ram.read_bytes()
    if sha(ram)!=author.shared.RAM_SHA:raise ValueError('Original PAL identity')
    lines=(a.source_root/'.katana-generated-artifacts').read_text().splitlines()
    generation='generation\tsha256:'+sha(('\n'.join(lines[2:])+'\n').encode())
    if lines[:2]!=['katana-codegen-artifacts-v2',generation]:raise ValueError('Manifest identity')
    records={v[0]:v for line in lines[2:] if len(v:=line.split('\t'))>=3}
    texts={}
    for name,digest in UNITS.items():
        data=(a.source_root/'code'/name).read_bytes()
        if sha(data)!=digest or records.get('code/'+name,[])[1:3]!=[str(len(data)),'sha256:'+digest]:raise ValueError('Original unit identity '+name)
        texts[name]=data.decode().replace('\r\n','\n')
    proof=[]
    if a.input:
        if a.input.name!=ROOT_UNIT:raise ValueError('Unexpected hierarchy hook unit')
        data=a.input.read_bytes();original=(a.source_root/'code'/ROOT_UNIT).read_bytes()
        if data!=original:
            if a.input.parent.name not in ('region-writes','region-extended-writes'):raise ValueError('Unreviewed active input')
            prior=json.loads((a.input.parent/'preparation.json').read_text());rows=[e for e in prior['units'] if e['unit']==ROOT_UNIT]
            if (prior['generation']!=generation or prior['mode']!='region' or prior.get('guard_probe',False) or prior.get('extended_regions',False)!=(a.input.parent.name=='region-extended-writes') or len(rows)!=1 or rows[0]['source_sha256']!=sha(original) or rows[0]['output_sha256']!=sha(data)):raise ValueError('RAM provenance')
        out=data.decode().replace('\r\n','\n')
        start=out.index('BlockExit fn_8C040784_runtime_entry(CpuState& cpu, BlockExecutionContext& context) {')
        boundary=out.index('    static_cast<void>(services);',start)
        injection="""    if (sonic::render_hierarchy::enabled() && cpu.pc == sonic::render_hierarchy::entry) {
        const auto outcome=sonic::render_hierarchy::try_dispatch(cpu,*services);
        if (outcome != sonic::render_hierarchy::Outcome::Declined) {
            const bool complete=outcome==sonic::render_hierarchy::Outcome::Complete;
            const auto source=complete?sonic::render_hierarchy::return_site:cpu.pc;
            runtime_dispatch_detail::active_exit_source={source,source&0x1FFFFFFFu};
            runtime_dispatch_detail::active_exit_kind=complete?katana::runtime::BlockEndKind::Return:katana::runtime::BlockEndKind::Fallthrough;
            runtime_dispatch_detail::active_exit_site_class=katana::runtime::DynamicDispatchSiteClass::NotDynamic;
            return;
        }
    }
"""
        out=out[:boundary]+injection+out[boundary:]
        out='#include "sonic_render_hierarchy.hpp"\n'+out.replace('#include "../include/','#include "')
    else:
        owners={o[1]:o for o in author.OWNERS};definitions={};prefix=None
        pattern=re.compile(r'^BlockExit fn_(8C[0-9A-F]{6})_runtime_entry\(CpuState& cpu, BlockExecutionContext& context\) \{',re.M)
        for name,text in texts.items():
            if prefix is None:prefix=text[:text.index('BlockExit fn_')]
            for match in pattern.finditer(text):
                entry=int(match[1],16)
                if entry not in owners:continue
                close='    return exit;\n}';end=text.index(close,match.end())+len(close)
                body,resumes=local_resumes(text[match.start():end],ram,owners[entry])
                definitions[entry]=body;proof.append(dict(owner=f'{entry:08X}',unit=name,local_resumes=resumes))
        if set(definitions)!=set(owners):raise ValueError('Incomplete private hierarchy owner set')
        # Internal original calls remain original, without any native hook.
        out=prefix+'\n'.join(definitions.values())
        for entry in owners:out=out.replace(f'fn_{entry:08X}_runtime_entry',f'hierarchy_original_{entry:08X}')
        declarations='\n'.join(f'BlockExit hierarchy_original_{e:08X}(CpuState&,BlockExecutionContext&);' for e in owners)
        declarations+='\nBlockExit fn_8C037098_runtime_entry(CpuState&,BlockExecutionContext&);'
        out=out.replace(prefix,prefix+declarations+'\n',1)+'\n}\n'
        out='#include "sonic_render_hierarchy.hpp"\n'+out
        out+='namespace sonic::render_hierarchy {\nbool resume_original(katana::runtime::CpuState& cpu,std::uint32_t owner) {\nkatana::runtime::BlockExecutionContext block;\nswitch(owner) {\n'
        for entry in owners:out+=f'case 0x{entry:08X}u:katana_port_generated::hierarchy_original_{entry:08X}(cpu,block);return true;\n'
        out+='default:return false;\n}\n}\n}\n'
    a.output.parent.mkdir(parents=True,exist_ok=True);data=out.encode()
    if not a.output.exists() or a.output.read_bytes()!=data:a.output.write_bytes(data)
    a.output.with_suffix('.json').write_text(json.dumps(dict(generation=generation,original_units=UNITS,output_sha256=sha(data),owners=proof),indent=2)+'\n')
    print(f'SONIC_RENDER_HIERARCHY_BRIDGE_READY private_owners={len(proof)} hook={int(a.input is not None)}')
if __name__=='__main__':main()
