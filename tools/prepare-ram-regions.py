"""Mixed, callback-free RAM/ALU prefixes; retained AOT is the exact fallback.

Called only by prepare-scalar-writes.py after full generation/unit SHA binding.
No code discovery or heuristic opcode recovery happens in this transformation.
"""
import importlib.util
import re
from pathlib import Path

spec = importlib.util.spec_from_file_location('retained_reads', Path(__file__).with_name('prepare-ram-read-aot.py'))
reads = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reads)
START = re.compile(r'(?m)^( +)\{\n\1    // katana-guest 0x([0-9A-F]{8})u\n')
REG = r'(?:katana_registers\[(?:[0-9]|1[0-5])\]|cpu\.fr\[(?:[0-9]|1[0-5])\])'
ACCOUNT = re.compile(r'\n +cpu\.attempted_guest_instructions \+= (\d+)u;\n +cpu\.retired_guest_instructions \+= (\d+)u;\n +cpu\.pending_guest_cycles \+= (\d+)u;')


def expression(text):
    """Only side-effect-free integer expressions present in this generation."""
    remainder = re.sub(REG, '', text)
    remainder = remainder.replace('katana_registers.gbr()', '')
    remainder = re.sub(r'katana::runtime::relocate_code_address_inline\(0x[0-9A-F]{8}u\)', '', remainder)
    remainder = re.sub(r'static_cast<std::(?:u?int(?:8|16|32)_t)>', '', remainder)
    remainder = re.sub(r'0x[0-9A-Fa-f]+u?|[0-9]+u?', '', remainder)
    return not re.search(r'[^\s()+\-~&|^<>]', remainder)


def scalar_operation(body):
    m = re.fullmatch(r'('+REG+r') = katana_direct_ram_read_(s8|s16|u32)\(guest_origin, (.+), (true|katana_guarded_unknown_ram_reads)\);', body.strip())
    if m and expression(m[3]):
        bits = int(m[2][1:])
        return {'operation': f'access.read<{bits},{str(m[2][0]=="s").lower()}>({m[3]}, {m[1]})', 'memory':True, 'store':False}
    m = re.fullmatch(r'katana_direct_ram_write_u(8|16|32)\(katana_direct_ram_writes, guest_origin, (.+), (.+), katana::runtime::CodeWriteSource::(?:Cpu|Fpu), (true|katana_guarded_unknown_ram_writes)\);', body.strip())
    if m and expression(m[2]) and expression(m[3]):
        return {'operation':f'access.write<{m[1]}>({m[2]}, {m[3]})', 'memory':True, 'store':True}
    m = re.fullmatch(r'('+REG+r') (=|\+=|-=|&=|\|=|\^=) (.+);', body.strip())
    if m and expression(m[3]):
        return {'operation':body.strip(), 'memory':False, 'store':False}
    return None


def extended_alu(body):
    """Callback-free integer shapes absent from the initial region pilot."""
    body = body.strip()
    if body == '/* nop */':
        return dict(operation=';', memory=False, store=False)
    m = re.fullmatch(r'(katana_registers\[(?:[0-9]|1[0-5])\]) (<<=|>>=) (1|2|8|16)u;', body)
    if m:
        return dict(operation=body, memory=False, store=False)
    m = re.fullmatch(r'katana_registers\.t\(\) = (.+) (==|!=|>=|<=|>|<) (.+);', body)
    if m and expression(m[1]) and expression(m[3]):
        return dict(operation=body, memory=False, store=False)
    return None


