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


def sha(data):
    return hashlib.sha256(data).hexdigest()


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


def transform(source):
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
        changed = block.replace(before, after).replace(read+";", consume+";")
        # Reverse just these two edits, proving the rest of the envelope intact.
        if changed.replace(consume+";", read+";").replace(after, before) != block:
            raise RuntimeError("Prepared read changes escaped their two source spans")
        edits.append((start.start(), end.end(), changed))
        sites.append({"pc":pc, "opcode":opcode, "source_line":source.count("\n", 0, start.start())+1})
    output = source
    for begin, end, changed in reversed(edits):
        output = output[:begin]+changed+output[end:]
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
    report = {"schema":"sarecomp-prepared-read-aot-v1", "mode":args.mode,
        "manifest_generation":manifest[1], "helper_sha256":sha(args.helper.read_bytes()), "units":[]}
    for name, expected in UNITS.items():
        data = (root/"code"/name).read_bytes()
        record = [line.split("\t") for line in manifest[2:] if line.split("\t")[0] == "code/"+name]
        if sha(data) != expected or len(record) != 1 or record[0][1:3] != [str(len(data)), "sha256:"+expected]:
            raise RuntimeError("Selected AOT source identity mismatch: "+name)
        source = data.decode()
        output, sites = transform(source)
        # Both sampled witnesses must qualify; a changed layout fails closed.
        witness = "8C02947C" if name.startswith("unit-v8C029400") else "8C036BE0"
        if not any(site["pc"] == witness for site in sites):
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
    write_if_changed(destination/"preparation.json", (json.dumps(report, indent=2)+"\n").encode())
    print("SONIC_PREPARED_READ_READY mode="+args.mode+" units=2 instructions="+
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
    p.add_argument("--mode", choices=("control", "prepared"), required=True)
    p.set_defaults(run=prepare)
    a = commands.add_parser("audit")
    a.add_argument("--map", type=Path, required=True)
    a.add_argument("--report", type=Path, required=True)
    a.set_defaults(run=audit)
    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
