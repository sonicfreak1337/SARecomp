"""Extract authenticated original and extended RAM-region game witnesses."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--source', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
data = a.source.read_bytes()
manifest = (a.source.parent.parent / '.katana-generated-artifacts').read_text().splitlines()
record = [line.split('\t') for line in manifest if line.startswith('code/' + a.source.name + '\t')]
if len(record) != 1 or record[0][1:3] != [str(len(data)), 'sha256:' + hashlib.sha256(data).hexdigest()]:
    raise ValueError('Extended original witness identity changed')
spec = importlib.util.spec_from_file_location('regions', Path(__file__).with_name('prepare-ram-regions.py'))
r = importlib.util.module_from_spec(spec)
spec.loader.exec_module(r)
source = data.decode()
changed, report = r.transform(source, extended=True)
nodes = [n for atom in r.instructions(source, True) for n in atom]
result = 'static unsigned extended_completed[7]{}, extended_partial[7]{};\n'
records = []
for index, (first, last, size) in enumerate((
        (0x8C01995E, 0x8C01996E, 9), (0x8C0199B2, 0x8C0199CC, 14),
        (0x8C01A40E, 0x8C01A41C, 8), (0x8C01A702, 0x8C01A712, 9),
        (0x8C01A18C, 0x8C01A1A0, 11), (0x8C01A240, 0x8C01A276, 28),
        (0x8C01A480, 0x8C01A4C8, 37))):
    if not any(g['pc'] == f'{first:08X}' and g['last_pc'] == f'{last:08X}' and g['instructions'] == size for g in report):
        raise ValueError('Extended witness interval changed')
    selected = [n for n in nodes if first <= n['pc'] <= last]
    original = source[selected[0]['start']:selected[-1]['end']]
    label_match = re.search(f'sonic_ram_{first:08X}_(\\d+)_\\d+:', changed)
    key, label = label_match[1], label_match.start()
    begin = changed.rfind(selected[0]['indent'] + '{\n' + selected[0]['indent'] + '    if (sonic::scalar_writes', 0, label)
    if begin < 0:
        raise ValueError('Missing generated prefix')
    end_label = f'sonic_ram_{first:08X}_{key}_end: ;'
    candidate = changed[begin:changed.index(end_label, label) + len(end_label)]
    # Fixture-only evidence that comparisons actually execute the optimized
    # prefix and its partial-commit path, rather than only its fallback.
    marker = '        switch (stopped) {'
    if candidate.count(marker) != 1:
        raise ValueError('Unexpected prefix completion')
    candidate = candidate.replace(marker,
        f'        if (stopped == {len(selected)}u) ++extended_completed[{index}];\n'
        f'        else if (stopped != 0u) ++extended_partial[{index}];\n' + marker)
    owner = source.rfind('\nBlockExit fn_', 0, selected[0]['start'])
    helper_start = source.index('    const auto katana_direct_ram_translate =', owner)
    helper_end = source.index('    if (katana::runtime::unrelocate_code_address_inline(cpu.pc) ==', helper_start)
    helpers = source[helper_start:helper_end]
    resumes = sorted(set(re.findall(r'katana_block_([0-9A-F]{8})_resume:', original)))
    resume_code = '\n'.join(f'    if (resume == 0x{pc}u) goto katana_block_{pc}_resume;' for pc in resumes)
    for name, body in [('original', original), ('extended', candidate)]:
        result += f'void {name}_witness_{index}(Owned& o,const DirectLinearMemoryGuard& entry,std::uint32_t resume) {{\n'
        result += '''    auto& cpu=o.f.cpu;
    auto* services=&o.f.services;
    NativeAotRegisterFile<0xFFFFu,0x3Fu> katana_registers(cpu);
    bool katana_guest_write_exit_requested=false;
    const auto* katana_direct_ram_code_tracker=&o.guard;
    auto katana_direct_ram=entry;
    Memory::DirectLinearWriteBatch* const katana_direct_ram_writes=nullptr;
''' + helpers + resume_code + '\n' + body + '\n}\n'
    records.append({'first': first, 'last': last, 'size': size, 'resumes': resumes})
result += '''struct ExtendedWitness {
    void (*original)(Owned&,const DirectLinearMemoryGuard&,std::uint32_t);
    void (*extended)(Owned&,const DirectLinearMemoryGuard&,std::uint32_t);
    std::uint32_t first;
    std::array<std::uint32_t,40> resumes;
};
static constexpr ExtendedWitness extended_witnesses[] = {
'''
for index, entry in enumerate(records):
    result += f'    {{original_witness_{index},extended_witness_{index},0x{entry["first"]:08X}u,{{'
    result += ','.join('0x' + pc + 'u' for pc in entry['resumes']) + '}},\n'
result += '};\n'
a.output.parent.mkdir(parents=True, exist_ok=True)
if not a.output.exists() or a.output.read_text() != result:
    a.output.write_text(result)
a.output.with_suffix('.json').write_text(json.dumps({'source_sha256': hashlib.sha256(data).hexdigest(),
    'witnesses': records}, indent=2) + '\n')
print('SONIC_RAM_EXTENDED_WITNESSES_READY instructions=116 witnesses=7')
