"""Author the complete PAL movement/contact owner, without a runtime decoder."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
import prepare_collision_candidates as shared
import prepare_near_collision as extra
ENTRY, BODY, END = 0x8C073018, 0x8C0730A0, 0x8C074214
RANGES = [(ENTRY, END-ENTRY), (0x8C0742C0,16)]
PHASES = {
    0x8C0730A0: 'Preserve caller frame and previous contact; build oriented NEAR query.',
    0x8C07322A: 'Filter live candidates and collect first-pass TOUCH responses.',
    0x8C0733CE: 'Prioritize selected and previous contacts, retaining guest array order.',
    0x8C07358E: 'Second TOUCH pass: classify support, direction and alternative response.',
    0x8C073994: 'Collect contact normals and orientation response.',
    0x8C073B5E: 'Rebuild query after orientation changes; accept/correct movement.',
    0x8C074022: 'Alternative response and next-candidate loop.',
    0x8C0740DC: 'Finalize position, velocity and retained contact references.'}

def emit_simple(pc,op,ram,restart=None):
    n,m=(op>>8)&15,(op>>4)&15
    r,s=f'cpu.r[{n}]',f'cpu.r[{m}]'
    origin=f'0x{pc:08X}u'
    if op&0xF00F==0x000E: out=f'{r}=load(cpu.r[0]+{s});'
    elif op&0xF00F==0x000D: out=f'{r}=std::uint32_t(std::int32_t(std::int16_t(load16(cpu.r[0]+{s}))));'
    elif op&0xF00F==0x0006: out=f'store({origin},cpu.r[0]+{r},{s},CodeWriteSource::Cpu);'
    elif op>>8==0x84: out=f'cpu.r[0]=std::uint32_t(std::int32_t(std::int8_t(load8({s}+{op&15}u))));'
    elif op>>8==0x85: out=f'cpu.r[0]=std::uint32_t(std::int32_t(std::int16_t(load16({s}+{(op&15)*2}u))));'
    elif op>>8==0x81: out=f'store16({origin},{s}+{(op&15)*2}u,std::uint16_t(cpu.r[0]),CodeWriteSource::Cpu);'
    elif op&0xF00F==0x600C: out=f'{r}={s}&255u;'
    elif op&0xF00F==0x600D: out=f'{r}={s}&65535u;'
    elif op&0xF0FF==0x0029: out=f'{r}=cpu.t?1u:0u;'
    elif op&0xF00F==0x3007: out=f'set_t(std::bit_cast<std::int32_t>({r})>std::bit_cast<std::int32_t>({s}));'
    elif op&0xF00F==0x200E: out=f'cpu.macl=({r}&65535u)*({s}&65535u);'
    elif op>>8==0xC8: out=f'set_t((cpu.r[0]&{op&255}u)==0u);'
    elif op>>8==0xC9: out=f'cpu.r[0]&={op&255}u;'
    elif op>>8==0xCB: out=f'cpu.r[0]|={op&255}u;'
    else: out=extra.emit_simple(pc,op,ram)
    # Every fallback access carries the exact originating instruction.
    out=re.sub(r'\b(load|load16|load8)\(',lambda m:m[1]+'('+origin+',',out)
    if restart is not None:
        out=re.sub(r'\b(load|load16|load8|store|store16)\('+origin,
                   lambda m:m[1]+'('+restart,out)
    return f'MOVEMENT_SITE({origin});'+out

def emit_body(ram,instructions):
    # Retain every original continuation while making the whole multi-pass
    # resolver one native owner. Never execute a partial slow instruction.
    word=lambda pc:struct.unpack_from('<H',ram,pc-shared.BASE)[0]
    label=lambda pc:f'L{pc:08X}'
    lines=['// Complete authenticated PAL movement/contact owner.']
    for pc,op in sorted(instructions.items()):
        high,upper=op>>12,op>>8
        lines.append(f'{label(pc)}: {{ MOVEMENT_SITE(0x{pc:08X}u); // {op:04X}')
        delay=lambda restart:emit_simple(pc+2,word(pc+2),ram,restart)
        restart=f'RestartPoint{{0x{pc:08X}u}}'
        if op==0x000B:
            lines.extend(['const auto target=cpu.pr;',delay(restart),'cpu.pc=target;return true;'])
        elif high==0xA:
            lines.extend([delay(restart),f'goto {label(pc+4+2*shared.signed(op&0xFFF,12))};'])
        elif upper in (0x89,0x8B,0x8D,0x8F):
            delayed=upper in (0x8D,0x8F)
            condition='cpu.t' if upper in (0x89,0x8D) else '!cpu.t'
            lines.append(f'const bool taken={condition};')
            if delayed:lines.append(delay(restart))
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
    p.add_argument('--ram',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args(); ram=a.ram.read_bytes()
    if len(ram)!=0x1000000 or hashlib.sha256(ram).hexdigest()!=shared.RAM_SHA:
        raise ValueError('Unexpected PAL memory identity')
    if hashlib.sha256(ram[BODY-shared.BASE:END-shared.BASE]).hexdigest()!='a6ea591ff19b73ed4f2124d643c6f94b9651cef5ec9f88cdb7d42cf29eb3e001':
        raise ValueError('Movement body identity')
    instructions,delays,calls=shared.inspect(ram,BODY,END)
    used=dict(instructions)
    used.update({pc:struct.unpack_from('<H',ram,pc-shared.BASE)[0] for pc in delays})
    literals=set()
    for pc,op in used.items():
        if op>>12==0xD: literals.add((((pc+4)&~3)+(op&255)*4,4))
        if op>>12==9: literals.add((pc+4+(op&255)*2,2))
        if op>>8==0xC7: literals.add((((pc+4)&~3)+(op&255)*4,4))
    for address,size in literals:
        if not any(x<=address and address+size<=x+n for x,n in RANGES):
            raise ValueError(f'Unbound literal {address:08X}')
    text=emit_body(ram,instructions)
    for pc,meaning in PHASES.items():
        text=text.replace(f'L{pc:08X}:',f'// {meaning}\nL{pc:08X}:')
    a.output.mkdir(parents=True,exist_ok=True)
    (a.output/'movement-body.inc').write_text(text,encoding='ascii',newline='\n')
    (a.output/'movement-identities.inc').write_text(shared.emit_identities(ram,RANGES),encoding='ascii',newline='\n')
    (a.output/'movement-inventory.json').write_text(json.dumps({
        'schema':'sarecomp-complete-movement-v1','ram_sha256':shared.RAM_SHA,
        'entry':f'{ENTRY:08X}','body':f'{BODY:08X}','end':f'{END:08X}',
        'instructions':len(instructions),'delay_slots':len(delays),'calls':calls,
        'reachable':{f'{pc:08X}':f'{op:04X}' for pc,op in sorted(used.items())},
        'literals':[{'address':f'{x:08X}','size':n} for x,n in sorted(literals)],
        'source_spans':[{'address':f'{x:08X}','size':n,'sha256':hashlib.sha256(ram[x-shared.BASE:x-shared.BASE+n]).hexdigest()} for x,n in RANGES]
    },indent=2)+'\n',encoding='ascii',newline='\n')
    print(f'SONIC_MOVEMENT_READY instructions={len(instructions)} delay_slots={len(delays)} calls={len(calls)}')
if __name__=='__main__': main()