def extended_memory(body):
    """Keep postincrement/predecrement publication after the successful access."""
    gpr = r'katana_registers\[(?:[0-9]|1[0-5])\]'
    value = r'(?:' + gpr + r'|katana_registers\.pr\(\))'
    m = re.fullmatch(
        r'\{\s*const bool same_register = (true|false);\s*'
        r'const std::uint32_t address = (' + gpr + r');\s*'
        r'const std::uint32_t value =\s*katana_direct_ram_read_u32\(guest_origin, address, katana_guarded_unknown_ram_reads\);\s*'
        r'(' + gpr + r') = value;\s*if \(!same_register\) \{\s*'
        r'(' + gpr + r') = address \+ 4u;\s*\}\s*\}', body.strip())
    if m and m[2] == m[4] and (m[1] == 'true') == (m[2] == m[3]):
        return dict(operation=f'access.read<32>(sonic_address, {m[3]})',
                    before=f'const auto sonic_address = {m[2]};',
                    after=f'{m[2]} = sonic_address + 4u;' if m[1] == 'false' else '',
                    memory=True, store=False, extended_kind='postincrement')
    m = re.fullmatch(
        r'\{\s*const std::uint32_t value = (' + value + r');\s*'
        r'const std::uint32_t address = (' + gpr + r') - 4u;\s*'
        r'katana_direct_ram_write_u32\(katana_direct_ram_writes, guest_origin, address, value, '
        r'katana::runtime::CodeWriteSource::Cpu, katana_guarded_unknown_ram_writes\);\s*'
        r'(' + gpr + r') = address;\s*\}', body.strip())
    if m and m[2] == m[3]:
        return dict(operation=f'access.write<32>(sonic_address, {m[1]})',
                    before=f'const auto sonic_address = {m[2]} - 4u;',
                    after=f'{m[2]} = sonic_address;', memory=True, store=True,
                    extended_kind='predecrement')
    m = re.fullmatch(
        r'\{\s*const std::uint32_t address = (' + gpr + r');\s*'
        r'const std::uint32_t value = katana_direct_ram_read_u32\(guest_origin, address, katana_guarded_unknown_ram_reads\);\s*'
        r'(' + gpr + r') = address \+ 4u;\s*katana_registers\.pr\(\) = value;\s*\}', body.strip())
    if m and m[1] == m[2]:
        return dict(operation='access.read<32>(sonic_address, sonic_value)',
                    before=f'const auto sonic_address = {m[1]}; std::uint32_t sonic_value = 0;',
                    after=f'{m[1]} = sonic_address + 4u; katana_registers.pr() = sonic_value;',
                    memory=True, store=False, extended_kind='pr_postincrement')
    return None


def extended_fmov_predecrement(body):
    """Admit the exact scalar half of an authenticated FMOV predecrement."""
    m = re.fullmatch(
        r'\{\s*const std::uint32_t width = \(cpu.fpscr & katana::runtime::fpscr_sz_mask\) != 0u \? 8u : 4u;\s*'
        r'const std::uint32_t address = katana_registers\[(\d+)\] - width;\s*'
        r'if \(\(cpu.fpscr & katana::runtime::fpscr_sz_mask\) != 0u\) \{\s*'
        r'const std::uint64_t bits = katana::runtime::read_fpu_pair_bits\(cpu, (\d+)u\);\s*'
        r'katana_direct_ram_write_u32\(katana_direct_ram_writes, guest_origin, address, static_cast<std::uint32_t>\(bits\), katana::runtime::CodeWriteSource::Fpu, katana_guarded_unknown_ram_writes\);\s*'
        r'katana_direct_ram_write_u32\(katana_direct_ram_writes, guest_origin, address \+ 4u, static_cast<std::uint32_t>\(bits >> 32u\), katana::runtime::CodeWriteSource::Fpu, katana_guarded_unknown_ram_writes\);\s*'
        r'\} else \{\s*'
        r'katana_direct_ram_write_u32\(katana_direct_ram_writes, guest_origin, address, cpu.fr\[(\d+)\], katana::runtime::CodeWriteSource::Fpu, katana_guarded_unknown_ram_writes\);\s*'
        r'\}\s*katana_registers\[(\d+)\] = address;\s*\}', body.strip())
    if not m or m[1] != m[4] or m[2] != m[3] or any(int(x) > 15 for x in m.groups()):
        return None
    return dict(operation=f'access.write<32>(sonic_address, cpu.fr[{m[3]}])',
                before=f'const auto sonic_address = katana_registers[{m[1]}] - 4u;',
                after=f'katana_registers[{m[1]}] = sonic_address;', memory=True,
                store=True, extended_kind='fmov_predecrement')


