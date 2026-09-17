"""Prove that selected Windows entries came from the prepared RAM archive."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--preparation', type=Path, required=True)
    parser.add_argument('--map', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    sha = lambda data: hashlib.sha256(data).hexdigest()
    manifest = (args.source_root / '.katana-generated-artifacts').read_text().splitlines()
    if (manifest[0] != 'katana-codegen-artifacts-v2' or
            manifest[1] != 'generation\tsha256:' + sha(('\n'.join(manifest[2:]) + '\n').encode())):
        raise ValueError('Retained generation changed')
    records = {parts[0]: parts for line in manifest[2:] if len(parts := line.split('\t')) >= 3}
    preparation = json.loads(args.preparation.read_text())
    if (preparation['schema'] != 'sarecomp-scalar-writes-v1' or
            preparation['mode'] != 'region' or preparation['generation'] != manifest[1] or
            preparation.get('guard_probe', False)):
        raise ValueError('Unqualified RAM preparation')
    expected = {}
    members = set()
    for unit in preparation['units']:
        name = unit['unit']
        if not re.fullmatch(r'unit-v[0-9A-F]+-[0-9A-F]+-[0-9a-f]+\.cpp', name):
            raise ValueError('Invalid selected member')
        original = (args.source_root / 'code' / name).read_bytes()
        prepared = (args.preparation.parent / name).read_bytes()
        if (records.get('code/' + name, [])[1:3] != [str(len(original)), 'sha256:' + sha(original)] or
                unit['source_sha256'] != sha(original) or unit['output_sha256'] != sha(prepared)):
            raise ValueError('RAM member identity changed: ' + name)
        symbols = re.findall(r'(?m)^BlockExit (fn_[0-9A-F]+_runtime_entry)\(CpuState& cpu, BlockExecutionContext& context\) \{', original.decode())
        if not symbols or any(symbol in expected for symbol in symbols):
            raise ValueError('Missing or duplicate source definitions')
        member = name.lower() + '.obj'
        if member in members:
            raise ValueError('Duplicate prepared member')
        members.add(member)
        expected.update((symbol, 'sonic_ram_regions:' + member) for symbol in symbols)
    seen = {symbol: [] for symbol in expected}
    entry_pattern = re.compile(r'^\s*[0-9A-Fa-f]+:[0-9A-Fa-f]+\s+\?(fn_[0-9A-F]+_runtime_entry)@katana_port_generated@@')
    with args.map.open(errors='strict') as stream:
        for line in stream:
            match = entry_pattern.search(line)
            if match and match[1] in seen:
                seen[match[1]].append(line.split()[-1].lower())
            if 'katana_generated:' in line:
                owner = line.rsplit(None, 1)[-1].lower()
                if owner.startswith('katana_generated:') and owner.partition(':')[2] in members:
                    raise ValueError('Retained selected member also linked')
    for symbol, owner in expected.items():
        if seen[symbol] != [owner]:
            raise ValueError(f'RAM entry ownership differs: {symbol}: {seen[symbol]}')
    report = dict(schema='sarecomp-ram-region-link-v1', entries=len(expected), units=len(members),
                  extended=preparation.get('extended_regions', False),
                  retained_selected_members=0, preparation_sha256=sha(args.preparation.read_bytes()))
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print('SONIC_RAM_REGION_LINK_PASS ' + json.dumps(report))


if __name__ == '__main__':
    main()
