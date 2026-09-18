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
# Exact lexical HostFpuExecutionEpoch scopes of the authenticated retained AOT.
# Endpoints are exclusive. Adjacent scopes must stay separate: restoring host
# flags/rounding at their boundary is observable by the following FPU helper.
EPOCHS={
 0x8C040200:((0x8C04022A,0x8C040232),),
 0x8C040240:((0x8C040262,0x8C040268),(0x8C04026A,0x8C040278),(0x8C040282,0x8C040288)),
 0x8C0402A4:((0x8C0402AE,0x8C0402B6),(0x8C0402B8,0x8C0402BE),(0x8C0402C4,0x8C0402C8)),
 0x8C03FF2C:((0x8C03FF56,0x8C03FF5E),(0x8C03FF5E,0x8C03FF62),(0x8C03FF72,0x8C03FF76),(0x8C03FF78,0x8C03FF7E),(0x8C03FF84,0x8C03FF88)),
 0x8C03FF90:((0x8C03FFBA,0x8C03FFC2),(0x8C03FFD2,0x8C03FFD8),(0x8C03FFEA,0x8C03FFF0),(0x8C040002,0x8C040008)),
 0x8C040C5C:((0x8C040C94,0x8C040C9C),(0x8C040CB6,0x8C040CBC),(0x8C040CC2,0x8C040CC6)),
 0x8C040D60:((0x8C040D98,0x8C040DA0),(0x8C040DA2,0x8C040DA6),(0x8C040DAA,0x8C040DAE),(0x8C040DB0,0x8C040DB6),(0x8C040DC2,0x8C040DC6),(0x8C040DC8,0x8C040DCE),(0x8C040DDA,0x8C040DDE),(0x8C040DE0,0x8C040DE6)),
 0x8C0417C8:((0x8C0417F2,0x8C0417F8),(0x8C0417FA,0x8C041800),(0x8C04182A,0x8C04182E),(0x8C04183A,0x8C04183E),(0x8C041842,0x8C04184C),(0x8C041856,0x8C04185A),(0x8C04185C,0x8C041866),(0x8C041872,0x8C041876),(0x8C041878,0x8C041882),(0x8C0418A2,0x8C0418A6),(0x8C0418BA,0x8C0418C0)),
 0x8C639C34:((0x8C639C3E,0x8C639C58),(0x8C639C6A,0x8C639C82),(0x8C639C94,0x8C639CAA),(0x8C639CC0,0x8C639CD6),(0x8C639CFE,0x8C639D02),(0x8C639D2C,0x8C639D40),(0x8C639D6A,0x8C639D6E),(0x8C639D9A,0x8C639DAC),(0x8C639DD6,0x8C639DDA)),
 0x8C639F38:((0x8C639F42,0x8C639F5A),(0x8C639F6C,0x8C639F82),(0x8C639F94,0x8C639FAE),(0x8C639FC4,0x8C639FD8),(0x8C63A002,0x8C63A006),(0x8C63A032,0x8C63A044),(0x8C63A06E,0x8C63A072),(0x8C63A09E,0x8C63A0B4),(0x8C63A0DC,0x8C63A0E0)),
 0x8C63A5DC:((0x8C63A5EC,0x8C63A5FC),(0x8C63A610,0x8C63A618)),
}
OWNERS=(
 ('morph_hierarchy',0x8C040880,0x8C040880,0x8C040942),
 ('morph_draw_original',0x8C040720,0x8C040720,0x8C04073C),
 ('morph_draw_single',0x8C04073C,0x8C04073C,0x8C040760),
 ('morph_draw_double',0x8C040760,0x8C040760,0x8C040784),
 ('morph_model',0x8C040498,0x8C040498,0x8C04057A),
 ('morph_position_channel',0x8C0402D0,0x8C0402D0,0x8C04031E),
 ('morph_normal_channel',0x8C04031E,0x8C04031E,0x8C04036C),
 ('morph_key_index',0x8C03FEF2,0x8C03FEF2,0x8C03FF2C),
 ('morph_key_pair',0x8C040200,0x8C040200,0x8C040240),
 ('morph_position_array',0x8C0403C0,0x8C0403C0,0x8C040404),
 ('morph_normal_array',0x8C040404,0x8C040404,0x8C040448),
 ('morph_both_arrays',0x8C040448,0x8C040448,0x8C040498),
 ('morph_position',0x8C0402A4,0x8C0402A4,0x8C0402D0),
 ('morph_normal',0x8C040240,0x8C040240,0x8C0402A4),
 ('rigid_hierarchy',0x8C036BC0,0x8C036BC0,0x8C036F12),
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
 ('blend_hierarchy',0x8C041A2E,0x8C041A2E,0x8C041B0E),
 ('blend_static',0x8C041516,0x8C041516,0x8C04154E),
 ('blend_single',0x8C04154E,0x8C04154E,0x8C0415B4),
 ('blend_double',0x8C041600,0x8C041600,0x8C041666),
 ('blend_triple',0x8C041666,0x8C041666,0x8C0416CC),
 ('blend_quad',0x8C0416CC,0x8C0416CC,0x8C041732),
 ('blend_static_position',0x8C0414A6,0x8C0414A6,0x8C0414CC),
 ('blend_static_scale',0x8C0414CC,0x8C0414CC,0x8C0414F0),
 ('blend_static_angle',0x8C0414F0,0x8C0414F0,0x8C041516),
 ('blend_position',0x8C040DF0,0x8C040DF0,0x8C040E74),
 ('blend_scale',0x8C040E74,0x8C040E74,0x8C040EF8),
 ('blend_angle',0x8C040EF8,0x8C040EF8,0x8C040F7A),
 ('blend_key_index',0x8C040A60,0x8C040A60,0x8C040AAA),
 ('blend_float_key',0x8C040C5C,0x8C040C5C,0x8C040CD0),
 ('blend_angle_key',0x8C040D60,0x8C040D60,0x8C040DF0),
 ('blend_apply',0x8C0417C8,0x8C0417C8,0x8C04196C),
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
    if op&0xF00F==0x600E:body=f'{r}=signed8(std::uint8_t({s}));'
    elif op&0xF00F==0x200B:body=f'{r}|={s};'
    elif op&0xF00F==0x3002:body=f'cpu.t={r}>={s};'
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
    epochs=dict(EPOCHS.get(entry,()))
    interiors={p for a,b in epochs.items() for p in range(a+2,b,2)}
    # Original epochs are straight-line register-only sequences, with no
    # side exit or entry in the middle. Reject an inventory error at generation.
    for pc,op in instructions.items():
        destinations=[]
        if op>>12 in (0xA,0xB):destinations.append(pc+4+2*shared.signed(op&4095,12))
        if op>>8 in (0x89,0x8B,0x8D,0x8F):destinations.append(pc+4+2*shared.signed(op&255,8))
        if any(p in interiors for p in destinations):raise ValueError('Branch enters arithmetic epoch')
    for pc,op in sorted(instructions.items()):
        if pc in interiors:continue
        lines.append(f'{label(pc)}: {{ HIERARCHY_SITE(0x{pc:08X}u); // {op:04X}')
        if pc in epochs:
            end=epochs[pc]
            lines+=['std::optional<HostFpuExecutionEpoch> original_epoch;',
                    'const auto original_mode=cpu.read_fpscr();',
                    'if((original_mode&(fpscr_exception_enable_mask|fpscr_dn_mask))==fpscr_dn_mask && '
                    '(original_mode&fpscr_rounding_mode_mask)<=1u'+
                    ('' if pc==0x8C03FF72 else ' && !(original_mode&fpscr_pr_mask)')+')original_epoch.emplace(cpu);']
            for p in range(pc,end,2):
                if p not in instructions:raise ValueError('Missing epoch instruction')
                o=instructions[p]
                if not (o>>12==0xF and (o&15) not in (6,7,8,9,10,11)):
                    raise ValueError(f'Non-register FPU instruction in epoch: {p:08X} {o:04X}')
                lines.append(emit_simple(p,o,ram))
            lines += [f'goto {label(end)};','}']
            continue
        high,upper=op>>12,op>>8
        if (high==0xA and pc+4+2*shared.signed(op&4095,12)<=pc) or (upper in (0x89,0x8B,0x8D,0x8F) and pc+4+2*shared.signed(op&255,8)<=pc):
            lines.append(f'backedge(0x{pc:08X}u);')
        at=f'RestartPoint{{0x{pc:08X}u}}'
        delay=lambda at:emit_simple(pc+2,word(pc+2),ram,at)
        if op==0xB:lines+=['const auto target=cpu.pr;',delay(at),f'return_site=0x{pc:08X}u;cpu.pc=target;return;']
        elif op&0xF0FF==0x402B:
            lines+=[f'const auto target=cpu.r[{(op>>8)&15}];',delay(at),'call(target,true);return;']
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
    args.output.mkdir(parents=True,exist_ok=True);spans=[];reports=[];dependencies={}
    epoch_rows=['struct OriginalEpoch {std::uint32_t begin,end;bool single;};','constexpr OriginalEpoch original_epochs[]{']
    epoch_rows += [f'{{0x{a:08X}u,0x{b:08X}u,{str(a!=0x8C03FF72).lower()}}},' for scopes in EPOCHS.values() for a,b in scopes]
    epoch_rows.append('};')
    (args.output/'hierarchy-epochs.inc').write_text('\n'.join(epoch_rows)+'\n',encoding='ascii',newline='\n')
    for name,entry,begin,end in OWNERS:
        ins,delays,calls=inspect(ram,entry,begin,end)
        used=dict(ins);used.update({pc:struct.unpack_from('<H',ram,pc-shared.BASE)[0] for pc in delays})
        spans.append((begin,end-begin));literals=set()
        for pc,op in used.items():
            if op>>12==0xD:literals.add((((pc+4)&~3)+(op&255)*4,4))
            if op>>12==9:literals.add((pc+4+(op&255)*2,2))
            if op>>8==0xC7:literals.add((((pc+4)&~3)+(op&255)*4,4))
        spans+=sorted(literals)
        # Authentication covers the same bytes, but adjacent literal words and
        # literals already inside the owner's body need no separate memcmp.
        # This is static range coalescing, not a proof surviving a callback.
        dependencies[name]=[]
        original_dependencies=[(begin,end-begin),*sorted(literals)]
        for address,size in sorted(original_dependencies):
            owner_spans=dependencies[name]
            if owner_spans and address<=sum(owner_spans[-1]):
                previous,length=owner_spans[-1]
                owner_spans[-1]=(previous,max(previous+length,address+size)-previous)
            else:owner_spans.append((address,size))
        original_bytes={p for a,n in original_dependencies for p in range(a,a+n)}
        merged_bytes={p for a,n in dependencies[name] for p in range(a,a+n)}
        if original_bytes!=merged_bytes:raise ValueError('Owner authentication coverage changed')
        (args.output/f'hierarchy-{name}.inc').write_text(emit_body(ram,entry,ins),encoding='ascii',newline='\n')
        reports.append(dict(name=name,entry=f'{entry:08X}',begin=f'{begin:08X}',end=f'{end:08X}',instructions=len(ins),delays=len(delays),calls=calls))
    merged=[]
    for start,size in sorted(spans):
        if merged and start<=sum(merged[-1]):merged[-1]=(merged[-1][0],max(start+size,sum(merged[-1]))-merged[-1][0])
        else:merged.append((start,size))
    proof=[shared.emit_identities(ram,merged)]
    for name,entry,begin,end in OWNERS:
        proof.append(f'constexpr std::array<SourceSpan,{len(dependencies[name])}> source_{name}{{{{')
        for address,size in dependencies[name]:
            matches=[(i,a) for i,(a,n) in enumerate(merged) if a<=address and address+size<=a+n]
            if len(matches)!=1:raise ValueError('Owner source dependency is not uniquely authenticated')
            i,a=matches[0]
            proof.append(f'    {{0x{address:08X}u,std::span{{identity_{i}}}.subspan({address-a}u,{size}u)}},')
        proof.append('}};')
    proof.append('constexpr std::array owner_sources{')
    proof+= [f'    std::span<const SourceSpan>{{source_{name}}},' for name,_,_,_ in OWNERS]
    proof.append('};\nunsigned source_owner_index(std::uint32_t owner) noexcept { switch(owner){')
    proof += [f'case 0x{entry:08X}u:return {i}u;' for i,(_,entry,_,_) in enumerate(OWNERS)]
    proof.append(f'default:return {len(OWNERS)}u;\n}}}}')
    proof.append('constexpr auto source_pages=[] { std::array<bool,4096> out{};')
    proof.append('for(const auto& s:identities)for(auto p=(s.address&0xFFFFFFu)>>12u;p<=((s.address&0xFFFFFFu)+s.bytes.size()-1u)>>12u;++p)out[p]=true;return out;}();')
    (args.output/'hierarchy-identities.inc').write_text('\n'.join(proof)+'\n',encoding='ascii',newline='\n')
    (args.output/'hierarchy-switch.inc').write_text('\n'.join(f'case 0x{entry:08X}u: {{\n#include "hierarchy-{name}.inc"\n}}' for name,entry,_,_ in OWNERS)+'\n')
    (args.output/'hierarchy-members.inc').write_text('\n'.join(f'case 0x{entry:08X}u:' for _,entry,_,_ in OWNERS)+'\nreturn true;\n')
    (args.output/'hierarchy-inventory.json').write_text(json.dumps(dict(schema='sarecomp-render-hierarchy-v1',ram_sha256=shared.RAM_SHA,owners=reports,source_spans=[dict(address=f'{a:08X}',size=n,sha256=hashlib.sha256(ram[a-shared.BASE:a-shared.BASE+n]).hexdigest()) for a,n in merged]),indent=2)+'\n')
    print(f'SONIC_RENDER_HIERARCHY_READY owners={len(reports)} instructions={sum(x["instructions"] for x in reports)}')
if __name__=='__main__':main()