def classify(block, extended=False):
    # No delay-slot, branch or nested accounting group can match. The
    # instruction comments themselves are never sufficient.
    attempts = re.findall(r'ExplicitGuestInstructionAttempt guest_instruction_attempt\(\s*cpu, (?:katana_instruction_runtime_pc|katana::runtime::relocate_code_address_inline\(0x[0-9A-F]+u\)), (\d+)u\);', block)
    inner = block[block.index('\n', block.index('// katana-guest'))+1:block.rfind('}')].strip()
    if not attempts:
        if 'ExplicitGuestInstructionAttempt' in block: return None
        result = scalar_operation(inner) or (extended_alu(inner) if extended else None)
        if result and not result['memory'] and 'cpu.fr' not in inner:
            return dict(result, cycles=1, attempt=False, fmov=False)
        return None
    if len(attempts) != 1 or block.count('guest_instruction_attempt.complete();') != 1:
        return None
    if extended and attempts == ['1']:
        simple = re.fullmatch(
            r'katana::runtime::ExplicitGuestInstructionAttempt guest_instruction_attempt\(\s*'
            r'cpu, katana::runtime::relocate_code_address_inline\(0x[0-9A-F]{8}u\), 1u\);\s*'
            r'(.+?)\s*guest_instruction_attempt\.complete\(\);', inner, re.S)
        if simple:
            result = scalar_operation(simple[1]) or extended_alu(simple[1])
            if result and not result['memory'] and 'cpu.fr' not in simple[1]:
                return dict(result, cycles=1, attempt=True, fmov=False, extended_kind='simple_attempt')
    # Exact ordinary arithmetic envelope. In the admitted scalar DN=1 mode,
    # with enables clear, these original helpers cannot call out or trap.
    fp_call = (r'katana::runtime::fpu_(?:binary\(cpu, katana::runtime::FpuBinaryOperation::'
               r'(?:Add|Subtract|Multiply|Divide), (?:[0-9]|1[0-5])u, (?:[0-9]|1[0-5])u'
               r'|(?:square_root|reciprocal_square_root)\(cpu, (?:[0-9]|1[0-5])u), std::nullopt\)')
    arithmetic = re.fullmatch(
        r'katana_registers\.flush_release\(\);\s*'
        r'katana::runtime::ExplicitGuestInstructionAttempt guest_instruction_attempt\(\s*'
        r'cpu, katana::runtime::relocate_code_address_inline\(0x[0-9A-F]{8}u\), (\d+)u\);\s*'
        r'if \(\(cpu\.sr & katana::runtime::sr_fd_mask\) != 0u\) \{\s*'
        r'raise_fpu_disabled\(cpu, katana::runtime::relocate_code_address_inline\(0x[0-9A-F]{8}u\)\);\s*return;\s*\}\s*'
        r'if \(([^;{}]+)\) \{\s*'
        r'raise_illegal_instruction\(cpu, katana::runtime::relocate_code_address_inline\(0x[0-9A-F]{8}u\)\);\s*return;\s*\}\s*'
        r'if \(('+fp_call+r')\) \{\s*return;\s*\}\s*'
        r'guest_instruction_attempt\.complete\(\);\s*katana_registers\.reload_acquire\(\);', inner)
    if arithmetic:
        # Only legality checks implied by PR=0 and legal RM are admissible.
        legality = arithmetic[2]
        if legality not in (
            '((cpu.fpscr & katana::runtime::fpscr_rounding_mode_mask) > 1u)',
            '(cpu.fpscr & katana::runtime::fpscr_pr_mask) != 0u || ((cpu.fpscr & katana::runtime::fpscr_rounding_mode_mask) > 1u)'):
            return None
        return dict(operation=arithmetic[3].replace(', std::nullopt)', ');'),
                    memory=False, store=False, cycles=int(arithmetic[1]), attempt=True,
                    fmov=False, arithmetic=True)
    if attempts != ['2']: return None
    fmov = 'raise_fpu_disabled(' in block
    if extended:
        constant = re.fullmatch(
            r'katana::runtime::ExplicitGuestInstructionAttempt guest_instruction_attempt\(\s*'
            r'cpu, katana::runtime::relocate_code_address_inline\(0x[0-9A-F]{8}u\), 2u\);\s*'
            r'if \(\(cpu.sr & katana::runtime::sr_fd_mask\) != 0u\) \{\s*'
            r'katana_registers.flush_release\(\);\s*'
            r'raise_fpu_disabled\(cpu, katana::runtime::relocate_code_address_inline\(0x[0-9A-F]{8}u\)\);\s*return;\s*\}\s*'
            r'if \(\(cpu.fpscr & katana::runtime::fpscr_pr_mask\) != 0u\) \{\s*'
            r'katana_registers.flush_release\(\);\s*'
            r'raise_illegal_instruction\(cpu, katana::runtime::relocate_code_address_inline\(0x[0-9A-F]{8}u\)\);\s*return;\s*\}\s*'
            r'(cpu.fr\[(?:[0-9]|1[0-5])\] = (?:0x00000000u|0x3F800000u);)\s*'
            r'guest_instruction_attempt.complete\(\);', inner)
        if constant:
            return dict(operation=constant[1], memory=False, store=False, cycles=2,
                        attempt=True, fmov=True, strict_fmov=True, extended_kind='fldi')
    if 'try {' in block:
        origin = re.search(r'const katana::runtime::GuestInstructionOrigin guest_origin\{[^\n]+\};\n', block)
        if not origin or block.count('try {') != 1 or block.count('enter_memory_exception_with_provenance(') != 1:
            return None
        body = block[origin.end():block.index('guest_instruction_attempt.complete();')].strip()
        if fmov and extended:
            result = extended_fmov_predecrement(body)
            if result:
                if 'if (katana_guest_write_exit_requested)' not in block or block.count('flush_pending_guest_cycles(') > 1:
                    return None
                return dict(result, cycles=2, attempt=True, fmov=True)
        if fmov:
            # Scalar FMOV without predecrement/postincrement; paired mode is
            # kept in the untouched original. No float conversion is used.
            m = re.fullmatch(r'\{\s*const std::uint32_t address = (.+);\nif \(\(cpu.fpscr & katana::runtime::fpscr_sz_mask\) != 0u\) \{\n.*?\n\} else \{\n([^{}]+)\n\}\n(?:(katana_registers\[\d+\]) = address \+ \(\(cpu.fpscr & katana::runtime::fpscr_sz_mask\) != 0u \? 8u : 4u\);\n)?\}', body, re.S)
            if not m or not expression(m[1]): return None
            body = m[2].strip().replace('address', 'sonic_address' if m[3] else m[1])
            # Postincrement commits only after a successful load. Keep the
            # pre-load address even if later admitted shapes reuse a register.
            if m[3]:
                result=scalar_operation(m[2].strip().replace('address',m[1]))
                if not result or result['store']: return None
                result['operation']=result['operation'].replace(m[1]+',','sonic_address,',1)
                return dict(result,cycles=2,attempt=True,fmov=True,
                            before=f'const auto sonic_address = {m[1]};',after=f'{m[3]} = sonic_address + 4u;')
        result = scalar_operation(body) or (extended_memory(body) if extended and not fmov else None)
        if not result or not result['memory']: return None
        if result['store'] and 'if (katana_guest_write_exit_requested)' not in block: return None
        if block.count('flush_pending_guest_cycles(') > 1: return None
        return dict(result, cycles=2, attempt=True, fmov=fmov)
    if fmov:
        m = re.search(r'\} else \{\n    (cpu\.fr\[\d+\] = cpu\.fr\[\d+\];)\n\}\s*guest_instruction_attempt.complete\(\);', block)
        if m:
            return dict(scalar_operation(m[1]), cycles=2, attempt=True, fmov=True)
    return None


