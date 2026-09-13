"""Transcribe authenticated PAL collision owners into bounded native C++ bodies.

Authoring only: no instruction decoding is linked into the game. The generated
body requires separate admission and synchronous callee contracts before use.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct

RAM_SHA = 'b64a98597751d995aa95346df260d79efb38deb37bd174efa01c8d732645846c'
BASE = 0x8C000000
ENTRY = 0x8C029B00
END = 0x8C02A66C

# Complete authored owner plus the RAM-only callee closure and coefficient
# table. The 88-byte copier follows CD1C -> CDBC and reads its extra last word.
SOURCE_RANGES = sorted([
    (ENTRY, END-ENTRY), (0x8C027360, 0x42),
    (0x8C63A69C, 0x10), (0x8C63A88C, 0x20),
    (0x8C639BB0, 0x80), (0x8C639AD8, 0x40),
    (0x8C10CF48, 0x50), (0x8C10CF98, 0x50), (0x8C10D038, 0x50),
    (0x8C10E4D0, 0x168), (0x8C10E6F8, 0xC0), (0x8C10EEC4, 0x1E0),
    (0x8C10FAD4, 0x128), (0x8C16012C, 0x1C8),
    (0x8C638E0C, 0x58), (0x8C638FF0, 0x804), (0x8C64F32C, 0x158),
    (0x8C639E08, 0x130), (0x8C10CD1C, 0xC0),
])

def emit_identities(ram):
    lines = ['// Authenticated immutable source bytes; never decoded at runtime.']
    for i, (address, size) in enumerate(SOURCE_RANGES):
        data = ram[address-BASE:address-BASE+size]
        lines.append(f'constexpr std::array<std::uint8_t,{size}> identity_{i}{{')
        lines.extend('    '+','.join(f'0x{v:02X}' for v in data[j:j+16])+','
                     for j in range(0, size, 16))
        lines.append('};')
    lines.append('constexpr std::array identities{')
    lines.extend(f'    SourceSpan{{0x{address:08X}u,identity_{i}}},'
                 for i, (address, _) in enumerate(SOURCE_RANGES))
    lines.append('};')
    return '\n'.join(lines)+'\n'

def signed(value, bits):
    return (value ^ (1 << (bits - 1))) - (1 << (bits - 1))

def inspect(ram):
    words = lambda pc: struct.unpack_from('<H', ram, pc - BASE)[0]
    pending = [ENTRY]
    instructions = {}
    delay_slots = set()
    calls = []
    while pending:
        pc = pending.pop()
        if pc in instructions:
            continue
        if pc < ENTRY or pc + 2 > END or pc & 1:
            raise ValueError(f'Control flow leaves the complete owner: {pc:08X}')
        op = words(pc)
        instructions[pc] = op
        high = op >> 12
        upper = op >> 8
        successors = [pc + 2]
        delayed = False
        if op == 0x000B:
            successors = []
            delayed = True
        elif high == 0xA:
            successors = [pc + 4 + 2 * signed(op & 0xFFF, 12)]
            delayed = True
        elif upper in (0x89, 0x8B, 0x8D, 0x8F):
            delayed = upper in (0x8D, 0x8F)
            successors = [pc + (4 if delayed else 2), pc + 4 + 2 * signed(op & 255, 8)]
        elif high == 0xB or op & 0xF0FF == 0x400B:
            delayed = True
            successors = [pc + 4]
            calls.append({'pc': f'{pc:08X}', 'opcode': f'{op:04X}',
                          'target': f'{pc + 4 + 2 * signed(op & 0xFFF, 12):08X}' if high == 0xB else None})
        elif op & 0xF0FF in (0x0003, 0x0023, 0x402B):
            raise ValueError(f'Unreviewed computed control transfer at {pc:08X}: {op:04X}')
        if delayed:
            delay_slots.add(pc + 2)
            slot = words(pc + 2)
            if slot == 0x000B or slot >> 12 in (0xA, 0xB) or slot >> 8 in (0x89, 0x8B, 0x8D, 0x8F) or slot & 0xF0FF in (0x400B, 0x402B, 0x0003, 0x0023):
                raise ValueError(f'Control transfer in delay slot {pc+2:08X}')
        pending.extend(successors)
    return instructions, delay_slots, sorted(calls, key=lambda call: call['pc'])

def emit_simple(pc, op, ram):
    n, m = (op >> 8) & 15, (op >> 4) & 15
    high, low = op >> 12, op & 15
    r, s = f'cpu.r[{n}]', f'cpu.r[{m}]'
    f, g = f'cpu.fr[{n}]', f'cpu.fr[{m}]'
    origin = f'0x{pc:08X}u'
    if op == 9: return ''
    if high == 0xE: return f'{r}=0x{signed(op & 255,8) & 0xFFFFFFFF:08X}u;'
    if high == 7: return f'{r}+=0x{signed(op & 255,8) & 0xFFFFFFFF:08X}u;'
    if high == 0xD:
        address = ((pc+4)&~3)+(op & 255)*4
        value = struct.unpack_from('<I', ram, address-BASE)[0]
        return f'{r}=0x{value:08X}u; // authenticated literal {address:08X}'
    if high == 9:
        address = pc+4+(op & 255)*2
        value = struct.unpack_from('<h', ram, address-BASE)[0] & 0xFFFFFFFF
        return f'{r}=0x{value:08X}u; // authenticated literal {address:08X}'
    if op >> 8 == 0xC7: return f'cpu.r[0]=0x{((pc+4)&~3)+(op & 255)*4:08X}u;'
    if high == 1: return f'store({origin},{r}+{low*4}u,{s},CodeWriteSource::Cpu);'
    if high == 5: return f'{r}=load({s}+{low*4}u);'
    if high == 6:
        if low == 0: return f'{r}=std::uint32_t(std::int32_t(std::int8_t(load8({s}))));'
        if low == 1: return f'{r}=std::uint32_t(std::int32_t(std::int16_t(load16({s}))));'
        if low == 2: return f'{r}=load({s});'
        if low == 3: return f'{r}={s};'
        if low == 6: return f'{r}=load({s});'+(f'{s}+=4u;' if n!=m else '')
        if low == 0xB: return f'{r}=0u-{s};'
        if low == 0xF: return f'{r}=std::uint32_t(std::int32_t(std::int16_t({s})));'
    if high == 2:
        if low == 2: return f'store({origin},{r},{s},CodeWriteSource::Cpu);'
        if low == 6: return f'{{const auto address={r}-4u;const auto value={s};store({origin},address,value,CodeWriteSource::Cpu);{r}=address;}}'
        if low == 8: return f'set_t(({r}&{s})==0u);'
        if low == 9: return f'{r}&={s};'
        if low == 0xF: return f'cpu.macl=std::uint32_t(std::int32_t(std::int16_t({r}))*std::int32_t(std::int16_t({s})));'
    if high == 3:
        if low == 0: return f'set_t({r}=={s});'
        if low == 3: return f'set_t(std::bit_cast<std::int32_t>({r})>=std::bit_cast<std::int32_t>({s}));'
        if low == 0xC: return f'{r}+={s};'
    if high == 0:
        if low == 5: return f'store16({origin},cpu.r[0]+{r},std::uint16_t({s}),CodeWriteSource::Cpu);'
        if op & 0xF0FF == 0x001A: return f'{r}=cpu.macl;'
        if op & 0xF0FF == 0x005A: return f'{r}=cpu.fpul;'
    if high == 4:
        suffix = op & 255
        if suffix == 0x11: return f'set_t(std::bit_cast<std::int32_t>({r})>=0);'
        if suffix == 0x15: return f'set_t(std::bit_cast<std::int32_t>({r})>0);'
        if suffix == 0x22: return f'{{const auto address={r}-4u;store({origin},address,cpu.pr,CodeWriteSource::Cpu);{r}=address;}}'
        if suffix == 0x26: return f'cpu.pr=load({r});{r}+=4u;'
        if suffix == 0x5A: return f'cpu.fpul={r};'
    if op >> 8 == 0x88: return f'set_t(cpu.r[0]==0x{signed(op & 255,8) & 0xFFFFFFFF:08X}u);'
    if high == 0xF:
        if low in (0,1,2,3):
            operation = ('Add','Subtract','Multiply','Divide')[low]
            return f'fpu_binary(cpu,FpuBinaryOperation::{operation},{m}u,{n}u);'
        if low == 4: return f'fpu_compare_equal(cpu,{m}u,{n}u);'
        if low == 5: return f'fpu_compare_greater(cpu,{m}u,{n}u);'
        if low == 6: return f'{f}=load(cpu.r[0]+{s});'
        if low == 7: return f'store({origin},cpu.r[0]+{r},{g},CodeWriteSource::Fpu);'
        if low == 8: return f'{f}=load({s});'
        if low == 9: return f'{f}=load({s});{s}+=4u;'
        if low == 0xA: return f'store({origin},{r},{g},CodeWriteSource::Fpu);'
        if low == 0xB: return f'{{const auto address={r}-4u;store({origin},address,{g},CodeWriteSource::Fpu);{r}=address;}}'
        if low == 0xC: return f'{f}={g};'
        if low == 0xD:
            if m == 2: return f'fpu_float_from_fpul(cpu,{n}u);'
            if m == 3: return f'fpu_truncate_to_fpul(cpu,{n}u);'
            if m == 4: return f'fpu_negate(cpu,{n}u);'
            if m == 5: return f'fpu_absolute(cpu,{n}u);'
            if m == 8: return f'{f}=0u;'
            if m == 9: return f'{f}=0x3F800000u;'
    raise ValueError(f'Unreviewed instruction at {pc:08X}: {op:04X}')

def emit_body(ram, instructions):
    lines = ['// Generated from the authenticated PAL TOUCH-POLY owner.',
             '// Required context: cpu, load/load16/load8, store/store16, set_t, call.',
             '// call(target) executes a complete synchronous return to cpu.pr.']
    word = lambda pc: struct.unpack_from('<H',ram,pc-BASE)[0]
    label = lambda pc: f'L{pc:08X}'
    for pc,op in sorted(instructions.items()):
        high,upper = op>>12,op>>8
        lines.append(f'{label(pc)}: {{ // {op:04X}')
        if op == 0x000B:
            lines.extend(['const auto target=cpu.pr;',emit_simple(pc+2,word(pc+2),ram),'cpu.pc=target;return true;'])
        elif high == 0xA:
            target=pc+4+2*signed(op&0xFFF,12)
            lines.extend([emit_simple(pc+2,word(pc+2),ram),f'goto {label(target)};'])
        elif upper in (0x89,0x8B,0x8D,0x8F):
            delayed=upper in (0x8D,0x8F)
            condition='cpu.t' if upper in (0x89,0x8D) else '!cpu.t'
            lines.append(f'const bool taken={condition};')
            if delayed: lines.append(emit_simple(pc+2,word(pc+2),ram))
            lines.append(f'if(taken)goto {label(pc+4+2*signed(op&255,8))};goto {label(pc+(4 if delayed else 2))};')
        elif high == 0xB or op&0xF0FF == 0x400B:
            target=f'0x{pc+4+2*signed(op&0xFFF,12):08X}u' if high==0xB else f'cpu.r[{(op>>8)&15}]'
            lines.extend([f'const auto target={target};cpu.pr=0x{pc+4:08X}u;',
                          emit_simple(pc+2,word(pc+2),ram),'call(target);',f'goto {label(pc+4)};'])
        else:
            lines.extend([emit_simple(pc,op,ram),f'goto {label(pc+2)};'])
        lines.append('}')
    return '\n'.join(line for line in lines if line)+'\n'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ram', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    ram = args.ram.read_bytes()
    if len(ram) != 0x1000000 or hashlib.sha256(ram).hexdigest() != RAM_SHA:
        parser.error('The complete retained PAL RAM identity does not match')
    instructions, delays, calls = inspect(ram)
    used = dict(instructions)
    used.update((pc, struct.unpack_from('<H', ram, pc - BASE)[0]) for pc in delays)
    groups = Counter()
    for op in used.values():
        high = op >> 12
        mask = 0xF00F if high in (0, 2, 3, 6, 15) else (0xF0FF if high == 4 else (0xFF00 if high in (8, 12) else 0xF000))
        groups[f'{op & mask:04X}/{mask:04X}'] += 1
    args.output.mkdir(parents=True, exist_ok=True)
    report = {'schema': 'sarecomp-collision-owner-v1', 'entry': f'{ENTRY:08X}',
              'end': f'{END:08X}', 'ram_sha256': RAM_SHA,
              'source_sha256': hashlib.sha256(ram[ENTRY-BASE:END-BASE]).hexdigest(),
              'source_spans': [{'address': f'{a:08X}', 'size': n,
                  'sha256': hashlib.sha256(ram[a-BASE:a-BASE+n]).hexdigest()}
                  for a, n in SOURCE_RANGES],
              'instructions': len(instructions), 'delay_slots': len(delays),
              'reachable_bytes': len(used)*2, 'calls': calls,
              'opcode_groups': dict(sorted(groups.items())),
              'reachable': {f'{pc:08X}': f'{op:04X}' for pc, op in sorted(used.items())}}
    (args.output/'touch-poly-inventory.json').write_text(json.dumps(report, indent=2)+'\n')
    body = emit_body(ram,instructions)
    (args.output/'touch-poly-body.inc').write_text(body,encoding='ascii',newline='\n')
    (args.output/'touch-poly-identities.inc').write_text(emit_identities(ram),encoding='ascii',newline='\n')
    print(f'SONIC_COLLISION_OWNER_READY instructions={len(instructions)} delays={len(delays)} calls={len(calls)} groups={len(groups)}')

if __name__ == '__main__':
    main()
