"""Optional, source-bound experiment; never modify the retained AOT sources."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from string import Template

UNITS = {
    "unit-v8C029400-8C029B00-9bed0201322da5d9.cpp":
        "d99c58370239f5009140005a238795a5dd6a0e434ba9ffe068857deaa556b9d1",
    "unit-v8C036BC0-8C037C3C-aa2f5ddfed3d4270.cpp":
        "c34ed098e7625b5a432263ebf286ae486cb86b534dad0cdb97d4991f2253e45a",
}
# Exact ordinary-instruction envelope. Delay slots use a different fault owner
# and deferred safepoint; postincrements, FPU accesses and extra work cannot fit.
ENVELOPE = Template("""{
    // katana-guest 0x${pc}u
    const bool katana_guarded_linear_access_${pc} = katana_direct_ram_can_read(${address}, 4u);
    if (!katana_guarded_linear_access_${pc} && services != nullptr) {
        katana_registers.flush_release();
        katana::runtime::flush_pending_guest_cycles(cpu, *services);
        katana_registers.reload_acquire();
    }
    std::uint64_t katana_mmio_boundary_epoch_before = 0u;
    if (!katana_guarded_linear_access_${pc}) {
        katana_mmio_boundary_epoch_before = cpu.memory.mmio_boundary_epoch();
    }
    const auto katana_instruction_runtime_pc = katana::runtime::relocate_code_address_inline(0x${pc}u);
    katana::runtime::ExplicitGuestInstructionAttempt guest_instruction_attempt(
        cpu, katana_instruction_runtime_pc, 2u);
    try {
        const katana::runtime::GuestInstructionOrigin guest_origin{0x${pc}u, katana_instruction_runtime_pc, true};
        katana_registers[${destination}] = katana_direct_ram_read_u32(guest_origin, ${address}, ${allow});
        guest_instruction_attempt.complete();
    } catch (const katana::runtime::MemoryAccessError& error) {
        enter_memory_exception_with_provenance(cpu, error, katana::runtime::relocate_code_address_inline(0x${pc}u), 0x${opcode}u);
        return;
    }
    if (services != nullptr && !katana_guarded_linear_access_${pc} && cpu.memory.mmio_boundary_epoch() != katana_mmio_boundary_epoch_before) {
        katana_registers.flush_release();
        cpu.pc = katana::runtime::relocate_code_address_inline(0x${next_pc}u);
        if (katana_commit_post_instruction_safepoint(
            cpu, *services, katana::runtime::relocate_code_address_inline(0x${pc}u))) return;
        katana_registers.reload_acquire();
    }
}""")
START = re.compile(r"(?m)^(?P<indent> +)\{\n(?P=indent)    // katana-guest 0x(?P<pc>[0-9A-F]{8})u\n"
    r"(?P=indent)    const bool katana_guarded_linear_access_(?P=pc) = katana_direct_ram_can_read\((?P<address>[^\n]+), 4u\);")
LOAD = re.compile(r"katana_registers\[(\d+)\] = katana_direct_ram_read_u32\(guest_origin, ([^\n]+), (katana_guarded_unknown_ram_reads|true)\);")
OPCODE = re.compile(r"enter_memory_exception_with_provenance\(cpu, error, katana::runtime::relocate_code_address_inline\(0x[0-9A-F]{8}u\), 0x([0-9A-F]{8})u\);")
FPU_START = re.compile(r"(?m)^(?P<indent> +)\{\n(?P=indent)    // katana-guest 0x(?P<pc>[0-9A-F]{8})u\n"
    r"(?P=indent)    const bool katana_guarded_linear_access_(?P=pc) = katana_direct_ram_can_read\((?P<address>[^\n]+?), 4u\) &&")
FPU_BODY = Template("""const std::uint32_t address = ${address};
if ((cpu.fpscr & katana::runtime::fpscr_sz_mask) != 0u) {
    const std::uint32_t low = katana_direct_ram_read_u32(guest_origin, address, katana_guarded_unknown_ram_reads);
    const std::uint32_t high = katana_direct_ram_read_u32(guest_origin, address + 4u, katana_guarded_unknown_ram_reads);
    katana::runtime::write_fpu_pair_bits(cpu, ${destination}u, (static_cast<std::uint64_t>(high) << 32u) | low);
} else {
    cpu.fr[${destination}] = katana_direct_ram_read_u32(guest_origin, address, katana_guarded_unknown_ram_reads);
}""")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def validate_preloaded_helpers(source):
    # Moving the read requires more than matching its instruction envelope.
    # Bind the known callback-free admission and successful-read helpers too;
    # the two retained fallback shapes are deliberately left unrestricted.
    expected = {
        'translate':'2e7cd8070de52f7ca1f1c93352dcffe7cab9e68dc3df2ee43ae646cfebbe1d94',
        'resolve':'a02290839f076c37200b026aabc261c54d06f82cc90cc8bc692178c03a7de59a',
        'can_read':'584a907976aa81d0f1758dc429acb257053acc33133e9ed5a659d43b533e3050',
        'read_u32':'99890d0423eabe27aec78dfa760a373708d97c2e60b45acba8128a6d4015bafc',
    }
    flags = re.findall(r'static constexpr bool katana_guarded_unknown_ram_reads = ([^;]+);', source)
    if not flags or any(flag != 'true' for flag in flags):
        raise RuntimeError('Preloaded reads require the retained direct-read allowance')
    for name, digest in expected.items():
        helpers = re.findall(r'const auto katana_direct_ram_'+name+r' =\n.*?^    };', source, re.M|re.S)
        if not helpers:
            raise RuntimeError('Preloaded read helper is missing: '+name)
        for helper in helpers:
            if name == 'read_u32':
                helper = helper.split('        }\n', 1)[0]+'        }\n'
            if sha(helper.encode()) != digest:
                raise RuntimeError('Preloaded read helper changed: '+name)


def operand_matches(opcode, destination, address):
    """Only nonincrementing MOV.L forms, with the opcode's exact GPR operands."""
    n, m = (opcode >> 8) & 15, (opcode >> 4) & 15
    if n != int(destination):
        return False
    reg = f"katana_registers[{m}]"
    if opcode & 0xF00F == 0x6002:
        return address == reg
    if opcode & 0xF000 == 0x5000:
        displacement = (opcode & 15) * 4
        return address == reg + (f" + {displacement}u" if displacement else "")
    if opcode & 0xF00F == 0x000E:
        return address == f"katana_registers[0] + {reg}"
    return False