def gap_ok(source, left, right):
    if left['pc'] + 2*left.get('weight',1) != right['pc'] or left['indent'] != right['indent']: return False
    gap = source[left['end']:right['start']]
    return bool(re.fullmatch(r'\s*(?:katana_block_'+f'{right["pc"]:08X}'+r'_resume:\s*)?', gap))


def compact(text):
    return re.sub(r'\s+','',re.sub(r'//[^\n]*','',text))


def read_groups(source):
    """Treat the retained, already atomic FMOV read groups as whole atoms."""
    result=[]
    pattern=r'(?m)^( +)\{\n\1    // Fully validated, callback-free FMOV.S read group\n'
    for m in re.finditer(pattern,source):
        end=source.index('\n'+m[1]+'}',m.end())+2+len(m[1])
        body=source[m.start():end]
        size=re.search(r'std::array<std::uint32_t, (\d+)u>',body)
        base=re.search(r'katana_fmov_group_virtual_address = katana_registers\[(\d+)\];',body)
        jump=re.search(r'goto katana_block_([0-9A-F]{8})_fmov_group_end;',body)
        registers=re.findall(r'cpu.fr\[(\d+)\] = katana_fmov_group_values\[(\d+)u\];',body)
        if not(size and base and jump):continue
        n=int(size[1]); first=int(jump[1],16); last=first+2*(n-1)
        if not 2<=n<=16 or [int(x[1]) for x in registers]!=list(range(n)):continue
        assignments='\n'.join(f'cpu.fr[{r}] = katana_fmov_group_values[{i}u];' for r,i in registers)
        expected=f'''{{
std::array<std::uint32_t, {n}u> katana_fmov_group_values{{}};
const auto katana_fmov_group_virtual_address = katana_registers[{base[1]}];
std::uint32_t katana_fmov_group_direct_address = 0u;
if ((cpu.sr & katana::runtime::sr_fd_mask) == 0u &&
    (cpu.fpscr & (katana::runtime::fpscr_pr_mask | katana::runtime::fpscr_sz_mask)) == 0u &&
    katana_direct_ram_translate(katana_fmov_group_virtual_address, katana_fmov_group_direct_address) &&
    katana::runtime::direct_linear_guard_read_u32_group(katana_direct_ram,katana_fmov_group_direct_address,katana_fmov_group_values)) {{
{assignments}
katana_registers[{base[1]}] = katana_fmov_group_virtual_address + {4*n}u;
{{
cpu.active_instruction_pc = katana::runtime::relocate_code_address_inline(0x{last:08X}u);
const auto katana_fpu_block_offset = cpu.active_instruction_pc - cpu.active_block_virtual_start;
cpu.active_instruction_physical_pc = cpu.active_block_size != 0u && katana_fpu_block_offset < cpu.active_block_size
 ? cpu.active_block_physical_start + katana_fpu_block_offset : katana::runtime::canonical_physical_address_inline(cpu.active_instruction_pc);
}}
cpu.attempted_guest_instructions += {n}u;
cpu.retired_guest_instructions += {n}u;
cpu.pending_guest_cycles += {2*n}u;
goto katana_block_{first:08X}_fmov_group_end;
}}
}}'''
        if compact(body)!=compact(expected):continue
        finish_label=f'katana_block_{first:08X}_fmov_group_end:;'
        finish=source.index(finish_label,end)+len(finish_label)
        # Original resume labels inside the fallback remain available. No
        # branch or unrelated instruction may be swallowed by an atom.
        pcs=[int(i[2],16) for i in START.finditer(source,end,finish)]
        if pcs!=list(range(first,last+2,2)):continue
        before=f'std::array<std::uint32_t,{n}> sonic_values{{}};'
        after=assignments.replace('katana_fmov_group_values','sonic_values')+f'\nkatana_registers[{base[1]}] += {n*4}u;'
        result.append(dict(valid=True,start=m.start(),end=finish,indent=m[1],pc=first,
                           accounting=None,weight=n,cycles=2*n,attempt=True,fmov=True,strict_fmov=True,
                           memory=True,store=False,before=before,after=after,
                           operation=f'access.read_group<{n}>(katana_registers[{base[1]}],sonic_values)'))
    return result


