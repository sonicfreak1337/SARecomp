"""Inject three authenticated public entries; retain all resume entries."""
import argparse
import hashlib
import json
from pathlib import Path
import re

def sha(data):
    return hashlib.sha256(data).hexdigest()

OWNERS = {
    'unit-v8C09036C-8C09168E-46c0db15f975e3ef.cpp': (
        ('8C09105A', '6666f74bee9fae425ca188a8f3bd2137854e1d576720653dd4cecf7826258ef6'),
        ('8C0912C0', '3c16088469ca56429d1e2ddbcd9007c62ff47d778e9df44a2051438e0f661ce1')),
    'unit-v8C091720-8C09276E-f12ea02f8b924edc.cpp': (
        ('8C091928', '3dd08bbbe8a22fab0bbe4f837d33f06355c03e40ff82bb37d202c2ad8967a635'),)
}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root', type=Path, required=True)
    p.add_argument('--input', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    original = (a.source_root / 'code' / a.input.name).read_bytes()
    lines = (a.source_root / '.katana-generated-artifacts').read_text().splitlines()
    generation = 'generation\tsha256:' + sha(('\n'.join(lines[2:]) + '\n').encode())
    if lines[:2] != ['katana-codegen-artifacts-v2', generation]:
        raise ValueError('Retained manifest identity')
    records = {r[0]: r for line in lines[2:] if len(r := line.split('\t')) >= 3}
    if records.get('code/' + a.input.name, [])[1:3] != [str(len(original)), 'sha256:' + sha(original)]:
        raise ValueError('Retained unit identity')
    data = a.input.read_bytes()
    if data != original:
        if a.input.parent.name not in ('region-writes', 'region-extended-writes'):
            raise ValueError('Unreviewed input preparation')
        prior = json.loads((a.input.parent / 'preparation.json').read_text())
        entries = [e for e in prior['units'] if e['unit'] == a.input.name]
        if (prior['generation'] != generation or prior['mode'] != 'region' or prior.get('guard_probe', False) or
                prior.get('extended_regions', False) != (a.input.parent.name == 'region-extended-writes') or
                len(entries) != 1 or entries[0]['source_sha256'] != sha(original) or
                entries[0]['output_sha256'] != sha(data)):
            raise ValueError('RAM region provenance')
    old = original.decode().replace('\r\n', '\n')
    text = data.decode().replace('\r\n', '\n')
    for owner, expected in OWNERS[a.input.name]:
        pattern = r'(?m)^BlockExit fn_' + owner + r'_runtime_entry\(CpuState& cpu, BlockExecutionContext& context\) \{\n'
        start = re.search(pattern, old).start()
        end = old.index('\nBlockExit fn_', start + 1) + 1
        if sha(old[start:end].encode()) != expected:
            raise ValueError('Owner audit identity')
        start = re.search(pattern, text).start()
        boundary = text.index('    static_cast<void>(services);', start)
        injection = '''    if (sonic::object_activation::enabled() &&
        sonic::object_activation::try_dispatch(cpu, *services)) {
        const auto source = sonic::object_activation::return_site;
        runtime_dispatch_detail::active_exit_source = {source, source & 0x1FFFFFFFu};
        runtime_dispatch_detail::active_exit_kind = katana::runtime::BlockEndKind::Return;
        runtime_dispatch_detail::active_exit_site_class = katana::runtime::DynamicDispatchSiteClass::NotDynamic;
        return;
    }
'''
        text = text[:boundary] + injection + text[boundary:]
    text = '#include "sonic_object_activation.hpp"\n' + text.replace('#include "../include/', '#include "')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    out = text.encode()
    if not a.output.exists() or a.output.read_bytes() != out:
        a.output.write_bytes(out)
    a.output.with_suffix('.json').write_text(json.dumps({
        'owners': [row[0] for row in OWNERS[a.input.name]], 'generation': generation,
        'input_sha256': sha(data), 'original_sha256': sha(original), 'output_sha256': sha(out)
    }, indent=2) + '\n')
    print('SONIC_OBJECT_ACTIVATION_BRIDGE_READY unit=' + a.input.name + ' retained_fallback=1')

if __name__ == '__main__':
    main()