def transform_scalar_fpu(source):
    edits, sites = [], []
    for start in FPU_START.finditer(source):
        indent, pc, address = start.group('indent', 'pc', 'address')
        end = re.compile(r'(?m)^'+re.escape(indent)+r'\}').search(source, start.end())
        if end is None:
            raise RuntimeError('Unterminated FPU instruction')
        block = source[start.start():end.end()]
        opcodes = list(OPCODE.finditer(block))
        if len(opcodes) != 1:
            continue
        opcode = opcodes[0][1]
        word = int(opcode, 16)
        n, m = (word >> 8) & 15, (word >> 4) & 15
        if word & 0xF00F not in (0xF008, 0xF006):
            continue
        expected_address = f'katana_registers[{m}]'
        if word & 15 == 6:
            expected_address = 'katana_registers[0] + '+expected_address
        if address != expected_address:
            continue
        expected = ENVELOPE.substitute(pc=pc, next_pc=f'{int(pc,16)+2:08X}',
            address=address, destination=n, allow='katana_guarded_unknown_ram_reads', opcode=opcode)
        plain_preflight = f'katana_direct_ram_can_read({address}, 4u)'
        pair_preflight = plain_preflight+f' && ((cpu.fpscr & katana::runtime::fpscr_sz_mask) == 0u || katana_direct_ram_can_read(({address}) + 4u, 4u))'
        expected = expected.replace(plain_preflight+';', pair_preflight+';')
        checks = f'''    if ((cpu.sr & katana::runtime::sr_fd_mask) != 0u) {{
        katana_registers.flush_release();
        raise_fpu_disabled(cpu, katana::runtime::relocate_code_address_inline(0x{pc}u));
        return;
    }}
    if (((cpu.fpscr & katana::runtime::fpscr_pr_mask) != 0u && (cpu.fpscr & katana::runtime::fpscr_sz_mask) != 0u)) {{
        katana_registers.flush_release();
        raise_illegal_instruction(cpu, katana::runtime::relocate_code_address_inline(0x{pc}u));
        return;
    }}
'''
        expected = expected.replace('    try {', checks+'    try {')
        expected = expected.replace('        enter_memory_exception_with_provenance',
            '        katana_registers.flush_release();\n        enter_memory_exception_with_provenance')
        expected = indent+expected.replace('\n', '\n'+indent)
        assignment = f'katana_registers[{n}] = katana_direct_ram_read_u32(guest_origin, {address}, katana_guarded_unknown_ram_reads);'
        # The pinned emitter leaves its embedded FPU instruction body unindented.
        expected = expected.replace(assignment, '{\n'+FPU_BODY.substitute(address=address,destination=n)+'\n}')
        if expected != block:
            continue
        token = f'sonic_preloaded_fpu_{pc}'
        eligible = f'sonic_preloaded_fpu_eligible_{pc}'
        before = f'const bool katana_guarded_linear_access_{pc} = {pair_preflight};'
        after = (f'const bool {eligible} = (cpu.sr & katana::runtime::sr_fd_mask) == 0u && '
            f'(cpu.fpscr & katana::runtime::fpscr_sz_mask) == 0u;\n{indent}    '
            f'const auto {token} = {eligible} ? sonic::memory::preload_read32('
            f'katana_direct_ram, {address}, katana_direct_ram_translate) : sonic::memory::PreloadedRead32{{}};\n{indent}    '
            f'const bool katana_guarded_linear_access_{pc} = {eligible} ? {token}.valid : ({pair_preflight});')
        read = 'katana_direct_ram_read_u32(guest_origin, address, katana_guarded_unknown_ram_reads)'
        scalar = f'cpu.fr[{n}] = {read};'
        consume = f'cpu.fr[{n}] = sonic::memory::consume_preloaded32({token}, [&] {{ return {read}; }});'
        changed = block.replace(before, after).replace(scalar, consume)
        if changed.replace(consume, scalar).replace(after, before) != block:
            raise RuntimeError('Scalar FPU read escaped its source spans')
        edits.append((start.start(), end.end(), changed))
        sites.append({'pc':pc, 'opcode':opcode, 'kind':'scalar-fmov',
            'source_line':source.count('\n',0,start.start())+1})
    output = source
    for begin, end, changed in reversed(edits):
        output = output[:begin]+changed+output[end:]
    return output, sites