def instructions(source, extended=False):
    special=read_groups(source)
    nodes = list(special)
    for m in START.finditer(source):
        if any(n['start']<=m.start()<n['end'] for n in special): continue
        close = source.find('\n'+m[1]+'}', m.end())
        if close < 0: raise ValueError('Unclosed instruction')
        end = close + 2 + len(m[1])
        block = source[m.start():end]
        shape = classify(block, extended)
        node = dict(shape or {}, valid=bool(shape), start=m.start(), end=end,
                    indent=m[1], pc=int(m[2],16), accounting=None)
        account = ACCOUNT.match(source, end)
        if account:
            node['accounting'] = tuple(map(int,account.groups()))
            node['end'] = account.end()
        nodes.append(node)
    # Integer instructions are billed as complete runs. Do not split a run or
    # include preceding unsupported ALU work in the optimized accounting.
    atoms = []; pending = []
    for node in sorted(nodes,key=lambda n:n['start']):
        if not node['valid']:
            pending=[]; continue
        if node['attempt']:
            pending=[]
            if node['accounting'] is None: atoms.append([node])
            continue
        if pending and not gap_ok(source,pending[-1],node): pending=[]
        pending.append(node)
        if node['accounting'] is not None:
            if node['accounting'] == (len(pending),)*3: atoms.append(pending)
            pending=[]
    return atoms


