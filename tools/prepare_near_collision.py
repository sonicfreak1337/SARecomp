"""Author complete NEAR-POLY/eligibility bodies from authenticated retail bytes."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import prepare_collision_candidates as shared

BASE = shared.BASE
OWNERS = (
    ('near-poly', 0x8C028BFE, 0x8C028E44,
     '1039a254e1a32bfd40deedc30c20cb7e4cb8c082732c926160ba58a95c6992c8'),
    ('near-eligibility', 0x8C0522C0, 0x8C052518,
     '38a058ff1b5ebddd17fe736c6834d581a645b0d4625be8aad6b71f97249dff28'),
)
SOURCE_RANGES = sorted([
    (0x8C028BFE, 0x246), (0x8C0522C0, 0x258), (0x8C052600, 4),
    (0x8C63A69C, 0x10), (0x8C639BB0, 0x80), (0x8C639AD8, 0x40),
    (0x8C638E0C, 0x58), (0x8C639E08, 0x2A), (0x8C639E9C, 0x2C),
    (0x8C63A10C, 0x2E), (0x8C63A820, 0x20), (0x8C63A904, 0x14),
])

def emit_simple(pc, op, ram):
    n, m = (op >> 8) & 15, (op >> 4) & 15
    r, s = f'cpu.r[{n}]', f'cpu.r[{m}]'
    origin = f'0x{pc:08X}u'
    # Additional original opcodes in the two complete owner bodies.
    if op & 0xF00F == 0x2001:
        return f'store16({origin},{r},std::uint16_t({s}),CodeWriteSource::Cpu);'
    if op & 0xF00F == 0x3002:
        return f'set_t({r}>={s});'
    if op & 0xF00F == 0x3008:
        return f'{r}-={s};'
    if op & 0xF0FF == 0x4000:
        return f'set_t(({r}&0x80000000u)!=0u);{r}<<=1u;'
    if op & 0xF0FF == 0x4008:
        return f'{r}<<=2u;'
    if op & 0xF0FF == 0x4018:
        return f'{r}<<=8u;'
    if op & 0xF00F == 0xF00E:
        return f'fpu_multiply_accumulate(cpu,{m}u,{n}u);'
    return shared.emit_simple(pc, op, ram)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ram', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args=parser.parse_args()
    ram=args.ram.read_bytes()
    if len(ram)!=0x1000000 or hashlib.sha256(ram).hexdigest()!=shared.RAM_SHA:
        raise ValueError('Unexpected original memory image')
    args.output.mkdir(parents=True, exist_ok=True)
    reports=[]
    for name, entry, end, digest in OWNERS:
        if hashlib.sha256(ram[entry-BASE:end-BASE]).hexdigest()!=digest:
            raise ValueError(f'Original owner changed: {name}')
        instructions, delays, calls=shared.inspect(ram,entry,end)
        used=dict(instructions)
        used.update({pc:struct.unpack_from('<H',ram,pc-BASE)[0] for pc in delays})
        # Each PC-relative literal used by reachable code must be authenticated,
        # including 052600 beyond the eligibility owner's return.
        literals=set()
        for pc, op in used.items():
            if op>>12==0xD: literals.add((((pc+4)&~3)+(op&255)*4,4))
            if op>>12==9: literals.add((pc+4+(op&255)*2,2))
            if op>>8==0xC7: literals.add((((pc+4)&~3)+(op&255)*4,4))
        for address, size in literals:
            if not any(a<=address and address+size<=a+n for a,n in SOURCE_RANGES):
                raise ValueError(f'Unauthenticated literal {address:08X}')
        body=shared.emit_body(ram,instructions,emit_simple)
        body=body.replace('PAL TOUCH-POLY owner',f'PAL {name} owner')
        (args.output/(name+'-body.inc')).write_text(body,encoding='ascii',newline='\n')
        reports.append({'name':name,'entry':f'{entry:08X}','end':f'{end:08X}',
            'source_sha256':digest,'instructions':len(instructions),'delay_slots':len(delays),
            'calls':calls,'literals':[{'address':f'{a:08X}','size':n} for a,n in sorted(literals)],
            'reachable':{f'{pc:08X}':f'{op:04X}' for pc,op in sorted(used.items())}})
    (args.output/'near-identities.inc').write_text(
        shared.emit_identities(ram,SOURCE_RANGES),encoding='ascii',newline='\n')
    (args.output/'near-inventory.json').write_text(json.dumps({
        'schema':'sarecomp-near-collision-v1','ram_sha256':shared.RAM_SHA,'owners':reports,
        'source_spans':[{'address':f'{a:08X}','size':n,
            'sha256':hashlib.sha256(ram[a-BASE:a-BASE+n]).hexdigest()} for a,n in SOURCE_RANGES]
    },indent=2)+'\n',encoding='ascii',newline='\n')
    print('SONIC_NEAR_OWNER_READY '+', '.join(
        f"{r['name']}={r['instructions']} instructions/{r['delay_slots']} delays" for r in reports))

if __name__=='__main__':
    main()