def transform(source, mode="prepared", scalar_fpu=False):
    if mode == 'preloaded':
        validate_preloaded_helpers(source)
    edits, sites = [], []
    for start in START.finditer(source):
        indent, pc, address = start.group("indent", "pc", "address")
        end = re.compile(r"(?m)^" + re.escape(indent) + r"\}").search(source, start.end())
        if end is None:
            raise RuntimeError("Unterminated generated instruction")
        block = source[start.start():end.end()]
        loads, opcodes = list(LOAD.finditer(block)), list(OPCODE.finditer(block))
        if len(loads) != 1 or len(opcodes) != 1:
            continue
        destination, read_address, allow = loads[0].groups()
        opcode = opcodes[0][1]
        if address != read_address or not operand_matches(int(opcode, 16), destination, address):
            continue
        expected = ENVELOPE.substitute(pc=pc, next_pc=f"{int(pc, 16)+2:08X}",
            address=address, destination=destination, allow=allow, opcode=opcode)
        expected = indent + expected.replace("\n", "\n"+indent)
        if block != expected:
            continue
        before = f"const bool katana_guarded_linear_access_{pc} = katana_direct_ram_can_read({address}, 4u);"
        token = f"sonic_prepared_read_{pc}"
        after = (f"const auto {token} = sonic::memory::prepare_read32("
            f"katana_direct_ram, {address}, katana_direct_ram_translate);\n{indent}    "
            f"const bool katana_guarded_linear_access_{pc} = {token}.valid;")
        read = f"katana_direct_ram_read_u32(guest_origin, {address}, {allow})"
        consume = (f"sonic::memory::read_prepared32(katana_direct_ram, {token}, {address}, "
            f"[&] {{ return {read}; }})")
        if mode == 'preloaded':
            after = after.replace('prepare_read32(', 'preload_read32(')
            consume = f"sonic::memory::consume_preloaded32({token}, [&] {{ return {read}; }})"
        changed = block.replace(before, after).replace(read+";", consume+";")
        # Reverse just these two edits, proving the rest of the envelope intact.
        if changed.replace(consume+";", read+";").replace(after, before) != block:
            raise RuntimeError("Prepared read changes escaped their two source spans")
        edits.append((start.start(), end.end(), changed))
        sites.append({"pc":pc, "opcode":opcode, "source_line":source.count("\n", 0, start.start())+1})
    output = source
    for begin, end, changed in reversed(edits):
        output = output[:begin]+changed+output[end:]
    if scalar_fpu:
        if mode != 'preloaded':
            raise RuntimeError('Scalar FPU reads require preloaded mode')
        # Match against the untouched input so report line numbers stay stable.
        fpu_output, fpu_sites = transform_scalar_fpu(source)
        if fpu_sites:
            # MOV.L and FMOV envelopes cannot overlap. Reapply the GPR edit pass
            # to the FPU copy without recursively transforming FP instructions.
            output, _ = transform(fpu_output, mode, False)
            output = output.removeprefix('#include "sonic_prepared_read.hpp"\n')
            sites.extend(fpu_sites)
    return '#include "sonic_prepared_read.hpp"\n'+output, sites