def localize(text, live_fpu=False):
    text = re.sub(r'katana_registers\[(\d+)\]',r'r\1',text)
    text = re.sub(r'katana_registers\.(pr|t)\(\)', r's_\1', text)
    return text if live_fpu else re.sub(r'cpu\.fr\[(\d+)\]',r'f\1',text)


def emit(group, key):
    indent=group[0]['indent']; prefix=[]
    joined='\n'.join(i.get('before','')+i['operation']+i.get('after','') for i in group)
    gprs=sorted(set(map(int,re.findall(r'katana_registers\[(\d+)\]',joined))))
    fprs=sorted(set(map(int,re.findall(r'cpu\.fr\[(\d+)\]',joined))))
    scalars=sorted(set(re.findall(r'katana_registers\.(pr|t)\(\)',joined)))
    live_fpu=any(i.get('arithmetic',False) for i in group)
    fp_guard = (' && sonic::ram_regions::arithmetic_admitted(cpu)' if live_fpu else
                ' && (cpu.sr & katana::runtime::sr_fd_mask) == 0u && (cpu.fpscr & katana::runtime::fpscr_sz_mask) == 0u' if fprs else '')
    if not live_fpu and any(i.get('strict_fmov') for i in group):
        fp_guard+=' && (cpu.fpscr & katana::runtime::fpscr_pr_mask) == 0u'
    prefix += ['{', '    if (sonic::scalar_writes::ram_regions_enabled() && katana_registers.owns_registers() &&',
               '        !cpu.trap_pending && !katana_guest_write_exit_requested && katana_direct_ram_writes == nullptr'+
               fp_guard+') {']
    prefix += [f'        auto r{r} = katana_registers[{r}];' for r in gprs]
    prefix += [f'        auto s_{name} = katana_registers.{name}();' for name in scalars]
    if not live_fpu: prefix += [f'        auto f{r} = cpu.fr[{r}];' for r in fprs]
    prefix += ['        sonic::ram_regions::Access access(cpu,katana_direct_ram,katana_direct_ram_code_tracker);',
               '        const auto stopped = [&]() -> unsigned {']
    if live_fpu: prefix += ['            katana::runtime::HostFpuExecutionEpoch sonic_epoch(cpu);']
    cycles=[0]; pcs=[0]; counts=[0]; last=0
    for n,inst in enumerate(group):
        operation=localize(inst['operation'],live_fpu)
        prefix += ['            {']
        if inst.get('before'):prefix += ['            '+localize(inst['before'],live_fpu)]
        prefix += [f'            if (!{operation}) return {n}u;' if inst['memory'] else '            '+operation]
        if inst.get('after'):prefix += ['            '+localize(inst['after'],live_fpu)]
        prefix += ['            }']
        cycles.append(cycles[-1]+inst['cycles'])
        counts.append(counts[-1]+inst.get('weight',1))
        if inst['attempt']: last=inst['pc']+2*(inst.get('weight',1)-1)
        pcs.append(last)
    prefix += [f'            return {len(group)}u;', '        }();']
    prefix += [f'        katana_registers[{r}] = r{r};' for r in gprs]
    prefix += [f'        katana_registers.{name}() = s_{name};' for name in scalars]
    if not live_fpu: prefix += [f'        cpu.fr[{r}] = f{r};' for r in fprs]
    prefix += ['        static constexpr unsigned cycles[] = {'+','.join(map(str,cycles))+'};',
               '        static constexpr unsigned counts[] = {'+','.join(map(str,counts))+'};',
               '        static constexpr std::uint32_t attempts[] = {'+','.join(f'0x{x:08X}u' for x in pcs)+'};',
               '        sonic::ram_regions::complete(cpu,counts[stopped],cycles[stopped],attempts[stopped]);',
               '        switch (stopped) {']
    labels=[]
    for n,inst in enumerate(group):
        if inst['memory']:
            label=f'sonic_ram_{key}_{n}'
            prefix += [f'        case {n}u: goto {label};']
            labels.append((inst['start'],indent+label+': ;\n'))
    prefix += [f'        default: goto sonic_ram_{key}_end;', '        }', '    }', '}']
    code='\n'.join(indent+line for line in prefix)+'\n'
    # A label for the first instruction must follow, not precede, the prefix.
    labels.append((group[-1]['end'],'\n'+indent+f'sonic_ram_{key}_end: ;'))
    return code,labels


