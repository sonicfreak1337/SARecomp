"""Author the complete PAL collision-world production family (build time only)."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import prepare_collision_candidates as shared

spec = importlib.util.spec_from_file_location('movement_author', Path(__file__).with_name('prepare-movement-resolver.py'))
movement = importlib.util.module_from_spec(spec)
spec.loader.exec_module(movement)
OWNERS = (
    ('world', 0x8C028EC2, 0x8C028EC2, 0x8C02939C),
    ('eligibility', 0x8C052518, 0x8C052518, 0x8C05289E),
    ('hierarchy', 0x8C028B00, 0x8C028B00, 0x8C028B9C),
    ('polygons', 0x8C0287A0, 0x8C0287A0, 0x8C028AD0),
    ('vertices', 0x8C028666, 0x8C028666, 0x8C02876C),
    ('object_allocate', 0x8C02CEF4, 0x8C02CEF4, 0x8C02CF20),
    ('object_release', 0x8C02CF48, 0x8C02CF20, 0x8C02CF62),
    ('polygon_dot', 0x8C02CF68, 0x8C02CF68, 0x8C02CF70),
    ('buckets_clear', 0x8C02CFA4, 0x8C02CFA4, 0x8C02CFB8),
    ('polygon_allocate', 0x8C02CFC0, 0x8C02CFC0, 0x8C02D00E),
    ('buckets_join', 0x8C02D00E, 0x8C02D00E, 0x8C02D050),
)

def inspect(ram, entry, begin, end):
    # CF48 tail-branches into CF20; walk that whole reviewed release owner.
    return shared.inspect(ram,entry,end,begin=begin)

def emit_simple(pc, op, ram, restart=None):
    n, m = (op >> 8) & 15, (op >> 4) & 15
    r, s = f'cpu.r[{n}]', f'cpu.r[{m}]'
    if op & 0xF00F == 0x0007:
        body = f'cpu.macl={r}*{s};'
    elif op & 0xF0FF == 0x4010:
        body = f'--{r};cpu.t={r}==0u;'
    elif op & 0xF00F == 0x400C:
        body = (f'{{const auto shift={s};const auto value={r};'
                f'{r}=!(shift&0x80000000u)?value<<(shift&31u):'
                f'!(shift&31u)?(std::bit_cast<std::int32_t>(value)<0?0xFFFFFFFFu:0u):'
                f'std::uint32_t(std::bit_cast<std::int32_t>(value)>>((0u-shift)&31u));}}')
    elif op & 0xF0FF == 0xF0ED:
        body = f'fpu_inner_product(cpu,{((op>>8)&3)*4}u,{((op>>10)&3)*4}u);'
    else:
        return movement.emit_simple(pc, op, ram, restart).replace('MOVEMENT_SITE', 'WORLD_SITE')
    return f'WORLD_SITE(0x{pc:08X}u);'+body

def emit_body(ram, entry, instructions):
    word=lambda pc:struct.unpack_from('<H',ram,pc-shared.BASE)[0]
    label=lambda pc:f'L{pc:08X}'
    lines=[f'goto {label(entry)};']
    for pc,op in sorted(instructions.items()):
        lines.append(f'{label(pc)}: {{ WORLD_SITE(0x{pc:08X}u); // {op:04X}')
        if pc == 0x8C028F9C:
            lines.append('if(index.membership(cpu,a))goto L8C028FE6;')
        if pc == 0x8C02901C:
            lines.append('if(index.first_eligible(cpu,a)){if(cpu.r[7]==0u)goto L8C029068;goto L8C029024;}')
        high,upper=op>>12,op>>8
        at=f'RestartPoint{{0x{pc:08X}u}}'
        delay=lambda restart:emit_simple(pc+2,word(pc+2),ram,restart)
        if op==0x000B:
            lines.extend(['const auto target=cpu.pr;',delay(at),f'return_site=0x{pc:08X}u;cpu.pc=target;return;'])
        elif high==0xA:
            lines.extend([delay(at),f'goto {label(pc+4+2*shared.signed(op&0xFFF,12))};'])
        elif upper in (0x89,0x8B,0x8D,0x8F):
            delayed=upper in (0x8D,0x8F)
            condition='cpu.t' if upper in (0x89,0x8D) else '!cpu.t'
            lines.append(f'const bool taken={condition};')
            if delayed:lines.append(delay(at))
            lines.append(f'if(taken)goto {label(pc+4+2*shared.signed(op&255,8))};goto {label(pc+(4 if delayed else 2))};')
        elif high==0xB or op&0xF0FF==0x400B:
            target=f'0x{pc+4+2*shared.signed(op&0xFFF,12):08X}u' if high==0xB else f'cpu.r[{(op>>8)&15}]'
            lines.extend([f'const auto target={target};[[maybe_unused]] const auto old_pr=cpu.pr;cpu.pr=0x{pc+4:08X}u;',
                          delay(f'RestartPoint{{0x{pc:08X}u,old_pr}}'),'call(target);',f'goto {label(pc+4)};'])
        else:
            lines.extend([emit_simple(pc,op,ram),f'goto {label(pc+2)};'])
        lines.append('}')
    return '\n'.join(lines)+'\n'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--ram',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();ram=a.ram.read_bytes()
    if len(ram)!=0x1000000 or hashlib.sha256(ram).hexdigest()!=shared.RAM_SHA:
        raise ValueError('Unexpected original PAL image')
    a.output.mkdir(parents=True,exist_ok=True)
    spans=[];reports=[]
    for name,entry,begin,end in OWNERS:
        ins,delays,calls=inspect(ram,entry,begin,end)
        used=dict(ins);used.update({pc:struct.unpack_from('<H',ram,pc-shared.BASE)[0] for pc in delays})
        spans.append((begin,end-begin));literals=set()
        for pc,op in used.items():
            if op>>12==0xD:literals.add((((pc+4)&~3)+(op&255)*4,4))
            if op>>12==9:literals.add((pc+4+(op&255)*2,2))
            if op>>8==0xC7:literals.add((((pc+4)&~3)+(op&255)*4,4))
        spans.extend(literals)
        (a.output/f'world-{name}.inc').write_text(emit_body(ram,entry,ins),encoding='ascii',newline='\n')
        reports.append(dict(name=name,entry=f'{entry:08X}',begin=f'{begin:08X}',end=f'{end:08X}',
            instructions=len(ins),delays=len(delays),calls=calls,
            reachable={f'{pc:08X}':f'{op:04X}' for pc,op in sorted(used.items())}))
    merged=[]
    for start,size in sorted(spans):
        if merged and start<=sum(merged[-1]):merged[-1]=(merged[-1][0],max(start+size,sum(merged[-1]))-merged[-1][0])
        else:merged.append((start,size))
    (a.output/'world-identities.inc').write_text(shared.emit_identities(ram,merged),encoding='ascii',newline='\n')
    (a.output/'world-inventory.json').write_text(json.dumps(dict(schema='sarecomp-collision-world-v1',
        ram_sha256=shared.RAM_SHA,owners=reports,source_spans=[dict(address=f'{x:08X}',size=n,
            sha256=hashlib.sha256(ram[x-shared.BASE:x-shared.BASE+n]).hexdigest()) for x,n in merged]),indent=2)+'\n')
    print(f'SONIC_COLLISION_WORLD_READY owners={len(reports)} instructions={sum(x["instructions"] for x in reports)}')
if __name__=='__main__':main()
