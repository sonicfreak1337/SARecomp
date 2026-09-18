"""Author one closed movement/contact family from authenticated PAL sources.

The reviewed render-family author supplies instruction and local-continuation
mechanics only. No generated code or mutable data is shared with that family.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import prepare_collision_candidates as shared

def module(name, file):
    spec=importlib.util.spec_from_file_location(name,Path(__file__).with_name(file))
    out=importlib.util.module_from_spec(spec);spec.loader.exec_module(out);return out
render=module('contact_instruction_author','prepare-render-hierarchy.py')
ROWS=json.loads(Path(__file__).with_name('movement-contact-owners.json').read_text())
OBJECT_ROWS=json.loads(Path(__file__).with_name('object-contact-owners.json').read_text())
OBJECT_ENTRIES={r['entry'] for r in OBJECT_ROWS}
if OBJECT_ENTRIES.intersection(r['entry'] for r in ROWS):raise ValueError('Duplicate contact owner')
ROWS+=OBJECT_ROWS
CAMERA_ROWS=json.loads(Path(__file__).with_name('camera-operation-owners.json').read_text())
CAMERA_ENTRIES={r['entry'] for r in CAMERA_ROWS}
if CAMERA_ENTRIES.intersection(r['entry'] for r in ROWS):raise ValueError('Duplicate camera owner')
ROWS+=CAMERA_ROWS
OWNERS=tuple((f'owner_{r["entry"]:08X}',r['entry'],r['begin'],r['end']) for r in ROWS)
EPOCHS={r['entry']:tuple(map(tuple,r['epochs'])) for r in ROWS}
UNITS={r['unit']:r['unit_sha'] for r in ROWS}
inspect=render.inspect
original_emit=render.emit_simple

def emit_simple(pc,op,ram,restart=None):
    at=restart or f'RestartPoint{{0x{pc:08X}u}}'
    if op>>8==0x80:return f'CONTACT_SITE(0x{pc:08X}u);store8({at},cpu.r[{(op>>4)&15}]+{op&15}u,std::uint8_t(cpu.r[0]),CodeWriteSource::Cpu);'
    if op&0xF00F==0x6007:return f'CONTACT_SITE(0x{pc:08X}u);cpu.r[{(op>>8)&15}]=~cpu.r[{(op>>4)&15}];'
    if op&0xF0FF==0x4021:
        r=f'cpu.r[{(op>>8)&15}]'
        return f'CONTACT_SITE(0x{pc:08X}u);cpu.t=({r}&1u)!=0u;{r}=({r}>>1u)|({r}&0x80000000u);'
    if op&0xF00F==0x000C:return f'CONTACT_SITE(0x{pc:08X}u);cpu.r[{(op>>8)&15}]=signed8(load8({at},cpu.r[0]+cpu.r[{(op>>4)&15}]));'
    if op&0xF0FF==0xF00D:return f'CONTACT_SITE(0x{pc:08X}u);cpu.fr[{(op>>8)&15}]=cpu.fpul;'
    if op&0xF0FF==0xF01D:return f'CONTACT_SITE(0x{pc:08X}u);cpu.fpul=cpu.fr[{(op>>8)&15}];'
    return original_emit(pc,op,ram,restart).replace('HIERARCHY_SITE','CONTACT_SITE')
render.emit_simple=emit_simple
render.EPOCHS=EPOCHS

def originals(root):
    sha=lambda b:hashlib.sha256(b).hexdigest()
    lines=(root/'.katana-generated-artifacts').read_text().splitlines()
    generation='generation\tsha256:'+sha(('\n'.join(lines[2:])+'\n').encode())
    if lines[:2]!=['katana-codegen-artifacts-v2',generation]:raise ValueError('Retained manifest identity')
    records={v[0]:v for line in lines[2:] if len(v:=line.split('\t'))>=3}
    texts={}
    for name,digest in UNITS.items():
        data=(root/'code'/name).read_bytes()
        if sha(data)!=digest or records.get('code/'+name,[])[1:3]!=[str(len(data)),'sha256:'+digest]:raise ValueError('Original source changed '+name)
        texts[name]=data.decode().replace('\r\n','\n')
    return generation,texts

def merge(spans):
    out=[]
    for address,size in sorted(spans):
        if out and address<=sum(out[-1]):
            a,n=out[-1];out[-1]=(a,max(a+n,address+size)-a)
        else:out.append((address,size))
    return out

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for k in ('ram','output','source-root'):p.add_argument('--'+k,type=Path,required=True)
    args=p.parse_args();ram=args.ram.read_bytes()
    if hashlib.sha256(ram).hexdigest()!=shared.RAM_SHA:raise ValueError('Original RAM identity')
    generation,texts=originals(args.source_root)
    bridge=module('contact_epoch_verifier','prepare-render-hierarchy-bridge.py')
    bridge.author.EPOCHS=EPOCHS
    args.output.mkdir(parents=True,exist_ok=True)
    deps={};reports=[]
    for row,(name,entry,begin,end) in zip(ROWS,OWNERS):
        text=texts[row['unit']]
        start=text.index(f'BlockExit fn_{entry:08X}_runtime_entry(CpuState& cpu, BlockExecutionContext& context) {{')
        stop=text.index('    return exit;\n}',start)+len('    return exit;\n}')
        bridge.verify_original_epochs(text[start:stop],entry)
        ins,delays,calls=inspect(ram,entry,begin,end)
        used=dict(ins);used.update({pc:struct.unpack_from('<H',ram,pc-shared.BASE)[0] for pc in delays})
        literals=set()
        for pc,op in used.items():
            if op>>12==0xD:literals.add((((pc+4)&~3)+(op&255)*4,4))
            if op>>12==9:literals.add((pc+4+(op&255)*2,2))
            if op>>8==0xC7:literals.add((((pc+4)&~3)+(op&255)*4,4))
        deps[name]=merge([(begin,end-begin),*literals])
        body=render.emit_body(ram,entry,ins).replace('HIERARCHY_SITE','CONTACT_SITE')
        for call in calls:
            if call.get('tail'):
                pc=int(call['pc'],16)
                marker=f'L{pc:08X}:';at=body.index(marker)
                expected=f'call(target,true,0x{pc:08X}u);return_site=0x{pc:08X}u;return;'
                if expected not in body[at:]:raise ValueError('Missing original tail provenance')
        if entry==0x8C10CD1C:
            # This complete record-copy operation includes all short-copy
            # jump-table tails. Its source span authenticates those bytes too.
            if begin!=0x8C10CD1C or end!=0x8C10CDDC:raise ValueError('Copy closure boundary')
            body='if(a.copy_contact_record())return; a.restart(0x8C10CD1Cu);\n'
        (args.output/f'contact-{name}.inc').write_text(body,encoding='ascii',newline='\n')
        reports.append(dict(name=name,entry=f'{entry:08X}',begin=f'{begin:08X}',end=f'{end:08X}',instructions=len(ins),delays=len(delays),calls=calls))
    merged=merge([s for spans in deps.values() for s in spans])
    proof=[shared.emit_identities(ram,merged)]
    for name,entry,_,_ in OWNERS:
        proof.append(f'constexpr std::array<SourceSpan,{len(deps[name])}> source_{name}{{{{')
        for address,size in deps[name]:
            match=[(i,a) for i,(a,n) in enumerate(merged) if a<=address and address+size<=a+n]
            if len(match)!=1:raise ValueError('Unbound owner dependency')
            i,a=match[0]
            proof.append(f'{{0x{address:08X}u,std::span{{identity_{i}}}.subspan({address-a}u,{size}u)}},')
        proof.append('}};')
    proof.append('constexpr std::array owner_sources{')
    proof += [f'std::span<const SourceSpan>{{source_{name}}},' for name,_,_,_ in OWNERS]
    proof.append('};\nunsigned source_owner_index(std::uint32_t owner) noexcept {switch(owner){')
    proof += [f'case 0x{entry:08X}u:return {i}u;' for i,(_,entry,_,_) in enumerate(OWNERS)]
    proof.append(f'default:return {len(OWNERS)}u;}}}}')
    proof += ['constexpr auto source_pages=[] {std::array<bool,4096> out{};',
              'for(const auto& s:identities)for(auto p=(s.address&0xFFFFFFu)>>12u;p<=((s.address&0xFFFFFFu)+s.bytes.size()-1u)>>12u;++p)out[p]=true;return out;}();']
    (args.output/'contact-identities.inc').write_text('\n'.join(proof)+'\n')
    (args.output/'contact-switch.inc').write_text('\n'.join(f'case 0x{entry:08X}u: {{\n#include "contact-{name}.inc"\n}}' for name,entry,_,_ in OWNERS)+'\n')
    members='\n'.join(f'case 0x{entry:08X}u:' for _,entry,_,_ in OWNERS if entry not in OBJECT_ENTRIES and entry not in CAMERA_ENTRIES)+'\nreturn true;\n'
    members+='\n'.join(f'case 0x{entry:08X}u:' for entry in sorted(OBJECT_ENTRIES))+'\nreturn object_selected();\n'
    members+='\n'.join(f'case 0x{entry:08X}u:' for entry in sorted(CAMERA_ENTRIES))+'\nreturn camera_selected();\n'
    (args.output/'contact-members.inc').write_text(members)
    (args.output/'contact-epochs.inc').write_text('struct OriginalEpoch {std::uint32_t begin,end;bool single;};\nconstexpr OriginalEpoch original_epochs[]{\n'+'\n'.join(f'{{0x{a:08X}u,0x{b:08X}u,true}},' for es in EPOCHS.values() for a,b in es)+'\n};\n')
    (args.output/'contact-inventory.json').write_text(json.dumps(dict(schema='sarecomp-movement-contact-v1',generation=generation,owners=reports,units=UNITS),indent=2)+'\n')
    print(f'SONIC_MOVEMENT_CONTACT_READY owners={len(OWNERS)} instructions={sum(r["instructions"] for r in reports)}')
if __name__=='__main__':main()
