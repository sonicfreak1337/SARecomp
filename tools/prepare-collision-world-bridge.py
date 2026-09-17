"""Bind the whole collision-world family with private original continuations."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
spec=importlib.util.spec_from_file_location('world_author',Path(__file__).with_name('prepare-collision-world.py'))
world=importlib.util.module_from_spec(spec);spec.loader.exec_module(world)
UNITS={
 'unit-v8C0273A2-8C028EC2-3c3ba866ec5c4265.cpp':('geometry','9b1f8bbff7ae1464c4a1b6dcb8380307c582dde17d7ef0069c8c9865f98ff2cd'),
 'unit-v8C02CA00-8C02DE20-1e7e2fdd391d9bd8.cpp':('pools','3fbc4ba9fe1102fabd68e54a3ea1c9ce27720e710438873dcff2df5ed5aa5e70'),
 'unit-v8C051E56-8C053338-ce4429c39e6969de.cpp':('eligibility','bd3ac13951986ce9098d8b3384a409e8bcbbdc31cf39ec9f25efdc644a0262c0'),
}
def sha(data):return hashlib.sha256(data).hexdigest()
def local_resumes(text,ram,owner):
    _,entry,begin,end=owner
    instructions,delays,calls=world.inspect(ram,entry,begin,end)
    memory=lambda line:re.search(r'\b(?:load|load16|load8|store|store16|fload|fstore)\(',line) is not None
    restart={entry}
    for pc,op in instructions.items():
        if pc+2 in delays:
            word=struct.unpack_from('<H',ram,pc+2-world.shared.BASE)[0]
            if memory(world.emit_simple(pc+2,word,ram)):restart.add(pc)
        elif not (op>>8 in (0x89,0x8B)) and memory(world.emit_simple(pc,op,ram)):
            restart.add(pc)
    restart.update(int(c['pc'],16)+4 for c in calls)
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
        if prefix is None:raise ValueError(f'Unsupported original block {owner:08X}')
        router=block.start()+prefix.end()
        existing_label='katana_block_'+f'{pc:08X}'+'_resume'
        if existing_label+':' in part:
            destination=existing_label
        else:
            destination='sonic_world_resume_'+f'{pc:08X}'
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
    for key in ('source-root','input','output','ram'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args()
    if a.input.name not in UNITS:raise ValueError('Unexpected collision-world unit')
    group,digest=UNITS[a.input.name];original=(a.source_root/'code'/a.input.name).read_bytes()
    if sha(original)!=digest:raise ValueError('Retained world unit identity')
    lines=(a.source_root/'.katana-generated-artifacts').read_text().splitlines()
    generation='generation\tsha256:'+sha(('\n'.join(lines[2:])+'\n').encode())
    if lines[:2]!=['katana-codegen-artifacts-v2',generation]:raise ValueError('Retained manifest identity')
    records={r[0]:r for line in lines[2:] if len(r:=line.split('\t'))>=3}
    if records.get('code/'+a.input.name,[])[1:3]!=[str(len(original)),'sha256:'+sha(original)]:raise ValueError('Manifest unit identity')
    data=a.input.read_bytes()
    if data!=original:
        if a.input.parent.name not in ('region-writes','region-extended-writes'):raise ValueError('Unreviewed input preparation')
        prior=json.loads((a.input.parent/'preparation.json').read_text())
        rows=[e for e in prior['units'] if e['unit']==a.input.name]
        if (prior['generation']!=generation or prior['mode']!='region' or prior.get('guard_probe',False) or
            prior.get('extended_regions',False)!=(a.input.parent.name=='region-extended-writes') or len(rows)!=1 or
            rows[0]['source_sha256']!=sha(original) or rows[0]['output_sha256']!=sha(data)):
            raise ValueError('RAM region provenance')
    ram=a.ram.read_bytes()
    if sha(ram)!=world.shared.RAM_SHA:raise ValueError('Original RAM identity')
    text=data.decode().replace('\r\n','\n')
    definitions=list(re.finditer(r'^BlockExit fn_(8C[0-9A-F]{6})_runtime_entry\(CpuState& cpu, BlockExecutionContext& context\) \{',text,re.M))
    owners={e:o for o in world.OWNERS for e in [o[1]]};proof=[];members=[]
    for i in range(len(definitions)-1,-1,-1):
        match=definitions[i];entry=int(match[1],16)
        if entry not in owners:
            closing='    return exit;\n}'
            close=text.index(closing,match.end())+len(closing)
            text=text[:match.start()]+'#ifndef SARECOMP_WORLD_AOT_TEST\n'+text[match.start():close]+'\n#endif\n'+text[close:]
            continue
        end=definitions[i+1].start() if i+1<len(definitions) else len(text)
        replacement,resumes=local_resumes(text[match.start():end],ram,owners[entry])
        if entry==world.OWNERS[0][1]:
            boundary=replacement.index('    static_cast<void>(services);')
            injection="""    if (sonic::collision_world::enabled() && cpu.pc == sonic::collision_world::entry) {
        const auto outcome = sonic::collision_world::try_dispatch(cpu, *services);
        if (outcome != sonic::collision_world::Outcome::Declined) {
            const bool complete = outcome == sonic::collision_world::Outcome::Complete;
            const auto source = complete ? sonic::collision_world::return_site : cpu.pc;
            runtime_dispatch_detail::active_exit_source = {source, source & 0x1FFFFFFFu};
            runtime_dispatch_detail::active_exit_kind = complete
                ? katana::runtime::BlockEndKind::Return : katana::runtime::BlockEndKind::Fallthrough;
            runtime_dispatch_detail::active_exit_site_class = katana::runtime::DynamicDispatchSiteClass::NotDynamic;
            return;
        }
    }
"""
            replacement=replacement[:boundary]+injection+replacement[boundary:]
        text=text[:match.start()]+replacement+text[end:]
        proof.append(dict(owner=f'{entry:08X}',local_resumes=resumes));members.append(entry)
    wrapper='\nnamespace sonic::collision_world {\nbool resume_'+group+'(katana::runtime::CpuState& cpu,std::uint32_t owner) {\n    katana::runtime::BlockExecutionContext block;\n    switch(owner) {\n'
    for entry in sorted(members):wrapper+=f'    case 0x{entry:08X}u:katana_port_generated::fn_{entry:08X}_runtime_entry(cpu,block);return true;\n'
    wrapper+='    default:return false;\n    }\n}\n}\n'
    out=('#include "sonic_collision_world.hpp"\n'+text.replace('#include "../include/','#include "')+wrapper).encode()
    a.output.parent.mkdir(parents=True,exist_ok=True)
    if not a.output.exists() or a.output.read_bytes()!=out:a.output.write_bytes(out)
    a.output.with_suffix('.json').write_text(json.dumps(dict(generation=generation,group=group,original_sha256=digest,input_sha256=sha(data),output_sha256=sha(out),owners=proof),indent=2)+'\n')
    print(f'SONIC_WORLD_BRIDGE_READY group={group} owners={len(members)} resumes={sum(len(p["local_resumes"]) for p in proof)}')
if __name__=='__main__':main()
