"""Author the complete PAL render/motion hierarchy, preserving live callbacks."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import prepare_collision_candidates as shared
import prepare_motion_sampling as motion

spec=importlib.util.spec_from_file_location('render_world',Path(__file__).with_name('prepare-collision-world.py'))
world=importlib.util.module_from_spec(spec);spec.loader.exec_module(world)
ENTRY=0x8C040784
OWNERS=(
 ('hierarchy',ENTRY,ENTRY,0x8C040830),
 ('static_position',0x8C04057A,0x8C04057A,0x8C040588),
 ('static_zyx',0x8C040588,0x8C040588,0x8C040596),
 ('static_yxz',0x8C040596,0x8C040596,0x8C0405A4),
 ('static_scale',0x8C0405A4,0x8C0405A4,0x8C0405B2),
 ('srt_static',0x8C0405B2,0x8C0405B2,0x8C0405D2),
 ('srt_single',0x8C0405D2,0x8C0405D2,0x8C040612),
 ('srt_double',0x8C040612,0x8C040612,0x8C040652),
 ('srt_triple',0x8C0406A0,0x8C0406A0,0x8C0406E0),
 ('srt_quad',0x8C0406E0,0x8C0406E0,0x8C040720),
)+tuple((name.replace('-','_'),entry,entry,entry+size) for name,entry,size,_ in motion.OWNERS)+(
 ('push',0x8C639BB0,0x8C639BB0,0x8C639C30),
 ('pop',0x8C639AD8,0x8C639AD8,0x8C639B18),
 ('translate_register',0x8C63A7B8,0x8C63A7B8,0x8C63A81A),
 ('scale_register',0x8C63A5DC,0x8C63A5DC,0x8C63A682),
 ('rotate_zyx_register',0x8C639C34,0x8C639C34,0x8C639E02),
 ('rotate_yxz_register',0x8C639F38,0x8C639F38,0x8C63A108),
)

def inspect(ram,entry,begin,end):
    pending=[entry];ins={};delays=set();calls=[]
    word=lambda pc:struct.unpack_from('<H',ram,pc-shared.BASE)[0]
    while pending:
        pc=pending.pop()
        if pc in ins:continue
        if pc<begin or pc+2>end or pc&1:raise ValueError(f'Owner {entry:08X} leaves reviewed span: {pc:08X}')
        op=word(pc);ins[pc]=op;high,upper=op>>12,op>>8
        successors=[pc+2];delayed=False
        if op==0xB:successors=[];delayed=True
        elif op&0xF0FF==0x402B:
            successors=[];delayed=True;calls.append(dict(pc=f'{pc:08X}',opcode=f'{op:04X}',target=None,tail=True))
        elif high==0xA:successors=[pc+4+2*shared.signed(op&4095,12)];delayed=True
        elif upper in (0x89,0x8B,0x8D,0x8F):
            delayed=upper in (0x8D,0x8F);successors=[pc+(4 if delayed else 2),pc+4+2*shared.signed(op&255,8)]
        elif high==0xB or op&0xF0FF==0x400B:
            successors=[pc+4];delayed=True
            calls.append(dict(pc=f'{pc:08X}',opcode=f'{op:04X}',target=f'{pc+4+2*shared.signed(op&4095,12):08X}' if high==0xB else None))
        elif op&0xF0FF in (0x0003,0x0023):raise ValueError(f'Unreviewed transfer {pc:08X}')
        if delayed:
            slot=word(pc+2)
            if slot==0xB or slot>>12 in (0xA,0xB) or slot>>8 in (0x89,0x8B,0x8D,0x8F) or slot&0xF0FF in (0x400B,0x402B,0x0003,0x0023):raise ValueError('Nested delay transfer')
            delays.add(pc+2)
        pending+=successors
    return ins,delays,sorted(calls,key=lambda c:c['pc'])

def emit_simple(pc,op,ram,restart=None):
    n,m=(op>>8)&15,(op>>4)&15;r,s=f'cpu.r[{n}]',f'cpu.r[{m}]'
    if op&0xF00F==0x3002:body=f'cpu.t={r}>={s};'
    elif op&0xF00F==0x3006:body=f'cpu.t={r}>{s};'
    elif op&0xF00F==0x3008:body=f'{r}-={s};'
    elif op&0xF0FF==0x4000:body=f'cpu.t=({r}&0x80000000u)!=0u;{r}<<=1u;'
    elif op&0xF0FF==0x4001:body=f'cpu.t=({r}&1u)!=0u;{r}>>=1u;'
    elif op&0xF0FF==0xF07D:body=f'fpu_reciprocal_square_root(cpu,{n}u);'
    else:return world.emit_simple(pc,op,ram,restart).replace('WORLD_SITE','HIERARCHY_SITE')
    return f'HIERARCHY_SITE(0x{pc:08X}u);'+body

def emit_body(ram,entry,instructions):
    word=lambda pc:struct.unpack_from('<H',ram,pc-shared.BASE)[0]
    label=lambda pc:f'L{pc:08X}'
    lines=[f'goto {label(entry)};']
    for pc,op in sorted(instructions.items()):
        lines.append(f'{label(pc)}: {{ HIERARCHY_SITE(0x{pc:08X}u); // {op:04X}')
        high,upper=op>>12,op>>8
        if (high==0xA and pc+4+2*shared.signed(op&4095,12)<=pc) or (upper in (0x89,0x8B,0x8D,0x8F) and pc+4+2*shared.signed(op&255,8)<=pc):
            lines.append(f'backedge(0x{pc:08X}u);')
        at=f'RestartPoint{{0x{pc:08X}u}}'
        delay=lambda at:emit_simple(pc+2,word(pc+2),ram,at)
        if op==0xB:lines+=['const auto target=cpu.pr;',delay(at),f'return_site=0x{pc:08X}u;cpu.pc=target;return;']
        elif op&0xF0FF==0x402B:
            lines+=[f'const auto target=cpu.r[{(op>>8)&15}];',delay(at),'call(target);return;']
        elif high==0xA:lines+=[delay(at),f'goto {label(pc+4+2*shared.signed(op&4095,12))};']
        elif upper in (0x89,0x8B,0x8D,0x8F):
            delayed=upper in (0x8D,0x8F);condition='cpu.t' if upper in (0x89,0x8D) else '!cpu.t'
            lines.append(f'const bool taken={condition};')
            if delayed:lines.append(delay(at))
            lines.append(f'if(taken)goto {label(pc+4+2*shared.signed(op&255,8))};goto {label(pc+(4 if delayed else 2))};')
        elif high==0xB or op&0xF0FF==0x400B:
            target=f'0x{pc+4+2*shared.signed(op&4095,12):08X}u' if high==0xB else f'cpu.r[{(op>>8)&15}]'
            lines+=[f'const auto target={target};[[maybe_unused]] const auto old_pr=cpu.pr;cpu.pr=0x{pc+4:08X}u;',delay(f'RestartPoint{{0x{pc:08X}u,old_pr}}'),'call(target);',f'goto {label(pc+4)};']
        else:lines+=[emit_simple(pc,op,ram),f'goto {label(pc+2)};']
        lines.append('}')
    return '\n'.join(lines)+'\n'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--ram',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();ram=args.ram.read_bytes()
    if len(ram)!=0x1000000 or hashlib.sha256(ram).hexdigest()!=shared.RAM_SHA:raise ValueError('PAL RAM identity')
    args.output.mkdir(parents=True,exist_ok=True);spans=[];reports=[]
    for name,entry,begin,end in OWNERS:
        ins,delays,calls=inspect(ram,entry,begin,end)
        used=dict(ins);used.update({pc:struct.unpack_from('<H',ram,pc-shared.BASE)[0] for pc in delays})
        spans.append((begin,end-begin));literals=set()
        for pc,op in used.items():
            if op>>12==0xD:literals.add((((pc+4)&~3)+(op&255)*4,4))
            if op>>12==9:literals.add((pc+4+(op&255)*2,2))
            if op>>8==0xC7:literals.add((((pc+4)&~3)+(op&255)*4,4))
        spans+=sorted(literals)
        (args.output/f'hierarchy-{name}.inc').write_text(emit_body(ram,entry,ins),encoding='ascii',newline='\n')
        reports.append(dict(name=name,entry=f'{entry:08X}',begin=f'{begin:08X}',end=f'{end:08X}',instructions=len(ins),delays=len(delays),calls=calls))
    merged=[]
    for start,size in sorted(spans):
        if merged and start<=sum(merged[-1]):merged[-1]=(merged[-1][0],max(start+size,sum(merged[-1]))-merged[-1][0])
        else:merged.append((start,size))
    (args.output/'hierarchy-identities.inc').write_text(shared.emit_identities(ram,merged),encoding='ascii',newline='\n')
    (args.output/'hierarchy-switch.inc').write_text('\n'.join(f'case 0x{entry:08X}u: {{\n#include "hierarchy-{name}.inc"\n}}' for name,entry,_,_ in OWNERS)+'\n')
    (args.output/'hierarchy-members.inc').write_text('\n'.join(f'case 0x{entry:08X}u:' for _,entry,_,_ in OWNERS)+'\nreturn true;\n')
    (args.output/'hierarchy-inventory.json').write_text(json.dumps(dict(schema='sarecomp-render-hierarchy-v1',ram_sha256=shared.RAM_SHA,owners=reports,source_spans=[dict(address=f'{a:08X}',size=n,sha256=hashlib.sha256(ram[a-shared.BASE:a-shared.BASE+n]).hexdigest()) for a,n in merged]),indent=2)+'\n')
    print(f'SONIC_RENDER_HIERARCHY_READY owners={len(reports)} instructions={sum(x["instructions"] for x in reports)}')
if __name__=='__main__':main()
