"""Bind the complete movement owner while retaining every original resume entry."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import importlib.util

UNIT='unit-v8C073018-8C073018-457a60bdbd02a56b.cpp'
SOURCE_SHA='f18e090b5b8b0f6899c122dc28234f7226464391c7e12339b702d3e9c30caf92'
def sha(data):return hashlib.sha256(data).hexdigest()

def local_resumes(text,ram):
    spec=importlib.util.spec_from_file_location('movement_author',Path(__file__).with_name('prepare-movement-resolver.py'))
    movement=importlib.util.module_from_spec(spec);spec.loader.exec_module(movement)
    instructions,delays,_=movement.shared.inspect(ram,movement.BODY,movement.END)
    memory=lambda line:re.search(r'\b(?:load|load16|load8|store|store16)\(',line) is not None
    restart={0x8C073018,0x8C07301E,0x8C073020,0x8C073022,0x8C073044,0x8C073046,
             0x8C073048,0x8C07304E,0x8C073052,0x8C073054,0x8C073056,0x8C07305A}
    for pc,op in instructions.items():
        if pc+2 in delays:
            word=struct.unpack_from('<H',ram,pc+2-movement.shared.BASE)[0]
            if memory(movement.emit_simple(pc+2,word,ram)):restart.add(pc)
        elif not (op>>8 in (0x89,0x8B)) and memory(movement.emit_simple(pc,op,ram)):
            restart.add(pc)
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
            destination='sonic_movement_resume_'+f'{pc:08X}'
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
    p.add_argument('--source-root',type=Path,required=True)
    p.add_argument('--input',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--ram',type=Path,required=True)
    a=p.parse_args()
    if a.input.name!=UNIT:raise ValueError('Unexpected movement unit')
    original=(a.source_root/'code'/UNIT).read_bytes()
    if sha(original)!=SOURCE_SHA:raise ValueError('Movement owner audit identity')
    lines=(a.source_root/'.katana-generated-artifacts').read_text().splitlines()
    generation='generation\tsha256:'+sha(('\n'.join(lines[2:])+'\n').encode())
    if lines[:2]!=['katana-codegen-artifacts-v2',generation]:raise ValueError('Retained manifest identity')
    records={r[0]:r for line in lines[2:] if len(r:=line.split('\t'))>=3}
    if records.get('code/'+UNIT,[])[1:3]!=[str(len(original)),'sha256:'+sha(original)]:raise ValueError('Retained unit identity')
    data=a.input.read_bytes()
    if data!=original:
        if a.input.parent.name not in ('region-writes','region-extended-writes'):raise ValueError('Unreviewed input preparation')
        prior=json.loads((a.input.parent/'preparation.json').read_text())
        rows=[e for e in prior['units'] if e['unit']==UNIT]
        if (prior['generation']!=generation or prior['mode']!='region' or prior.get('guard_probe',False) or
            prior.get('extended_regions',False)!=(a.input.parent.name=='region-extended-writes') or len(rows)!=1 or
            rows[0]['source_sha256']!=sha(original) or rows[0]['output_sha256']!=sha(data)):
            raise ValueError('RAM region provenance')
    text=data.decode().replace('\r\n','\n')
    ram=a.ram.read_bytes()
    if sha(ram)!='b64a98597751d995aa95346df260d79efb38deb37bd174efa01c8d732645846c':raise ValueError('Original RAM identity')
    text,resumes=local_resumes(text,ram)
    start=text.index('BlockExit fn_8C073018_runtime_entry(CpuState& cpu, BlockExecutionContext& context) {')
    boundary=text.index('    static_cast<void>(services);',start)
    injection="""    if (sonic::movement::enabled()) {
        const auto outcome = sonic::movement::try_dispatch(cpu, *services);
        if (outcome != sonic::movement::Outcome::Declined && outcome != sonic::movement::Outcome::ResumeOriginal) {
            const bool complete = outcome == sonic::movement::Outcome::Complete;
            const auto source = complete ? sonic::movement::return_site : cpu.pc;
            runtime_dispatch_detail::active_exit_source = {source, source & 0x1FFFFFFFu};
            runtime_dispatch_detail::active_exit_kind = complete
                ? katana::runtime::BlockEndKind::Return : katana::runtime::BlockEndKind::Fallthrough;
            runtime_dispatch_detail::active_exit_site_class = katana::runtime::DynamicDispatchSiteClass::NotDynamic;
            // Stop/exception exits retain their normal dispatcher behavior.
            return;
        }
        // ResumeOriginal enters the original instruction scopes below in this
        // SAME invocation. Do not dispatch unregistered inner instruction PCs.
    }
"""
    text=text[:boundary]+injection+text[boundary:]
    out=('#include "sonic_movement_resolver.hpp"\n'+text.replace('#include "../include/','#include "')).encode()
    a.output.parent.mkdir(parents=True,exist_ok=True)
    if not a.output.exists() or a.output.read_bytes()!=out:a.output.write_bytes(out)
    a.output.with_suffix('.json').write_text(json.dumps(dict(owner='8C073018',generation=generation,
        input_sha256=sha(data),original_sha256=sha(original),output_sha256=sha(out),retained_resumes=True,
        local_resumes=resumes),indent=2)+'\n')
    print('SONIC_MOVEMENT_BRIDGE_READY retained_resumes=1')
if __name__=='__main__':main()