def write_if_changed(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def prepare(args):
    root, destination = args.source_root.resolve(), args.destination.resolve()
    if root == destination or root in destination.parents or destination in root.parents:
        raise RuntimeError("Prepared output must be separate from the retained source tree")
    manifest = (root/".katana-generated-artifacts").read_text().splitlines()
    if manifest[0] != "katana-codegen-artifacts-v2" or manifest[1] != "generation\tsha256:"+sha(("\n".join(manifest[2:])+"\n").encode()):
        raise RuntimeError("Invalid generated artifact manifest")
    report = {"schema":"sarecomp-prepared-read-aot-v1", "mode":args.mode, "scalar_fpu":args.scalar_fpu,
        "manifest_generation":manifest[1], "helper_sha256":sha(args.helper.read_bytes()), "units":[]}
    selected = args.unit or list(UNITS)
    if len(selected) != len(set(selected)):
        raise RuntimeError("Duplicate selected AOT unit")
    for name in selected:
        if not re.fullmatch(r'unit-v[0-9A-F]+-[0-9A-F]+-[0-9a-f]+\.cpp', name):
            raise RuntimeError("Invalid selected AOT unit")
        expected = UNITS.get(name)
        data = (root/"code"/name).read_bytes()
        record = [line.split("\t") for line in manifest[2:] if line.split("\t")[0] == "code/"+name]
        if expected is None and len(record) == 1:
            expected = record[0][2].removeprefix('sha256:')
        if sha(data) != expected or len(record) != 1 or record[0][1:3] != [str(len(data)), "sha256:"+expected]:
            raise RuntimeError("Selected AOT source identity mismatch: "+name)
        source = data.decode()
        output, sites = transform(source, args.mode, args.scalar_fpu)
        # Both sampled witnesses must qualify; a changed layout fails closed.
        witness = "8C02947C" if name.startswith("unit-v8C029400") else "8C036BE0"
        if not sites or (name in UNITS and not any(site["pc"] == witness for site in sites)):
            raise RuntimeError("Required ordinary read no longer qualifies")
        if args.mode == "control":
            output = source
        encoded = output.encode()
        entries = re.findall(r"(?m)^BlockExit (fn_[0-9A-F]+_runtime_entry)\(CpuState& cpu, BlockExecutionContext& context\) \{", source)
        if not entries or len(entries) != len(set(entries)):
            raise RuntimeError("Unexpected generated entry definitions")
        report["units"].append({"name":name, "source_sha256":expected, "output_sha256":sha(encoded),
            "eligible_instructions":len(sites), "sites":sites, "entry_symbols":entries})
        write_if_changed(destination/name, encoded)
    if args.scalar_fpu and not any(site.get('kind') == 'scalar-fmov'
            for unit in report['units'] for site in unit['sites']):
        raise RuntimeError('Scalar FPU preparation found no admitted instruction')
    write_if_changed(destination/"preparation.json", (json.dumps(report, indent=2)+"\n").encode())
    print("SONIC_PREPARED_READ_READY mode="+args.mode+" units="+str(len(selected))+" instructions="+
        str(sum(unit["eligible_instructions"] for unit in report["units"]))+" retained_aot_unchanged=1")


def audit(args):
    report = json.loads(args.report.read_text())
    expected = {symbol:unit["name"].lower()+".obj" for unit in report["units"] for symbol in unit["entry_symbols"]}
    seen = {symbol:[] for symbol in expected}
    unwanted = []
    # The entry name also occurs inside hundreds of nested lambda symbols.
    # Admit only a public symbol whose token starts with the entry itself.
    entry_pattern = re.compile(r"^\s*[0-9A-Fa-f]+:[0-9A-Fa-f]+\s+\?(fn_[0-9A-F]+_runtime_entry)@katana_port_generated@@")
    members = tuple(unit["name"].lower()+".obj" for unit in report["units"])
    with args.map.open(errors="strict") as stream:
        for line in stream:
            if "_runtime_entry@katana_port_generated@@" in line:
                entry = entry_pattern.search(line)
                if entry and entry[1] in seen:
                    seen[entry[1]].append(line.split()[-1].lower())
            if "unit-v8" in line:
                lower = line.lower()
                if "katana_generated:" in lower and any(member in lower for member in members):
                    unwanted.append(line.strip())
    for symbol, owners in seen.items():
        if owners != ["sonic_ram_reads:"+expected[symbol]]:
            raise RuntimeError(f"Prepared member ownership mismatch: {symbol} definitions={len(owners)} owners={sorted(set(owners))}")
    if unwanted:
        raise RuntimeError("Original selected AOT member also linked")
    print(f"SONIC_PREPARED_READ_LINK_PASS mode={report['mode']} entries={len(seen)} original_selected_members=0")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    p = commands.add_parser("prepare")
    p.add_argument("--source-root", type=Path, required=True)
    p.add_argument("--destination", type=Path, required=True)
    p.add_argument("--helper", type=Path, required=True)
    p.add_argument("--mode", choices=("control", "prepared", "preloaded"), required=True)
    p.add_argument("--unit", action="append", help="Manifest-authenticated selected AOT member")
    p.add_argument("--scalar-fpu", action="store_true", help="Also preload admitted nonincrementing scalar FMOV reads")
    p.set_defaults(run=prepare)
    a = commands.add_parser("audit")
    a.add_argument("--map", type=Path, required=True)
    a.add_argument("--report", type=Path, required=True)
    a.set_defaults(run=audit)
    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