def transform(source, extended=False):
    reads.validate_preloaded_helpers(source)
    if 'static constexpr bool katana_guarded_unknown_ram_writes = false;' in source:
        raise ValueError('Original store permission differs')
    groups=[]
    for atom in instructions(source, extended):
        if groups and sum(n.get('weight',1) for n in groups[-1]+atom)<=128 and gap_ok(source,groups[-1][-1],atom[0]):
            groups[-1].extend(atom)
        else: groups.append(list(atom))
    edits=[];report=[]
    for group in groups:
        size=sum(i.get('weight',1) for i in group)
        memory=sum(i.get('weight',1) for i in group if i['memory'])
        if size<8 or memory<4: continue
        first=group[0]
        owner=source.rfind('\nBlockExit fn_',0,first['start'])
        # Every admitted owner uses the authenticated native read/write helpers.
        if owner<0 or 'bool katana_guest_write_exit_requested = false;' not in source[owner:first['start']]: continue
        key=f'{first["pc"]:08X}_{len(report)}'
        prefix,labels=emit(group,key)
        # Stable ordering at equal source offsets puts the prefix BEFORE the
        # original instruction label, allowing a miss to bypass it entirely.
        edits.append((first['start'],0,prefix))
        edits.extend((where,1,text) for where,text in labels)
        report.append({'pc':f'{first["pc"]:08X}','last_pc':f'{group[-1]["pc"]+2*(group[-1].get("weight",1)-1):08X}',
                       'instructions':size,'memory':memory,
                       'stores':sum(i['store'] for i in group),
                       'arithmetic':sum(i.get('arithmetic',False) for i in group)})
    result=source
    for where,order,text in sorted(edits,reverse=True): result=result[:where]+text+result[where:]
    restored=result
    for _,_,text in edits: restored=restored.replace(text,'',1)
    if restored!=source: raise ValueError('Original instruction path changed')
    return ('#include "sonic_ram_regions.hpp"\n'+result if report else source),report
