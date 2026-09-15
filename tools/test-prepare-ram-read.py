"""Fail-closed qualification tests using an actual retained instruction."""
import importlib.util
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ram_prepare", ROOT/"tools/prepare-ram-read-aot.py")
recipe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recipe)
generated = ROOT/".local/working-product/generated"
source = (generated/"code"/next(iter(recipe.UNITS))).read_text()
start = next(m for m in recipe.START.finditer(source) if m["pc"] == "8C02947C")
end = source.index("\n"+start["indent"]+"}", start.end())+len(start["indent"])+2
original = source[start.start():end]
changed, sites = recipe.transform(original)
assert len(sites) == 1 and changed.count("read_prepared32(") == 1
assert "return katana_direct_ram_read_u32(guest_origin, katana_registers[3], katana_guarded_unknown_ram_reads);" in changed

rejections = [
    ("try {", "cpu.mmucr = 1u;\n"+start["indent"]+"    try {"),
    ("guest_instruction_attempt.complete();", "katana_registers[3] += 4u;\n"+start["indent"]+"        guest_instruction_attempt.complete();"),
    ("katana_guarded_unknown_ram_reads);", "false);"),
    ("guest_origin, katana_registers[3],", "guest_origin, katana_registers[5],"),
    ("katana_instruction_runtime_pc, 2u);", "katana_instruction_runtime_pc, 1u);"),
    ("0x00006432u);", "0x00006436u);"),
    ("0x00006432u);", "0x00006432u, 0x8C02947Au);"),
    ("0x8C02947Eu);", "0x8C029480u);"),
    ("guest_instruction_attempt.complete();", "services->consume_guest_cycles(1u, 0u);"),
]
for before, after in rejections:
    assert original.count(before) == 1, before
    mutant = original.replace(before, after)
    result, accepted = recipe.transform(mutant)
    assert not accepted and result.endswith(mutant), before

for name, count in zip(recipe.UNITS, (154, 115), strict=True):
    data = (generated/"code"/name).read_bytes()
    assert recipe.sha(data) == recipe.UNITS[name]
    _, accepted = recipe.transform(data.decode())
    assert len(accepted) == count

# The Linux preload experiment additionally authenticates the callback-free
# helper bodies and admits scalar FMOV only through its exact original envelope.
recipe.validate_preloaded_helpers(source)
for before, after in (
    ('katana_guarded_unknown_ram_reads = true;', 'katana_guarded_unknown_ram_reads = false;'),
    ('const auto katana_direct_ram_resolve =\n', 'const auto katana_direct_ram_resolve =\n        /* changed helper */\n'),
):
    assert before in source
    try:
        recipe.validate_preloaded_helpers(source.replace(before, after))
        raise AssertionError('Changed preload helper admitted')
    except RuntimeError:
        pass
fp_start = next(m for m in recipe.FPU_START.finditer(source) if m['pc'] == '8C02943A')
fp_end = source.index('\n'+fp_start['indent']+'}', fp_start.end())+len(fp_start['indent'])+2
fp_original = source[fp_start.start():fp_end]
fp_changed, fp_sites = recipe.transform_scalar_fpu(fp_original)
assert len(fp_sites) == 1 and 'consume_preloaded32(' in fp_changed
for before, after in (
    ('0x0000F3E8u);', '0x0000F3E9u);'),
    ('cpu.fr[3] =', 'cpu.fr[4] ='),
    ('0x0000F3E8u);', '0x0000F3E8u, 0x8C029438u);'),
):
    assert fp_original.count(before) == 1
    mutant = fp_original.replace(before, after)
    result, admitted = recipe.transform_scalar_fpu(mutant)
    assert not admitted and result == mutant
_, preloaded_sites = recipe.transform(source, 'preloaded', True)
assert len(preloaded_sites) == 363
assert sum(site.get('kind') == 'scalar-fmov' for site in preloaded_sites) == 209

# Verify a modified input cannot pass the bound preparation, and a linked
# candidate cannot masquerade as the old retained archive or duplicate entry.
scratch = ROOT/".local/analysis/prepared-read"
scratch.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="qualification-", dir=scratch) as directory:
    temp = Path(directory).resolve()
    assert scratch.resolve() in temp.parents
    fake = temp/"source"
    (fake/"code").mkdir(parents=True)
    (fake/".katana-generated-artifacts").write_bytes((generated/".katana-generated-artifacts").read_bytes())
    name = next(iter(recipe.UNITS))
    (fake/"code"/name).write_text(source+"\n// changed\n")
    try:
        recipe.prepare(SimpleNamespace(source_root=fake, destination=temp/"output",
            helper=ROOT/"src/sonic_prepared_read.hpp", mode="prepared", scalar_fpu=False, unit=None))
        raise AssertionError("Modified retained input accepted")
    except RuntimeError as error:
        assert "identity mismatch" in str(error)
    assert not (temp/"output").exists()
    report = temp/"preparation.json"
    symbol = "fn_8C029400_runtime_entry"
    report.write_text(json.dumps({"mode":"prepared", "units":[{"name":name, "entry_symbols":[symbol]}]}))
    map_path = temp/"link.map"
    line = f"0001:00000000 ?{symbol}@katana_port_generated@@ 0000000140001000 f sonic_ram_reads:{name}.obj\n"
    map_path.write_text(line)
    recipe.audit(SimpleNamespace(map=map_path, report=report))
    map_path.write_text(line+line.replace("?"+symbol, "??lambda@?0??"+symbol))
    recipe.audit(SimpleNamespace(map=map_path, report=report))
    for invalid in (line.replace("sonic_ram_reads:", "katana_generated:"), line+line, ""):
        map_path.write_text(invalid)
        try:
            recipe.audit(SimpleNamespace(map=map_path, report=report))
            raise AssertionError("Missing, duplicate or retained owner accepted")
        except RuntimeError as error:
            assert "ownership mismatch" in str(error)

print("SONIC_PREPARED_READ_QUALIFICATION_OK eligible=269 rejected_envelopes=9 changed_source_rejected=1 rejected_link_owners=3")
print("SONIC_PRELOADED_READ_QUALIFICATION_OK witness_gpr=154 witness_fmov=209 rejected_helpers=2 rejected_fmov_envelopes=3")
