"""Author the seven complete inverse-trig parents; reuse the four native children."""
import argparse
import hashlib
from pathlib import Path
import struct
import prepare_collision_candidates as shared

OWNERS = (
    ('acos_wrapper', 0x8C10CF48, 0x50, 100, '67190b24fdf1f5c43e1a3a7ea5c1fbd06087f1fbd0b721cba490e73102bf777a'),
    ('asin_wrapper', 0x8C10CF98, 0x50, 96, '96055337e822b6e0fee2a34e5d6c37e0c3e495cb49cc2bf75581e0f11ade1111'),
    ('atan2_wrapper', 0x8C10CFE8, 0x50, 92, '78db9abf037229a0aebf28c5d183f0fd7adab9c5d0ab948341257ceb170805b6'),
    ('atan2_wrapper_alt', 0x8C10D038, 0x50, 92, '78db9abf037229a0aebf28c5d183f0fd7adab9c5d0ab948341257ceb170805b6'),
    ('acos', 0x8C10E4D0, 0x1C, 92, 'c64279e7db910660f546db7619305d6fa96170593bf67c472d1672b3c4dcbe88'),
    ('asin', 0x8C10E4EC, 0xF4, 88, 'cca074bef0fb28c0432706617278427494850ca927e2dbaeb6a53da156b58913'),
    ('atan2', 0x8C10E5E0, 0x58, 84, '42f5dbc9da5124c303617d367cb4f87cf27d7d2761e1a4b3b325744e1e92df2b'),
)

def emit(pc, op, ram):
    n, m = (op >> 8) & 15, (op >> 4) & 15
    if op & 0xF00F == 0x200B:
        return f'cpu.r[{n}]|=cpu.r[{m}];'
    if op & 0xF0FF == 0xF06D:
        return f'fpu_square_root(cpu,{n}u);'
    return shared.emit_simple(pc, op, ram)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ram', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args=parser.parse_args(); ram=args.ram.read_bytes()
    if len(ram)!=0x1000000 or hashlib.sha256(ram).hexdigest()!=shared.RAM_SHA:
        raise ValueError('Original RAM identity differs')
    identities=[]; bodies=[]; members=[]; total=0
    for name, entry, size, depth, digest in OWNERS:
        data=ram[entry-shared.BASE:entry-shared.BASE+size]
        if hashlib.sha256(data).hexdigest()!=digest: raise ValueError(f'Owner changed: {entry:08X}')
        ins, delays, calls=shared.inspect(ram,entry,entry+size)
        total+=len(ins)+len(delays)
        body=shared.emit_body(ram,ins,emit)
        # These are the exact two arithmetic epochs in the retained asin AOT.
        # In particular E58E's delayed divide is OUTSIDE its preceding epoch.
        for pc in (0x8C10E57C,0x8C10E590):
            body=body.replace(f'L{pc:08X}: {{',f'L{pc:08X}: {{ epoch.emplace(cpu);')
        for pc in (0x8C10E58C,0x8C10E5A2):
            body=body.replace(f'L{pc:08X}: {{',f'L{pc:08X}: {{ epoch.reset();')
        for pc, op in ins.items():
            if op==0x000B:
                old=f'L{pc:08X}: {{ // 000B\nconst auto target=cpu.pr;'
                new=old+f' inverse_return_site=0x{pc:08X}u;'
                if old not in body:raise ValueError('Return annotation missing')
                body=body.replace(old,new)
        bodies.append(f'case 0x{entry:08X}u: {{\n{body}\n}}')
        members.append(f'case 0x{entry:08X}u:return {depth}u;')
        words=struct.unpack('<'+'H'*(size//2),data)
        identities.append(f'constexpr std::array<std::uint16_t,{len(words)}> inverse_{entry:08X}{{'+','.join(f'0x{w:04X}u' for w in words)+'};')
    identities.append('constexpr std::array inverse_identities{')
    identities.extend(f'InverseIdentity{{0x{a:08X}u,std::span(inverse_{a:08X})}},' for _,a,_,_,_ in OWNERS)
    identities.append('};')
    identities.append('static_assert(sizeof(inverse_8C10CF48)==0x50u);')
    if struct.unpack_from('<I',ram,0x16017C)[0]!=0xC0490FDB:raise ValueError('Negative pi constant differs')
    args.output.mkdir(parents=True,exist_ok=True)
    for name,text in [('inverse-identities.inc','\n'.join(identities)),('inverse-bodies.inc','\n'.join(bodies)),('inverse-members.inc','\n'.join(members))]:
        data=(text+'\n').encode('ascii'); path=args.output/name
        if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)
    print(f'SONIC_INVERSE_TRIG_READY parents=7 instruction_pcs={total}')

if __name__=='__main__':main()
